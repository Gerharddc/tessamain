#ifndef GRIDRENDERER_H
#define GRIDRENDERER_H

#ifdef GLES
#include <GLES2/gl2.h>
#elif defined(QT_APPLICATION)
#include "loadedgl.h"
#endif

#include "structures.h"
#include "gridgeneration.h"

namespace GridRendering
{
    void FreeMemory();
    void Draw();
    void Init();

    void ProjMatDirty();
    void SceneMatDirty();
    void GridDirty();
}

#endif // GRIDRENDERER_H
