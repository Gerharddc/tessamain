#ifndef CHOPPERENGINE_H
#define CHOPPERENGINE_H

#include <string>
#include <memory>

#include "meshinfo.h"

class Mesh;

namespace ChopperEngine
{
    typedef void (*LogDelegate)(std::string message);

    extern MeshInfoPtr SliceMesh(Mesh *inputMesh);
    extern void WriteMeshGcode(std::string outputFile, MeshInfoPtr mip);

    //extern void SliceFile(Mesh* inputMesh, std::string outputFile);
    extern void SlicerLog(std::string message);
    //extern std::size_t layerCount;
    //extern Mesh* sliceMesh;
}

#endif // CHOPPERENGINE_H
