#ifndef MESHINFO_H
#define MESHINFO_H

#include <memory>

#include "mesh.h"
#include "toolpath.h"

class Mesh;

namespace ChopperEngine {

struct MeshInfo
{
    std::size_t layerCount = 0;
    Mesh* sliceMesh = nullptr;
    LayerComponent* layerComponents = nullptr;

    // ETA gets calculated when data chunks are generated
    long totalMillis = -1;

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

}

#endif // MESHINFO_H
