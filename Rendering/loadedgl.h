#ifndef LOADEDGL_H
#define LOADEDGL_H

#include <QOpenGLFunctions>

namespace LoadedGL
{
    extern void ActivateGL();
    extern void DeactivateGL();
}

extern GLuint glCreateShader(GLenum shaderType);
extern void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
extern void glCompileShader(GLuint shader);
extern void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
extern void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern GLuint glCreateProgram();
extern void glDeleteProgram(GLuint program);
extern void glDeleteShader(GLuint shader);
extern void glAttachShader(GLuint program, GLuint shader);
extern void glLinkProgram(GLuint program);
extern void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
extern void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, char *infoLog);
extern void glDeleteBuffers(GLsizei n, const GLuint *buffers);
extern GLint glGetAttribLocation(GLuint program, const char * name);
extern GLint glGetUniformLocation(GLuint program, const char* name);
extern void glGenBuffers(GLsizei n, GLuint *buffers);
extern void glBindBuffer(GLenum target, GLuint buffer);
extern void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern void glUseProgram(GLuint program);
extern void glEnableVertexAttribArray(GLuint index);
extern void glVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *ptr);
extern void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
extern void glUniform1f(GLint location,  GLfloat v0);
extern void glUniform1i(GLint location,  GLint value);

#endif // LOADEDGL_H
