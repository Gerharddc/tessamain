#ifndef GLES

#include "loadedgl.h"
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <iostream>

QOpenGLFunctions *glFuncs = nullptr;

void LoadedGL::ActivateGL()
{
    if (QOpenGLContext::currentContext())
        glFuncs = QOpenGLContext::currentContext()->functions();
    else
    {
        glFuncs = nullptr;
        std::cout << "No OpenGL context availible." << std::endl;
    }
}

void LoadedGL::DeactivateGL()
{
    glFuncs = nullptr;
}

bool ThrowInactive()
{
    if (glFuncs == nullptr)
    {
        std::cout << "OpenGL function called without context." << std::endl;
        LoadedGL::ActivateGL();
        return true;
    }

    return false;
}

GLuint glCreateShader(GLenum shaderType)
{
    if (ThrowInactive())
        return -1;

    return glFuncs->glCreateShader(shaderType);
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length)
{
    if (ThrowInactive())
        return;

    glFuncs->glShaderSource(shader, count, string, length);
}

void glCompileShader(GLuint shader)
{
    if (ThrowInactive())
        return;

    glFuncs->glCompileShader(shader);
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    if (ThrowInactive())
        return;

    glFuncs->glGetShaderiv(shader, pname, params);
}

void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog)
{
    if (ThrowInactive())
        return;

    glFuncs->glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

GLuint glCreateProgram()
{
    if (ThrowInactive())
        return -1;

    return glFuncs->glCreateProgram();
}

void glDeleteProgram(GLuint program)
{
    if (ThrowInactive())
        return;

    try
    {
        glFuncs->glDeleteProgram(program);
    }
    catch (...) {}
}

void glDeleteShader(GLuint shader)
{
    if (ThrowInactive())
        return;

    glFuncs->glDeleteShader(shader);
}

void glAttachShader(GLuint program, GLuint shader)
{
    if (ThrowInactive())
        return;

    glFuncs->glAttachShader(program, shader);
}

void glLinkProgram(GLuint program)
{
    if (ThrowInactive())
        return;

    glFuncs->glLinkProgram(program);
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    if (ThrowInactive())
        return;

    glFuncs->glGetProgramiv(program, pname, params);
}

void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, char *infoLog)
{
    if (ThrowInactive())
        return;

    glFuncs->glGetProgramInfoLog(program, bufsize, length, infoLog);
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    if (ThrowInactive())
        return;

    glFuncs->glDeleteBuffers(n, buffers);
}

GLint glGetAttribLocation(GLuint program, const char *name)
{
    if (ThrowInactive())
        return -1;

    return glFuncs->glGetAttribLocation(program, name);
}

GLint glGetUniformLocation(GLuint program, const char *name)
{
    if (ThrowInactive())
        return -1;

    return glFuncs->glGetUniformLocation(program, name);
}

void glGenBuffers(GLsizei n, GLuint *buffers)
{
    if (ThrowInactive())
        return;

    glFuncs->glGenBuffers(n, buffers);
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    if (ThrowInactive())
        return;

    glFuncs->glBindBuffer(target, buffer);
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    if (ThrowInactive())
        return;

    glFuncs->glBufferData(target, size, data, usage);
}

void glUseProgram(GLuint program)
{
    if (ThrowInactive())
        return;

    glFuncs->glUseProgram(program);
}

void glEnableVertexAttribArray(GLuint index)
{
    if (ThrowInactive())
        return;

    glFuncs->glEnableVertexAttribArray(index);
}

void glVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *ptr)
{
    if (ThrowInactive())
        return;

    glFuncs->glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (ThrowInactive())
        return;

    glFuncs->glUniformMatrix4fv(location, count, transpose, value);
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
    if (ThrowInactive())
        return;

    glFuncs->glUniform4fv(location, count, value);
}

void glUniform1f(GLint location,  GLfloat v0)
{
    if (ThrowInactive())
        return;

    glFuncs->glUniform1f(location, v0);
}

void glUniform1i(GLint location,  GLint value)
{
    if (ThrowInactive())
        return;

    glFuncs->glUniform1i(location, value);
}

#endif //GLES
