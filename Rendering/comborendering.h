#ifndef COMBORENDERING_H
#define COMBORENDERING_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <set>
#include <future>
#include <string>

#include "stlrendering.h"
#include "toolpathrendering.h"
#include "gridrendering.h"
#include "structures.h"

namespace ComboRendering
{
    void FreeMemory();

    typedef void(*UpdateHandler)(void*);
    void SetUpdateHandler(UpdateHandler handler,void *context);

    void SetViewSize(float width, float height);
    void Init();
    void Draw();
    void ApplyRot(float x, float y);
    void Move(float x, float y);
    void Zoom(float scale);
    void ResetView(bool updateNow);
    void LoadMesh(std::string path);
    void LoadToolpath(std::string path);
    void RemoveMesh(Mesh *mesh);
    void Update();
    std::string SaveMeshes(std::string fileName);
    std::string SliceMeshes(std::string fileName);

    void TestMouseIntersection(float x, float y);

    const std::set<Mesh*> &getSelectedMeshes();
    const Toolpath* getToolpath();

    const glm::mat4 &getSceneTrans();
    const glm::mat4 &getSceneProj();
    int getMeshCount();
}

#endif // COMBORENDERING_H
