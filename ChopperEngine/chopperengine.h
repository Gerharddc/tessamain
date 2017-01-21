#ifndef CHOPPERENGINE_H
#define CHOPPERENGINE_H

#include <string>

#include "meshinfo.h"
#include "mesh.h"
#include "progressor.h"

namespace ChopperEngine
{
    typedef void (*LogDelegate)(std::string message);

    extern MeshInfoPtr SliceMesh(Mesh *inputMesh, Progressor::ProgressCallback callback,
                                 const void *callbackContext);

    extern void SlicerLog(std::string message);    
}

#endif // CHOPPERENGINE_H
