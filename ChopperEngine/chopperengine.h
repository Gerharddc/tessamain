#ifndef CHOPPERENGINE_H
#define CHOPPERENGINE_H

#include <string>
#include <Rendering/structures.h>

namespace ChopperEngine
{
    typedef void (*LogDelegate)(std::string message);

    extern void SliceFile(Mesh* inputMesh, std::string outputFile);
    extern void SlicerLog(std::string message);
    extern std::size_t layerCount;
    extern Mesh* sliceMesh;
}

#endif // CHOPPERENGINE_H
