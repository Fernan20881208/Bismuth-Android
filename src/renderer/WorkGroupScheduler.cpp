#include "WorkGroupScheduler.hpp"
#include "Renderer.hpp"
#include "../cl.hpp"

void SortedObjectBatch::bindBuffers() {
    if (!haveBuffersBeenCreated) {
        spriteRemapBuffer = Buffer::createStaticDraw(
            "SpriteRemapBuffer",
            inputSpriteIndicies.data(),
            inputSpriteIndicies.size() * sizeof(u32)
        );

        if (!spriteRemapBuffer)
            return;

        workGroupBuffer = Buffer::createStaticDraw(
            "WorkGroupBuffer",
            workGroups.data(),
            workGroups.size() * sizeof(WorkGroup)
        );

        if (!workGroupBuffer)
            return;

        drawCommandBuffer = Buffer::createGPUExclusive(
            "DrawCommandBuffer",
            workGroups.size() * sizeof(DrawCommand)
        );

        if (!drawCommandBuffer)
            return; // FIXME: Make sure that this shows an error and exits the level

        drawCommandBuffer->write(drawCommands.data(), drawCommandBuffer->getSize());

        preCulledSpritesBuffer->flushAllAndUnmap();

        inputSpriteIndicies.clear();
        workGroups.clear();
        drawCommands.clear();
        haveBuffersBeenCreated = true;
    }

    spriteRemapBuffer->bindAsStorageBuffer(SPRITE_REMAP_BUFFER_BINDING);
    workGroupBuffer->bindAsStorageBuffer(WORK_GROUP_BUFFER_BINDING);
    drawCommandBuffer->bindAsStorageBuffer(DRAW_COMMAND_BUFFER_BINDING);
    preCulledSpritesBuffer->bind();
}

WorkGroupScheduler::~WorkGroupScheduler() {
    if (thread.joinable())
        thread.join();
}

void WorkGroupScheduler::addSprite(
    GameObject* object,
    cocos2d::CCSprite* sprite,
    SpriteType type,
    u32 inputSpriteIndex,
    u32 triCount
) {
    spriteEntries.push_back({
        .zOrder = object->getObjectZOrder(),
        .zLayer = object->getObjectZLayer(),
        .spriteSheet = ObjectUtils::getSpritesheetOfObject(object, type),
        .colorChannelId = (u16)ObjectUtils::getSpriteColorChannel(type, object, sprite),
        .inputSpriteIndex = inputSpriteIndex,
        .triCount = triCount,
        .groupCombinationIndex = Renderer::get()->getGroupManager().getGroupCombinationIndexForObject(object),
        .object = object
    });
}

SortedObjectBatch* WorkGroupScheduler::fetchSortedBatchForBlendStates(const ColorChannelBlendStates& blendStates) {
    std::scoped_lock (mutex);

    if (maxWorkGroupCount == -1) {
        maxWorkGroupCount = getGPUCoreCount();
        if (maxWorkGroupCount == -1)
            maxWorkGroupCount = 8000; // dummy value
        else
            maxWorkGroupCount = (double)maxWorkGroupCount * WORK_GROUPS_PER_GPU_CORES;
    }
    
    if (sortedBatchesPerBlendStates.contains(blendStates))
        return sortedBatchesPerBlendStates[blendStates].get();

    auto sortedBatch = std::make_unique<SortedObjectBatch>();
    sortedBatch->preCulledSpritesBuffer = PreCulledSprite::Buffer::create(spriteEntries.size(), GL_STATIC_DRAW);

    generateJobs.emplace(GenerateJob {blendStates, std::move(sortedBatch)});

    if (!currentlyExecutingJobs.load()) {
        if (thread.joinable())
            thread.join();

        currentlyExecutingJobs.store(true);
        thread = std::thread(&WorkGroupScheduler::executeGenerateJobs, this);
        //executeGenerateJobs();
    }

    return nullptr;
}

void WorkGroupScheduler::executeGenerateJobs() {
    while (1) {
        GenerateJob job;

        {
            std::scoped_lock (mutex);
            if (generateJobs.empty())
                break;
            job = std::move(generateJobs.front());
            generateJobs.pop(); 
        }

        auto sortedBatch = generateSortedBatchForBlendStates(job.blendStates, std::move(job.sortedBatch));

        {
            std::scoped_lock (mutex);
            sortedBatchesPerBlendStates[job.blendStates] = std::move(sortedBatch);
        }
    }

    currentlyExecutingJobs.store(false);
}

void WorkGroupScheduler::sortForBlendStates(const ColorChannelBlendStates& blendStates) {
    assert(!spriteEntries.empty());

    sortedSpriteLayers.clear();

    for (auto& sprite : spriteEntries) {
        auto key = sprite.getFullLayerKey(blendStates);
        sortedSpriteLayers[key].push_back(&sprite);
    }

    for (auto& [key, layer] : sortedSpriteLayers) {
        std::sort(layer.begin(), layer.end(), [](const SpriteEntry* a, const SpriteEntry* b) {
            return a->zOrder < b->zOrder;
        });
    }
}

UPtr<SortedObjectBatch> WorkGroupScheduler::generateSortedBatchForBlendStates(
    ColorChannelBlendStates blendStates,
    UPtr<SortedObjectBatch> sortedBatch
) {
    sortForBlendStates(blendStates);

    inputSpriteIndicies.clear();
    inputSpriteIndicies.reserve(spriteEntries.size());

    u32 approxWorkGroupCount = std::min<u32>(spriteEntries.size(), maxWorkGroupCount);
    u32 approxSpritesPerWorkGroup = spriteEntries.size() / approxWorkGroupCount;

    workGroups.clear();
    workGroups.reserve(approxWorkGroupCount + 100);

    currentSpriteIndex = 0;

    u32 outIndex = 0;
    std::map<LayerKey, WorkGroupRange> workGroupRangePerLayerKey;

    log::info("Generating new SortedObjectBatch for (new) channel color blending states...");

    for (auto& [key, layer] : sortedSpriteLayers) {
        // log::info("Process layer: {}", key.toString());
        u32 workGroupStartIndex = workGroups.size();
        generateWorkGroupsForLayer(key, approxSpritesPerWorkGroup, outIndex, sortedBatch->preCulledSpritesBuffer.get());
        u32 workGroupEndIndex = workGroups.size();

        workGroupRangePerLayerKey[key] = {
            .offset = workGroupStartIndex,
            .count  = workGroupEndIndex - workGroupStartIndex
        };

        // log::info("Layer processed {}: (index={}, size={})", key.toString(), workGroupStartIndex, workGroupEndIndex - workGroupStartIndex);
    }

    std::vector<DrawCommand> drawCommands;
    drawCommands.resize(workGroups.size());

    for (usize i = 0; i < workGroups.size(); i++) {
        drawCommands[i].baseInstance  = 0;
        drawCommands[i].instanceCount = 1;
        drawCommands[i].first = workGroups[i].culledIndexBufferOutputIndex * 3;
    }

    usize totalSize = inputSpriteIndicies.size() * sizeof(u32) +
                      workGroups.size() * sizeof(WorkGroup) +
                      drawCommands.size() * sizeof(DrawCommand) +
                      sortedBatch->preCulledSpritesBuffer->getByteSize();

    log::info("  - Work group count: {}", workGroups.size());
    log::info("  - SortedObjectBatch VRAM usage: {}", byteSizeToString(totalSize));

    sortedBatch->workGroupCount = (u32)workGroups.size();
    sortedBatch->workGroupRangePerLayerKey = std::move(workGroupRangePerLayerKey);
    sortedBatch->haveBuffersBeenCreated = false; // Buffers can only be created on the main thread!
    sortedBatch->inputSpriteIndicies = std::move(inputSpriteIndicies);
    sortedBatch->workGroups = std::move(workGroups);
    sortedBatch->drawCommands = std::move(drawCommands);

    /*
    auto spriteRemapBuffer = Buffer::createStaticDraw(
        "SpriteRemapBuffer",
        inputSpriteIndicies.data(),
        inputSpriteIndicies.size() * sizeof(u32)
    );

    if (!spriteRemapBuffer)
        return nullptr;

    auto workGroupBuffer = Buffer::createStaticDraw(
        "WorkGroupBuffer",
        workGroups.data(),
        workGroups.size() * sizeof(WorkGroup)
    );

    if (!workGroupBuffer)
        return nullptr;

    // auto preCulledSpritesBuffer = Buffer::createStaticDraw("Pre-culled sprites buffer", preCulledSprites.data(), preCulledSprites.size() * sizeof(PreCulledSprite));
    // preCulledSprites.clear();

    // if (!preCulledSpritesBuffer)
    //     return nullptr;

    auto drawCommandBuffer = Buffer::createGPUExclusive(
        "DrawCommandBuffer",
        workGroups.size() * sizeof(DrawCommand)
    );

    if (!drawCommandBuffer)
        return nullptr;

    drawCommandBuffer->write(drawCommands.data(), drawCommandBuffer->getSize());

    preCulledSprites->flushAllAndUnmap();
    */

    return sortedBatch;
}

void WorkGroupScheduler::generateWorkGroupsForLayer(
    LayerKey layerKey,
    u32 spritesPerWorkGroup,
    u32& culledIndexBufferOutputIndex,
    PreCulledSprite::Buffer* preCulledSprites
) {
    assert(spritesPerWorkGroup > 0);

    const auto& sprites = sortedSpriteLayers[layerKey];

    u32 i = 0;
    while (i < sprites.size()) {
        u32 spritesToAdd = spritesPerWorkGroup;

        u32 culledOutputIndex = culledIndexBufferOutputIndex;
        u32 startIndex = inputSpriteIndicies.size();

        while (spritesToAdd > 0 && i < sprites.size()) {
            auto sprite = sprites[i];
            inputSpriteIndicies.push_back(sprite->inputSpriteIndex);
            culledIndexBufferOutputIndex += sprite->triCount;

            (*preCulledSprites)[currentSpriteIndex].groupCombinationIndex = sprite->groupCombinationIndex;
            (*preCulledSprites)[currentSpriteIndex].startPosition = ccPointToGLM(sprite->object->m_startPosition);

            currentSpriteIndex++;
            spritesToAdd--;
            i++;
        }

        u32 endIndex = inputSpriteIndicies.size();

        // log::info("  - Work group {} = (index={}, count={}), {}", workGroups.size(), startIndex, endIndex - startIndex, culledOutputIndex);

        workGroups.push_back({
            .inputSpriteStart = startIndex,
            .inputSpriteEnd   = endIndex,
            .culledIndexBufferOutputIndex = culledOutputIndex
        });
    }
}

LayerKey WorkGroupScheduler::SpriteEntry::getFullLayerKey(const ColorChannelBlendStates& blendingStates) const {
    return { zLayer, spriteSheet, blendingStates[colorChannelId] };
}