#ifndef CollisionSystem_h__
#define CollisionSystem_h__

#include <GLFW/glfw3.h>
#include <glm/common.hpp>

#include "Common.h"
#include "Core/System.h"
#include "Core/EventBroker.h"
#include "Core/EKeyUp.h"

class CollisionSystem : public System
{
public:
    CollisionSystem(EventBroker* eventBroker)
        : System(eventBroker, "AABB")
    {
        //TODO: Debug stuff, remove later.
        EVENT_SUBSCRIBE_MEMBER(m_EKeyUp, &CollisionSystem::OnKeyUp);
    }

    virtual void Update(World* world, ComponentWrapper& collision, double dt) override;

private:

    EventRelay<CollisionSystem, Events::KeyUp> m_EKeyUp;
    bool OnKeyUp(const Events::KeyUp &event);
};

#endif