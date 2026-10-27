#pragma once

#include <common.hpp>
#include <ObjectUtils.hpp>
#include "Buffer.hpp"
#include <map>
#include <queue>

#define WORK_GROUPS_PER_GPU_CORES 2.5

class Renderer;

using ColorChannelBlendStates = std::array<bool, 1101>;

struct WorkGroupRange {
    u32 offset, count;
};

struct SortedObjectBatch {
    u32 workGroupCount;
    std::map<LayerKey, WorkGroupRange> workGroupRangePerLayerKey;

    //// BUFFERS ////
    bool haveBuffersBeenCreated = false;
    UPtr<Buffer> spriteRemapBuffer;
    UPtr<Buffer> workGroupBuffer;
    UPtr<Buffer> drawCommandBuffer;
    UPtr<PreCulledSprite::Buffer> preCulledSpritesBuffer;

    //// BUFFER DATAS ////
    std::vector<u32> inputSpriteIndicies;
    std::vector<WorkGroup> workGroups;
    std::vector<DrawCommand> drawCommands;

    void bindBuffers();
};

class WorkGroupScheduler {
public:
    ~WorkGroupScheduler();

    void addSprite(
        GameObject* object,
        cocos2d::CCSprite* sprite,
        SpriteType type,
        u32 inputSpriteIndex,
        u32 triCount
    );

    inline usize getSpriteCount() const {
        return spriteEntries.size();
    }

    SortedObjectBatch* fetchSortedBatchForBlendStates(const ColorChannelBlendStates& blendStates);

    inline void waitForJobs() {
        if (thread.joinable())
            thread.join();
    }

private:
    void executeGenerateJobs();

    void sortForBlendStates(const ColorChannelBlendStates& blendStates);

    UPtr<SortedObjectBatch> generateSortedBatchForBlendStates(
        ColorChannelBlendStates blendStates,
        UPtr<SortedObjectBatch> sortedBatch
    );

    void generateWorkGroupsForLayer(
        LayerKey layerKey,
        u32 spritesPerWorkGroup,
        u32& culledIndexBufferOutputIndex,
        PreCulledSprite::Buffer* preCulledSprites
    );

private:
    struct SpriteEntry {
        i32 zOrder;
        ZLayer zLayer;
        SpriteSheet spriteSheet;
        u16 colorChannelId;
        u32 inputSpriteIndex;
        u32 triCount;
        u32 groupCombinationIndex;
        GameObject* object;

        LayerKey getFullLayerKey(const ColorChannelBlendStates& blendStates) const;
    };

    std::vector<SpriteEntry> spriteEntries;

    struct GenerateJob {
        ColorChannelBlendStates blendStates;
        UPtr<SortedObjectBatch> sortedBatch;
    };

    std::thread thread;

    isize maxWorkGroupCount = -1;

    //// VARIABLES ACCESSED BY BOTH THREADS ////

    std::map<ColorChannelBlendStates, UPtr<SortedObjectBatch>> sortedBatchesPerBlendStates;
    std::queue<GenerateJob> generateJobs;

    std::atomic<bool> currentlyExecutingJobs = false;

    std::mutex mutex;

    //// INFO FOR CURRENT THREAD ////

    std::vector<SpriteEntry*> sortedSpriteEntries;
    usize currentSpriteIndex;

    std::unordered_map<LayerKey, std::vector<SpriteEntry*>> sortedSpriteLayers;
    std::vector<u32> inputSpriteIndicies;
    std::vector<WorkGroup> workGroups;
};