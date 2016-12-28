#ifndef TOOLPATHRENDERER_H
#define TOOLPATHRENDERER_H

#ifdef GLES
#include <GLES2/gl2.h>
#elif defined(QT_APPLICATION)
#include "loadedgl.h"
#endif

#include "structures.h"

namespace ToolpathRendering {
    void FreeMemory();
    void Draw();
    void Init();
    void SetToolpath(Toolpath *tp);\
    void ProjMatDirty();
    void SceneMatDirty();
    void SetOpacity(float alpha);
    float GetOpacity();
    void SetColor(glm::vec3 color);
    bool ToolpathLoaded();
    void ShowPrintedToLine(int64_t lineNum);
}

#endif // TOOLPATHRENDERER_H
