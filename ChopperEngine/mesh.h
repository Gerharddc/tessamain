#ifndef MESH_H
#define MESH_H

#include <vector>
#include <limits>
#include <glm/glm.hpp>

// The unit that defines the component of vectors
typedef float veccomp;

class Vector3
{
public:
    veccomp x = 0;
    veccomp y = 0;
    veccomp z = 0;

    void ToMax()
    {
        x = y = z = std::numeric_limits<veccomp>::max();
    }

    void ToMin()
    {
        x = y = z = std::numeric_limits<veccomp>::lowest();
    }
};

typedef Vector3 Vec3;

struct Triangle;

struct Vertex
{
    std::vector<std::size_t> trigIdxs;
};

struct Triangle
{
    std::size_t vertIdxs[3];
};

// This is not a safe class and should be used carefully because it has almost
// no defences against logical mistakes. This because it should only be used in high
// performance and well tested areas of the program.
class Mesh
{
private:
    float *vertFloats = nullptr;
    float *normFloats = nullptr;

    std::size_t vertArrCount = 0;

public:
    Vec3 MinVec;
    Vec3 MaxVec;

    float *vertexFloats;
    std::size_t *indices;
    Vertex *vertices;
    Triangle *trigs;
    std::size_t trigCount;

    std::size_t vertexCount = 0;

    Mesh(std::size_t size);
    ~Mesh();

    void ShrinkVertices(std::size_t vertCount);
    void ShrinkTrigs(std::size_t newSize);

    glm::vec3 Centre()
    {
        glm::vec3 centre;
        centre.x = (MinVec.x + MaxVec.x) / 2;
        centre.y = (MinVec.y + MaxVec.y) / 2;
        centre.z = (MinVec.z + MaxVec.z) / 2;
        return centre;
    }


    // These functions temporarily create an array of repetead vertices for flat shading
    // it is recommended to delete the values directly after passing them to the GPU
    const float *getFlatVerts();
    void dumpFlatVerts()
    {
        delete[] vertFloats;
        vertFloats = nullptr;
    }

    // These functions temporarily create an array of repetead vertices for flat shading
    // it is recommended to delete the values directly after passing them to the GPU
    const float *getFlatNorms();
    void dumpFlatNorms()
    {
        delete[] normFloats;
        normFloats = nullptr;
    }
};

#endif // MESH_H
