#include "Geode/cocos/CCDirector.h"
#include "Renderer.hpp"
#include <decomp/PlayLayer.hpp>
#include <decomp/GJBaseGameLayer.hpp>
#include <BProfiler.hpp>

using namespace geode::prelude;

static bool newPlayLayer = false;
// static CCArray* allGameObjects = nullptr;

#include <Geode/modify/PlayLayer.hpp>
class $modify(RendererPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        newPlayLayer = true;
        PlayLayer::setupHasCompleted();
        newPlayLayer = false;
    }

    bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects) {
        // if (!allGameObjects) {
        //     allGameObjects = CCArray::create();
        //     allGameObjects->retain();
        // }
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }

    // This gets called from inside PlayLayer::setupHasCompleted()
    void resetLevel() {
        if (newPlayLayer) {
            auto batchLayer = this->m_objectLayer;
            if (!batchLayer) {
                log::error("failed to attach renderer: batch layer not found");
                return;
            }

            auto renderer = Renderer::create(this, m_objects);
            if (renderer)
                batchLayer->addChild(renderer, -100000);
        }

        auto renderer = Renderer::get();
        if (renderer)
            renderer->reset();

        PlayLayer::resetLevel();
    }

    void updateVisibility(float dt) {
        auto timer = BProfiler::start("PlayLayer::updateVisibility");
        auto renderer = Renderer::get();
        if (renderer && renderer->useOptimizations()) {
            ((decomp_PlayLayer*)this)->optimized_updateVisibility(dt);
            timer.end();
            return;
        }

        PlayLayer::updateVisibility(dt);
        // ((decomp_PlayLayer*)this)->virtual_updateVisibility(dt);
        timer.end();
    }
};

#include <Geode/modify/GameObject.hpp>
class $modify(MyGameObject, GameObject) {
    /*
    static GameObject* objectFromVector(
        gd::vector<gd::string>& propValues,
        gd::vector<void*>& propIsPresent,
        GJBaseGameLayer* gameLayer,
        bool lowDetail
    ) {
        GameObject* ret = GameObject::objectFromVector(propValues, propIsPresent, gameLayer, lowDetail);
        if (!GameManager::get()->m_playLayer)
            return ret;

        allGameObjects->addObject(ret);
        ret->retain();

        if (ret->m_objectType == GameObjectType::Decoration)
            return ret;
        
        return ret;
    }
    */
};

static void removeDecoObjects(CCArray* array) {
    for (u32 i = 0; i < array->count();) {
        auto object = (GameObject*)array->objectAtIndex(i);
        if (object->m_objectType == GameObjectType::Decoration)
            array->removeObjectAtIndex(i);
        else
            i++;
    }
}

#include <Geode/modify/GJBaseGameLayer.hpp>
class $modify(RendererGJBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto timer = BProfiler::start("GJBaseGameLayer::update");
        GJBaseGameLayer::update(dt);
        timer.end();

        auto renderer = Renderer::get();
        if (renderer)
            renderer->update(dt);
    }

    void processMoveActions() {
        auto renderer = Renderer::get();
        if (renderer == nullptr) {
            GJBaseGameLayer::processMoveActions();
            return;
        }

        m_effectManager->processMoveCalculations();
        for (auto node : m_effectManager->m_unkVector6c0) {
            if (node->m_unk0d0)
                continue;

            int groupId = node->getTag();

            renderer->getGroupManager().moveGroup(groupId, node->m_unk038, node->m_unk040);

            CCArray* objects = getStaticGroup(groupId);
            if (objects)
                moveObjects(objects, node->m_unk038, node->m_unk040, 0);

            if (!renderer->useOptimizations()) {
                objects = getOptimizedGroup(groupId);
                if (objects)
                    moveObjects(objects, node->m_unk090, node->m_unk098, 0);
            }
        }
    }

    void siphon_processRotationActions() {
        auto renderer = Renderer::get();
        if (renderer == nullptr) {
            GJBaseGameLayer::processRotationActions();
            return;
        }

        auto eman = m_effectManager;
        for (auto cmdObj : eman->m_unkVector5b0) {
            if (/* cmdObj->m_unkInt204 != m_gameState.m_unkUint2 ||*/ cmdObj->m_someInterpValue1RelatedFalse)
                continue;

            i32 targetId = cmdObj->m_targetGroupID;
            i32 centerId = cmdObj->m_centerGroupID;

            auto mainObject = tryGetMainObject(centerId);
            auto staticGroup = getStaticGroup(targetId);

            float rotation;
            if (staticGroup->count() != 0)
                rotation = cmdObj->m_someInterpValue1RelatedOne - cmdObj->m_someInterpValue1RelatedZero;
            else
                rotation = cmdObj->m_someInterpValue2RelatedOne - cmdObj->m_someInterpValue2RelatedZero;

            if (rotation == 0.0 && !cmdObj->m_finishRelated)
                continue;

            // Idk what the hell this does but sure
            if (eman->m_unkMap770.find({ targetId, centerId }) != eman->m_unkMap770.end()) {
                for (auto obj : eman->m_unkMap770[{ targetId, centerId }]) {
                    if (obj->m_someInterpValue1RelatedFalse)
                        continue;
                    if (staticGroup->count() != 0)
                        rotation += cmdObj->m_someInterpValue1RelatedOne - cmdObj->m_someInterpValue1RelatedZero;
                    else
                        rotation += cmdObj->m_someInterpValue2RelatedOne - cmdObj->m_someInterpValue2RelatedZero;
                }
            }

            if (mainObject == nullptr)
                renderer->getGroupManager().rotateGroup(targetId, rotation, cmdObj->m_lockObjectRotation);
            else {
                auto pos = mainObject->getUnmodifiedPosition();
                renderer->getGroupManager().rotateGroup(targetId, rotation, cmdObj->m_lockObjectRotation, ccPointToGLM(pos));
            }
        }

        GJBaseGameLayer::processRotationActions();
    }

    void processRotationActions() {
        auto renderer = Renderer::get();
        if (renderer == nullptr) {
            GJBaseGameLayer::processRotationActions();
            return;
        }

        if (renderer->useOptimizations())
            ((decomp_GJBaseGameLayer*)this)->optimized_processRotationActions();
        else
            siphon_processRotationActions();
    }

    void processFollowActions() {
        auto renderer = Renderer::get();
        if (renderer == nullptr) {
            GJBaseGameLayer::processFollowActions();
            return;
        }

        auto eman = m_effectManager;
        for (auto node : eman->m_unkVector6d8) {
            i32 targetGroupId = node->getTag();
            i32 followGroupId = node->m_unk074;

            auto mainObject = tryGetMainObject(followGroupId);
            if (mainObject == nullptr)
                continue;

            double moveX = 0.0, moveY = 0.0;
            if (mainObject->m_unk4C4 == m_gameState.m_commandIndex) {
                moveX = (mainObject->m_positionX - mainObject->m_lastPosition.x) * node->m_unk080; /* followXMod */
                moveY = (mainObject->m_positionY - mainObject->m_lastPosition.y) * node->m_unk088; /* followYMod */
            }

            renderer->getGroupManager().moveGroup(targetGroupId, moveX, moveY);
        }

        GJBaseGameLayer::processFollowActions();
    }

    void toggleGroup(int id, bool activate) {
        GJBaseGameLayer::toggleGroup(id, activate);
        auto renderer = Renderer::get();
        if (renderer)
            renderer->getGroupManager().toggleGroup(id, activate);
    }

    void optimizeMoveGroups() {
        GJBaseGameLayer::optimizeMoveGroups();

        auto renderer = Renderer::get();
        if (!renderer || !renderer->useOptimizations())
            return;

        for (auto array : m_optimizedGroups)
            removeDecoObjects(array);
        for (auto array : m_staticGroups)
            removeDecoObjects(array);
    }
};

#include <Geode/modify/CCDisplayLinkDirector.hpp>
class $modify(RendererCCDisplayLinkDirector, CCDisplayLinkDirector) {
    void mainLoop() {
        auto ren = Renderer::get();
        if (!ren) {
            CCDisplayLinkDirector::mainLoop();
            return;
        }

        auto timer = BProfiler::start("Frame time");
        CCDisplayLinkDirector::mainLoop();
        timer.end();

        BProfiler::frameEnd();
    }
};