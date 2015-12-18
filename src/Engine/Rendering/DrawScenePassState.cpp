#include "Rendering/DrawScenePassState.h"


DrawScenePassState::DrawScenePassState()
{
    GLERROR("---");
    BindFramebuffer(0);
    GLERROR("---");
    Enable(GL_DEPTH_TEST);
    Enable(GL_CULL_FACE);
    ClearColor(glm::vec4(255.f / 255, 163.f / 255, 176.f / 255, 0.f));
    Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

DrawScenePassState::~DrawScenePassState()
{

}
