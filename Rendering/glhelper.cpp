#include "glhelper.h"

#include <iostream>
#include <errno.h>
#include <cstring>
#include <QCoreApplication>
#include <QString>
#include <QFile>

#ifdef NOTQRC
std::string GLHelper::ReadEntireFile(const std::string &path)
{
    QString np = QCoreApplication::applicationDirPath() + "/" + QString::fromStdString(path);

    std::ifstream in(np.toStdString(), std::ios::in);

    if (in.is_open())
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return contents;
    }
    else
    {
        std::cout << "Could not open " << np.toStdString() << " error: " << strerror(errno) << std::endl;
        throw "Could not open gl file"; // TODO: make this better
    }
}
#else
std::string GLHelper::ReadEntireFile(const std::string &path)
{
    QFile file(QString::fromStdString(":/GL/" + path));
    file.open(QFile::ReadOnly | QFile::Text);

    QString string = QString(file.readAll());
    return string.toStdString();
}

#endif

GLuint GLHelper::CompileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);

    const char *sourceArray = source.c_str();
    glShaderSource(shader, 1, &sourceArray, NULL);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    if (compileResult == 0)
    {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

        GLchar infoLog[infoLogLength];
        glGetShaderInfoLog(shader, infoLogLength, NULL, infoLog);

        throw std::runtime_error("Shader compliation failed: " + std::string(infoLog));
        return -1;
    }

    return shader;
}

GLuint GLHelper::CompileProgram(const std::string &vsSource, const std::string &fsSource)
{
    GLuint program = glCreateProgram();

    if (program == 0)
    {
        throw std::runtime_error("GL program creation failed.");
        return -1;
    }

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

    if (vs == 0 || fs == 0)
    {
        glDeleteShader(fs);
        glDeleteShader(vs);
        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if (linkStatus == 0)
    {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

        GLchar infoLog[infoLogLength];
        glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);

        throw std::runtime_error("Program link failed: " + std::string(infoLog));
        return -1;
    }

    return program;
}

GLuint GLHelper::CompileProgramFromFile(const std::string &vsFile, const std::string &fsFile)
{
    std::string vs = ReadEntireFile(vsFile);
    std::string fs = ReadEntireFile(fsFile);
    return CompileProgram(vs, fs);
}
