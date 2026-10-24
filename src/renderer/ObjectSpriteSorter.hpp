#pragma once

#include <common.hpp>
#include <ObjectUtils.hpp>
#include "Buffer.hpp"
#include <map>

class Renderer;

struct WorkGroupRange {
    u32 offset, count;
};

struct SortedObjectBatch {
    UPtr<Buffer> spriteRemapBuffer;
    UPtr<Buffer> workGroupBuffer;
    UPtr<Buffer> drawCommandBuffer;
    UPtr<PreCulledSprite::Buffer> preCulledSpritesBuffer;

    u32 workGroupCount;

    std::map<LayerKey, WorkGroupRange> workGroupRangePerLayerKey;

    void bindBuffers();
};

class ObjectSpriteSorter {
public:
    void addSprite(
        GameObject* object,
        cocos2d::CCSprite* sprite,
        SpriteType type,
        u32 inputSpriteIndex,
        u32 triCount
    );

    void sortForCurrentBlendingStates();

    UPtr<SortedObjectBatch> generateSortedBatch();

    inline usize getSpriteCount() const {
        return spriteEntries.size();
    }

private:
    void generateWorkGroupsForLayer(
        LayerKey layerKey,
        u32 spritesPerWorkGroup,
        u32& culledIndexBufferOutputIndex,
        std::vector<u32>& inputSpriteIndiciesOut,
        std::vector<WorkGroup>& workGroupsOut
    );

private:
    struct SpriteEntry {
        i32 zOrder;
        ZLayer zLayer;
        SpriteSheet spriteSheet;
        u16 colorChannelId;
        u32 inputSpriteIndex;
        u32 triCount;
        GameObject* object;

        LayerKey getFullLayerKey() const;
    };

    std::vector<SpriteEntry> spriteEntries;
    std::vector<SpriteEntry*> sortedSpriteEntries;

    usize currentSpriteIndex;
    
    UPtr<PreCulledSprite::Buffer> preCulledSprites;

    std::unordered_map<LayerKey, std::vector<SpriteEntry*>> sortedSpriteLayers;
};