#include "Core/TransformSystem.h"

std::unordered_map<EntityWrapper, glm::vec3> TransformSystem::PositionCache;
std::unordered_map<EntityWrapper, glm::quat> TransformSystem::OrientationCache;
std::unordered_map<EntityWrapper, glm::vec3> TransformSystem::ScaleCache;
std::unordered_map<EntityWrapper, glm::mat4> TransformSystem::MatrixCache;

int TransformSystem::RecalculatedPositions = 0;
int TransformSystem::RecalculatedOrientations = 0;
int TransformSystem::RecalculatedScales = 0;

TransformSystem::TransformSystem(SystemParams params)
    : System(params)
{
    EVENT_SUBSCRIBE_MEMBER(m_EEntityDeleted, &TransformSystem::OnEntityDeleted);
}

bool TransformSystem::OnEntityDeleted(const Events::EntityDeleted& e)
{
    // Clean up deleted entity
    PositionCache.erase(e.DeletedEntity);
    OrientationCache.erase(e.DeletedEntity);
    ScaleCache.erase(e.DeletedEntity);
    MatrixCache.erase(e.DeletedEntity);
    return true;
}

//glm::mat4 Transform::AbsoluteTransformation(EntityWrapper entity)
//{
//    glm::mat4 t = glm::mat4(1.f);
//
//    while (entity.Valid()) {
//        t = glm::translate((const glm::vec3&)entity["Transform"]["Position"]) * glm::toMat4(glm::quat((const glm::vec3&)entity["Transform"]["Orientation"])) * glm::scale((const glm::vec3&)entity["Transform"]["Scale"]) * t;
//        entity = entity.Parent();
//    }
//
//    return t;
//}

glm::vec3 TransformSystem::AbsolutePosition(World* world, EntityID entity)
{
    return TransformSystem::AbsolutePosition(EntityWrapper(world, entity));
}

glm::vec3 TransformSystem::AbsolutePosition(EntityWrapper entity)
{
    if (!entity.Valid()) {
        return glm::vec3();
    }

    auto cacheIt = PositionCache.find(entity);
    ComponentWrapper cTransform = entity["Transform"];
    ComponentWrapper::SubscriptProxy cTransformPosition = cTransform["Position"];
    if (cacheIt != PositionCache.end() && !cTransformPosition.Dirty(DirtySetType::Transform)) {
        return cacheIt->second;
    } else {
        EntityWrapper parent = entity.Parent();
        // Calculate position
        glm::vec3 position = AbsolutePosition(parent) + TransformSystem::AbsoluteOrientation(parent) * (TransformSystem::AbsoluteScale(parent) * (const glm::vec3&)cTransformPosition);
        // Cache it
        PositionCache[entity] = position;
        RecalculatedPositions++;
        // Unset dirty flag
        cTransformPosition.SetDirty(DirtySetType::Transform, false);
        return position;
    }
}

glm::vec3 TransformSystem::AbsoluteOrientationEuler(EntityWrapper entity)
{
    glm::vec3 orientation;

    while (entity.Valid()) {
        ComponentWrapper transform = entity["Transform"];
        orientation += (Field<glm::vec3>)transform["Orientation"];
        entity = entity.Parent();
    }

    return orientation;
}

glm::quat TransformSystem::AbsoluteOrientation(World* world, EntityID entity)
{
    return TransformSystem::AbsoluteOrientation(EntityWrapper(world, entity));
}

glm::quat TransformSystem::AbsoluteOrientation(EntityWrapper entity)
{
    if (!entity.Valid()) {
        return glm::quat();
    }

    auto cacheIt = OrientationCache.find(entity);
    ComponentWrapper cTransform = entity["Transform"];
    ComponentWrapper::SubscriptProxy cTransformOrientation = cTransform["Orientation"];
    if (cacheIt != OrientationCache.end() && !cTransformOrientation.Dirty(DirtySetType::Transform)) {
        return cacheIt->second;
    } else {
        EntityWrapper parent = entity.Parent();
        // Calculate orientation
        glm::quat orientation = AbsoluteOrientation(parent) * glm::quat((const glm::vec3&)cTransformOrientation);
        // Cache it
        OrientationCache[entity] = orientation;
        RecalculatedOrientations++;
        // Unset dirty flag
        cTransformOrientation.SetDirty(DirtySetType::Transform, false);
        return orientation;
    }
}

glm::vec3 TransformSystem::AbsoluteScale(World* world, EntityID entity)
{
    return TransformSystem::AbsoluteScale(EntityWrapper(world, entity));
}

glm::vec3 TransformSystem::AbsoluteScale(EntityWrapper entity)
{
    if (!entity.Valid()) {
        return glm::vec3(1.f);
    }

    auto cacheIt = ScaleCache.find(entity);
    ComponentWrapper cTransform = entity["Transform"];
    ComponentWrapper::SubscriptProxy cTransformScale = cTransform["Scale"];
    if (cacheIt != ScaleCache.end() && !cTransformScale.Dirty(DirtySetType::Transform)) {
        return cacheIt->second;
    } else {
        EntityWrapper parent = entity.Parent();
        // Calculate scale
        glm::vec3 scale = AbsoluteScale(parent) * (const glm::vec3&)cTransformScale;
        // Cache it
        ScaleCache[entity] = scale;
        RecalculatedPositions++;
        // Unset dirty flag
        cTransformScale.SetDirty(DirtySetType::Transform, false);
        return scale;
    }
}

glm::mat4 TransformSystem::ModelMatrix(EntityID entityID, World* world)
{
    return ModelMatrix(EntityWrapper(world, entityID));
}

glm::mat4 TransformSystem::ModelMatrix(EntityWrapper entity)
{
    //auto cacheIt = MatrixCache.find(entity);
    //ComponentWrapper cTransform = entity["Transform"];
    //bool isDirty = cTransform["Position"].Dirty(DirtySetType::Transform) || cTransform["Orientation"].Dirty(DirtySetType::Transform) || cTransform["Scale"].Dirty(DirtySetType::Transform);
    //if (cacheIt != MatrixCache.end() && !isDirty && false) {
    //    return cacheIt->second;
    //} else {
        glm::mat4 matrix = glm::translate(AbsolutePosition(entity)) * glm::toMat4(AbsoluteOrientation(entity)) * glm::scale(AbsoluteScale(entity));
    //    MatrixCache[entity] = matrix;
        return matrix;
    //}
}

glm::vec3 TransformSystem::TransformPoint(const glm::vec3& point, const glm::mat4& matrix)
{
    return glm::vec3(matrix * glm::vec4(point.x, point.y, point.z, 1));
}