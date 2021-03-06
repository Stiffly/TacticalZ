#ifndef TriggerSystem_h__
#define TriggerSystem_h__

#include <glm/common.hpp>
#include <unordered_set>

#include "../Core/System.h"
#include "../Core/EventBroker.h"
#include "../Core/Octree.h"
#include "ETrigger.h"
#include "EntityAABB.h"

class AABB;

class TriggerSystem : public PureSystem
{
public:
    TriggerSystem(SystemParams params, Octree<EntityAABB>* octree)
        : System(params)
        , PureSystem("Trigger")
        , m_Octree(octree)
    {
        EVENT_SUBSCRIBE_MEMBER(m_ETouch, &TriggerSystem::OnTouch);
        EVENT_SUBSCRIBE_MEMBER(m_EEnter, &TriggerSystem::OnEnter);
        EVENT_SUBSCRIBE_MEMBER(m_ELeave, &TriggerSystem::OnLeave);
    }

    virtual void UpdateComponent(EntityWrapper& entity, ComponentWrapper& component, double dt) override;

private:
    Octree<EntityAABB>* m_Octree;
    std::vector<EntityAABB> m_OctreeOut;
    std::unordered_map<EntityWrapper, std::unordered_set<EntityWrapper>> m_EntitiesTouchingTrigger;
    std::unordered_map<EntityWrapper, std::unordered_set<EntityWrapper>> m_EntitiesCompletelyInTrigger;

    //TODO: Only exists for debug purposes, remove later.
    EventRelay<TriggerSystem, Events::TriggerEnter> m_EEnter;
    bool OnEnter(const Events::TriggerEnter &event);
    EventRelay<TriggerSystem, Events::TriggerTouch> m_ETouch;
    bool OnTouch(const Events::TriggerTouch &event);
    EventRelay<TriggerSystem, Events::TriggerLeave> m_ELeave;
    bool OnLeave(const Events::TriggerLeave &event);

    //True if leave event was thrown.
    bool throwLeaveIfWasInTrigger(std::unordered_set<EntityWrapper>& triggerSet, EntityWrapper colliderENtity, EntityWrapper triggerEntity);
    template<typename Event>
    void publish(EntityWrapper collider, EntityWrapper trigger)
    {
        Event e;
        e.Trigger = trigger;
        e.Entity = collider;
        m_EventBroker->Publish(e);
    }
};

#endif