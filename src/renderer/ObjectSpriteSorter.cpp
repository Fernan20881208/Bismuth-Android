#include "ObjectSpriteSorter.hpp"
#include "Renderer.hpp"

void SortedObjectBatch::bindBuffers() {
    spriteRemapBuffer->bindAsStorageBuffer(SPRITE_REMAP_BUFFER_BINDING);
    workGroupBuffer->bindAsStorageBuffer(WORK_GROUP_BUFFER_BINDING);
    drawCommandBuffer->bindAsStorageBuffer(DRAW_COMMAND_BUFFER_BINDING);
    preCulledSpritesBuffer->bind();
}

void ObjectSpriteSorter::addSprite(
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
        .object = object
    });
}

void ObjectSpriteSorter::sortForCurrentBlendingStates() {
    assert(!spriteEntries.empty());

    sortedSpriteLayers.clear();

    for (auto& sprite : spriteEntries) {
        auto key = sprite.getFullLayerKey();
        sortedSpriteLayers[key].push_back(&sprite);
    }

    for (auto& [key, layer] : sortedSpriteLayers) {
        std::sort(layer.begin(), layer.end(), [](const SpriteEntry* a, const SpriteEntry* b) {
            return a->zOrder < b->zOrder;
        });
    }
}

UPtr<SortedObjectBatch> ObjectSpriteSorter::generateSortedBatch() {
    std::vector<u32> spriteRemap;
    spriteRemap.reserve(spriteEntries.size());

    u32 approxWorkGroupCount = std::min<u32>(spriteEntries.size(), Renderer::get()->getWorkGroupCount());
    u32 approxSpritesPerWorkGroup = spriteEntries.size() / approxWorkGroupCount;

    std::vector<WorkGroup> workGroups;
    workGroups.reserve(approxWorkGroupCount + 100);

    u32 outIndex = 0;
    std::map<LayerKey, WorkGroupRange> workGroupRangesPerLayerKey;

    preCulledSprites = PreCulledSprite::Buffer::create(spriteEntries.size(), GL_STATIC_DRAW);
    currentSpriteIndex = 0;

    for (auto& [key, layer] : sortedSpriteLayers) {
        // log::info("Process layer: {}", key.toString());
        u32 workGroupStartIndex = workGroups.size();
        generateWorkGroupsForLayer(key, approxSpritesPerWorkGroup, outIndex, spriteRemap, workGroups);
        u32 workGroupEndIndex = workGroups.size();

        workGroupRangesPerLayerKey[key] = {
            .offset = workGroupStartIndex,
            .count  = workGroupEndIndex - workGroupStartIndex
        };

        // log::info("Layer processed {}: (index={}, size={})", key.toString(), workGroupStartIndex, workGroupEndIndex - workGroupStartIndex);
    }

    log::info("Approx sprites per work group: {}", approxSpritesPerWorkGroup);
    log::info("Work group count: {}", workGroups.size());

    auto spriteRemapBuffer = Buffer::createStaticDraw(
        "SpriteRemapBuffer",
        spriteRemap.data(),
        spriteRemap.size() * sizeof(u32)
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

    std::vector<DrawCommand> drawCommands;
    drawCommands.resize(workGroups.size());

    for (usize i = 0; i < workGroups.size(); i++) {
        drawCommands[i].baseInstance  = 0;
        drawCommands[i].instanceCount = 1;
        drawCommands[i].first = workGroups[i].culledIndexBufferOutputIndex * 3;
    }

    auto drawCommandBuffer = Buffer::createGPUExclusive(
        "DrawCommandBuffer",
        workGroups.size() * sizeof(DrawCommand)
    );

    if (!drawCommandBuffer)
        return nullptr;

    drawCommandBuffer->write(drawCommands.data(), drawCommandBuffer->getSize());

    preCulledSprites->flushAllAndUnmap();

    return std::make_unique<SortedObjectBatch>(
        std::move(spriteRemapBuffer),
        std::move(workGroupBuffer),
        std::move(drawCommandBuffer),
        std::move(preCulledSprites),
        workGroups.size(),
        workGroupRangesPerLayerKey
    );
}

void ObjectSpriteSorter::generateWorkGroupsForLayer(
    LayerKey layerKey,
    u32 spritesPerWorkGroup,
    u32& culledIndexBufferOutputIndex,
    std::vector<u32>& inputSpriteIndiciesOut,
    std::vector<WorkGroup>& workGroupsOut
) {
    assert(spritesPerWorkGroup > 0);

    const auto& sprites = sortedSpriteLayers[layerKey];

    auto& gman = Renderer::get()->getGroupManager();

    u32 i = 0;
    while (i < sprites.size()) {
        u32 spritesToAdd = spritesPerWorkGroup;

        u32 culledOutputIndex = culledIndexBufferOutputIndex;
        u32 startIndex = inputSpriteIndiciesOut.size();

        while (spritesToAdd > 0 && i < sprites.size()) {
            auto sprite = sprites[i];
            inputSpriteIndiciesOut.push_back(sprite->inputSpriteIndex);
            culledIndexBufferOutputIndex += sprite->triCount;

            (*preCulledSprites)[currentSpriteIndex].groupCombinationIndex = gman.getGroupCombinationIndexForObject(sprite->object);
            (*preCulledSprites)[currentSpriteIndex].startPosition = ccPointToGLM(sprite->object->m_startPosition);

            currentSpriteIndex++;
            spritesToAdd--;
            i++;
        }

        u32 endIndex = inputSpriteIndiciesOut.size();

        // log::info("  - Work group {} = (index={}, count={}), {}", workGroupsOut.size(), startIndex, endIndex - startIndex, culledOutputIndex);

        workGroupsOut.push_back({
            .inputSpriteStart = startIndex,
            .inputSpriteEnd   = endIndex,
            .culledIndexBufferOutputIndex = culledOutputIndex
        });
    }
}

LayerKey ObjectSpriteSorter::SpriteEntry::getFullLayerKey() const {
    return { zLayer, spriteSheet, Renderer::get()->isColorChannelBlending(colorChannelId) };
}