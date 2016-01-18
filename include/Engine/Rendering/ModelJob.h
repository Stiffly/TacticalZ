#ifndef ModelJob_h__
#define ModelJob_h__

#include <cstdint>

#include "../Common.h"
#include "../GLM.h"
#include "../Core/ComponentWrapper.h"
#include "Texture.h"
#include "Model.h"
#include "RenderJob.h"
#include "../Core/ResourceManager.h"
#include "Camera.h"
#include "../Core/World.h"

struct ModelJob : RenderJob
{
    ModelJob(Model* model, Camera* camera, glm::mat4 matrix, ::Model::MaterialGroup texGroup, ComponentWrapper modelComponent, World* world)
        : RenderJob()
    {
        Model = model;
        TextureID = (texGroup.Texture) ? texGroup.Texture->ResourceID : 0;
        DiffuseTexture = texGroup.Texture.get();
        NormalTexture = texGroup.NormalMap.get();
        SpecularTexture = texGroup.SpecularMap.get();
        StartIndex = texGroup.StartIndex;
        EndIndex = texGroup.EndIndex;
        Matrix = matrix;
        Color = modelComponent["Color"];
        Entity = modelComponent.EntityID;
        World = world;
    };

    unsigned int TextureID;
    unsigned int ShaderID;

    EntityID Entity;
    glm::mat4 Matrix;
    const Texture* DiffuseTexture;
    const Texture* NormalTexture;
    const Texture* SpecularTexture;
    float Shininess = 0.f;
    glm::vec4 Color;
    const ::Model* Model = nullptr;
    unsigned int StartIndex = 0;
    unsigned int EndIndex = 0;
    const World* World;

    void CalculateHash() override
    {
        Hash = TextureID;
    }
};

#endif