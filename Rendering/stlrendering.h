#ifndef STLRENDERER_H
#define STLRENDERER_H

#ifdef GLES
#include <GLES2/gl2.h>
#elif defined(QT_APPLICATION)
#include "loadedgl.h"
#endif

#include <map>
#include <set>
#include <thread>
#include <glm/gtx/quaternion.hpp>
#include "structures.h"

const glm::vec4 normalMeshCol = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
const glm::vec4 selectedMeshCol = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

struct MeshGroupData
{
    bool meshDirty = false;

    GLuint mVertexPositionBuffer = 0;
    GLuint mVertexNormalBuffer = 0;

    // The scale that the actual mesh is currently on
    float scaleOnMesh = 1.0f;
    // The scale that the matrix is applying to the mesh
    float scaleOnMat = 1.0f;

    // The matrix that is currently being GPU-applied to the mesh
    glm::mat4 gpuMat;
    // The matrix that has already been applied to the mesh as seen by the cpu
    glm::mat4 meshMat;

    // Thes matrices and flag are used to store the combined mesh and scene matrices
    glm::mat4 sceneMat, normalMat;
    bool sceneMatsDirty = true;
    bool meshMatDirty = false;

    // The colour of the mesh with transparency
    glm::vec4 color = normalMeshCol;

    // The rotation that the actual mesh currently has
    glm::vec3 rotOnMesh;
    // The rotation that the matrix is applying to the mesh
    glm::vec3 rotOnMat;

    // This is the negative coordinate at the centre of the mesh
    glm::vec3 meshCentre;
    // The offset that the actual mesh currently has
    glm::vec3 moveOnMesh;
    // The offset that the matrix is applying to the mesh
    glm::vec3 moveOnMat;

    // The radius of the bounding sphere (scaled)
    float bSphereRadius;

    // Dimensions (scaled)
    float length, width, height;

    ~MeshGroupData();
};

namespace STLRendering {
    void FreeMemory();

    void Draw();
    void Init();

    void AddMesh(Mesh *mesh);
    void RemoveMesh(Mesh *mesh);
    void PackMeshes();

    void ScaleMesh(Mesh *mesh, float absScale);
    void CentreMesh(Mesh *mesh, float absX, float absY);
    void LiftMesh(Mesh *mesh, float absZ);
    void RotateMesh(Mesh *mesh, float absX, float absY, float absZ);

    const MeshGroupData &getMeshData(Mesh *mesh);

    void ColorMesh(Mesh *mesh, const glm::vec4 &colorAlpha);
    void ColorMesh(Mesh *mesh, const glm::vec3 &color);
    void ColorMesh(Mesh *mesh, float alpha);

    void SetBaseOpacity(float alpha);
    float GetBaseOpacity();

    bool TestMeshIntersection(Mesh *mesh, const glm::vec3 &near, const glm::vec3 &far, const glm::mat4 &MV, float &screenZ);

    // Applies pending changes to meshes in memory and returns if any changes were made
    bool PrepMeshesSave(std::set<Mesh*> &meshes);

    void ProjMatDirty();
    void SceneMatDirty();
}

#endif // STLRENDERER_H
