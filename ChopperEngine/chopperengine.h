#ifndef CHOPPERENGINE_H
#define CHOPPERENGINE_H

#include <string>
#include <Rendering/structures.h>
#include <memory>

#include "toolpath.h"

namespace ChopperEngine
{
    typedef void (*LogDelegate)(std::string message);

    struct MeshInfo
    {
        std::size_t layerCount = 0;
        Mesh* sliceMesh = nullptr;
        LayerComponent* layerComponents = nullptr;

        MeshInfo(Mesh *inputMesh) {
            sliceMesh = inputMesh;

            // Calculate the amount layers that will be sliced
            // TODO
            layerCount = (std::size_t)(sliceMesh->MaxVec.z / GlobalSettings::LayerHeight.Get()) + 1;

            if (layerComponents != nullptr)
                layerComponents = (LayerComponent*)realloc(layerComponents, sizeof(LayerComponent) * layerCount);
            else
                layerComponents = (LayerComponent*)malloc(sizeof(LayerComponent) * layerCount);

            for (std::size_t i = 0; i < layerCount; i++)
                new ((void*)(layerComponents + i)) LayerComponent();
        }

        ~MeshInfo() {
            // Free the memory
            if (layerComponents != nullptr)
            {
                for (std::size_t i = 0; i < layerCount; i++)
                    layerComponents[i].~LayerComponent();

                free(layerComponents);
            }
        }
    };

    typedef std::shared_ptr<MeshInfo> MeshInfoPtr;

    extern MeshInfoPtr SliceMesh(Mesh *inputMesh);
    extern void WriteMeshGcode(std::string outputFile, MeshInfoPtr mip);

    //extern void SliceFile(Mesh* inputMesh, std::string outputFile);
    extern void SlicerLog(std::string message);
    //extern std::size_t layerCount;
    //extern Mesh* sliceMesh;
}

#endif // CHOPPERENGINE_H
