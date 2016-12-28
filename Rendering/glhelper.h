#ifndef GLHELPER
#define GLHELPER

#ifdef GLES
#include <GLES2/gl2.h>
#elif defined(QT_APPLICATION)
#include "loadedgl.h"
#endif

#include <stdexcept>
#include <string>
#include <fstream>

namespace GLHelper
{
    std::string ReadEntireFile(const std::string &path);

    GLuint CompileShader(GLenum type, const std::string &source);

    GLuint CompileProgram(const std::string &vsSource, const std::string &fsSource);

    GLuint CompileProgramFromFile(const std::string &vsFile, const std::string &fsFile);
}

#endif // GLHELPER
