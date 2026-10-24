#pragma once

#include <Geode/binding/GameObject.hpp>
#include <common.hpp>
#include "Geode/cocos/sprite_nodes/CCSpriteBatchNode.h"
#include "ObjectBatch.hpp"
#include "ObjectUtils.hpp"
#include "OverdrawView.hpp"
#include "Shader.hpp"
#include <unordered_map>
#include <array>

#include "DifferenceMode.hpp"
#include "GroupManager.hpp"
#include "ObjectBatchNode.hpp"
#include "SAOBuffer.hpp"

using namespace geode;

class Renderer : public cocos2d::CCNode {
private:
    inline Renderer()
        : objectBatch(*this), groupManager(*this), differenceMode(*this),
          overdrawView(*this) {}
    ~Renderer() override;

    bool init(PlayLayer* layer, cocos2d::CCArray* objects);

    void generateBatchNodes();

    void terminate();

    void prepareShaderUniforms();

    void prepareColorChannelBuffer();

    void generateStaticRenderingBuffer(std::vector<GameObject*>& objects);

    void draw() override;

    void finish();

    void updateDebugText();

    void bindCullingComputeShaderBuffers();

    void fetchBlendingStates();

    friend class FinishNode;
    friend class ObjectBatch;

protected:
    Shader* prepareDraw();

    void finishDraw();

    friend class GroupManager;
    friend class ObjectBatchNode;

public:
    void update(float dt) override;

    cocos2d::CCSpriteBatchNode* getSpriteBatchNodeWithLayerId(LayerKey id);

    inline bool isColorChannelBlending(i32 channel) {
        return colorChannelsIsBlending[channel];
    }

    inline cocos2d::CCTexture2D* getSpriteSheetTexture(SpriteSheet sheet) {
        if ((i32)sheet < 0 || (i32)sheet >= (i32)SpriteSheet::COUNT)
            return nullptr;
        return spriteSheets[(i32)sheet];
    }

    inline PlayLayer* getPlayLayer() {
        return layer;
    }

    inline usize getObjectSRBIndex(GameObject* object) {
        return objectSRBIndicies[object];
    }

    inline bool isEnabled() { return enabled; }

    inline DifferenceMode* getDifferenceMode() {
        if (!differenceModeEnabled) return nullptr;
        return &differenceMode;
    }

    inline void toggleDebugText() {
        debugTextEnabled = !debugTextEnabled;
    }

    inline bool isDifferenceModeEnabled() const { return differenceModeEnabled; }
    inline void setDifferenceModeEnabled(bool enabled) {
        differenceModeEnabled = enabled;
    }

    inline bool canEnableDisableIngame() const { return ingameEnableDisable; }

    inline bool isPaused() {
        return layer->m_isPaused;
    }

    inline ObjectBatch& getObjectBatch() { return objectBatch; }

    inline Shader* getCullingComputeShader() const {
        return cullingComputeShader.get();
    }

    inline usize getWorkGroupCount() const { return workGroupCount; }

    bool useOptimizations();

    void setEnabled(bool enabled);

    inline GroupManager& getGroupManager() { return groupManager; }
    
    void reset();

    void drawLine(const glm::vec2& p1, const glm::vec2& p2, const glm::vec4& color);

public:
    static Ref<Renderer> create(PlayLayer* layer, cocos2d::CCArray* objects);

    /**
     * Returns the current renderer. If the game
     * is not in a PlayLayer. This returns nullptr.
     */
    static Ref<Renderer> get();

private:
    PlayLayer* layer;

    bool enabled = false;
    bool debugTextEnabled = false;
    bool ingameEnableDisable = false;

    u64 rendererStartTime = 0;

    usize spritesOnScreen = 0;

    usize workGroupCount = 0;

    std::unordered_map<GameObject*, usize> objectSRBIndicies;

    u32 timeElapsedQuery = 0;

    glm::vec2 cameraCenterPos;

    std::vector<ObjectBatchNode*> batchNodes;

    cocos2d::CCTexture2D* spriteSheets[(i32)SpriteSheet::COUNT] = { nullptr };

    std::array<bool, 1101> colorChannelsIsBlending;

    Ref<cocos2d::CCLabelBMFont> debugText;
    Ref<cocos2d::CCLabelBMFont> debugTextOutline1;
    Ref<cocos2d::CCLabelBMFont> debugTextOutline2;

    GPUTimer renderingTimer { "Rendering time" };

    GroupManager groupManager;

    ObjectBatch objectBatch;
    UPtr<Shader> shader = nullptr;
    UPtr<Shader> basicShader = nullptr;
    UPtr<Shader> cullingComputeShader = nullptr;

    ColorChannelBuffer* colorChannelBuffer;
    UPtr<Buffer> colorChannelBufferObject = nullptr;

    UPtr<Buffer> srbBuffer = nullptr;

    GameStateBuffer gameState;
    UPtr<Buffer> gameStateBuffer = nullptr;

    bool differenceModeEnabled = false;
    DifferenceMode differenceMode;
    OverdrawView overdrawView;

    float gameTimer = 0.0;

    i64 groupStateCount;
    i64 renderedGameObjectCount;
    i64 renderTime;
};

void storeGLStates();

void restoreGLStates();