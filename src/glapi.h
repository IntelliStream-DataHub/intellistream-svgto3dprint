/* Minimal OpenGL 3.3 core loader.  Generated: do not include system GL headers. */
#ifndef LOGO3D_GLAPI_H
#define LOGO3D_GLAPI_H

#include <stddef.h>

#if defined(_WIN32) && !defined(_WIN64)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_ALWAYS 0x0207
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_MULTISAMPLE 0x809D
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_FUNC_ADD 0x8006
#define GL_LINE_SMOOTH 0x0B20

typedef void (GLAPIENTRY *PFN_glEnable)(GLenum cap);
typedef void (GLAPIENTRY *PFN_glDisable)(GLenum cap);
typedef void (GLAPIENTRY *PFN_glBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void (GLAPIENTRY *PFN_glBlendEquation)(GLenum mode);
typedef void (GLAPIENTRY *PFN_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY *PFN_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY *PFN_glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
typedef void (GLAPIENTRY *PFN_glClear)(GLbitfield mask);
typedef void (GLAPIENTRY *PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (GLAPIENTRY *PFN_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef GLenum (GLAPIENTRY *PFN_glGetError)(void);
typedef void (GLAPIENTRY *PFN_glGetIntegerv)(GLenum pname, GLint *data);
typedef const GLubyte * (GLAPIENTRY *PFN_glGetString)(GLenum name);
typedef void (GLAPIENTRY *PFN_glDepthFunc)(GLenum func);
typedef void (GLAPIENTRY *PFN_glDepthMask)(GLboolean flag);
typedef void (GLAPIENTRY *PFN_glCullFace)(GLenum mode);
typedef void (GLAPIENTRY *PFN_glFrontFace)(GLenum mode);
typedef void (GLAPIENTRY *PFN_glLineWidth)(GLfloat width);
typedef void (GLAPIENTRY *PFN_glPolygonOffset)(GLfloat factor, GLfloat units);
typedef void (GLAPIENTRY *PFN_glPixelStorei)(GLenum pname, GLint param);
typedef void (GLAPIENTRY *PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GLAPIENTRY *PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY *PFN_glGenTextures)(GLsizei n, GLuint *textures);
typedef void (GLAPIENTRY *PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void (GLAPIENTRY *PFN_glDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void (GLAPIENTRY *PFN_glActiveTexture)(GLenum texture);
typedef GLuint (GLAPIENTRY *PFN_glCreateShader)(GLenum type);
typedef void (GLAPIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
typedef void (GLAPIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void (GLAPIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GLAPIENTRY *PFN_glDeleteShader)(GLuint shader);
typedef GLuint (GLAPIENTRY *PFN_glCreateProgram)(void);
typedef void (GLAPIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (GLAPIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void (GLAPIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GLAPIENTRY *PFN_glUseProgram)(GLuint program);
typedef void (GLAPIENTRY *PFN_glDeleteProgram)(GLuint program);
typedef GLint (GLAPIENTRY *PFN_glGetUniformLocation)(GLuint program, const GLchar *name);
typedef GLint (GLAPIENTRY *PFN_glGetAttribLocation)(GLuint program, const GLchar *name);
typedef void (GLAPIENTRY *PFN_glUniform1i)(GLint location, GLint v0);
typedef void (GLAPIENTRY *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void (GLAPIENTRY *PFN_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GLAPIENTRY *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GLAPIENTRY *PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (GLAPIENTRY *PFN_glDisableVertexAttribArray)(GLuint index);
typedef void (GLAPIENTRY *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (GLAPIENTRY *PFN_glGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (GLAPIENTRY *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (GLAPIENTRY *PFN_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (GLAPIENTRY *PFN_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (GLAPIENTRY *PFN_glDeleteBuffers)(GLsizei n, const GLuint *buffers);
typedef void (GLAPIENTRY *PFN_glGenVertexArrays)(GLsizei n, GLuint *arrays);
typedef void (GLAPIENTRY *PFN_glBindVertexArray)(GLuint array);
typedef void (GLAPIENTRY *PFN_glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);
typedef void (GLAPIENTRY *PFN_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (GLAPIENTRY *PFN_glFinish)(void);

extern PFN_glEnable l3d_glEnable;
extern PFN_glDisable l3d_glDisable;
extern PFN_glBlendFunc l3d_glBlendFunc;
extern PFN_glBlendEquation l3d_glBlendEquation;
extern PFN_glViewport l3d_glViewport;
extern PFN_glScissor l3d_glScissor;
extern PFN_glClearColor l3d_glClearColor;
extern PFN_glClear l3d_glClear;
extern PFN_glDrawArrays l3d_glDrawArrays;
extern PFN_glDrawElements l3d_glDrawElements;
extern PFN_glGetError l3d_glGetError;
extern PFN_glGetIntegerv l3d_glGetIntegerv;
extern PFN_glGetString l3d_glGetString;
extern PFN_glDepthFunc l3d_glDepthFunc;
extern PFN_glDepthMask l3d_glDepthMask;
extern PFN_glCullFace l3d_glCullFace;
extern PFN_glFrontFace l3d_glFrontFace;
extern PFN_glLineWidth l3d_glLineWidth;
extern PFN_glPolygonOffset l3d_glPolygonOffset;
extern PFN_glPixelStorei l3d_glPixelStorei;
extern PFN_glTexImage2D l3d_glTexImage2D;
extern PFN_glTexParameteri l3d_glTexParameteri;
extern PFN_glGenTextures l3d_glGenTextures;
extern PFN_glBindTexture l3d_glBindTexture;
extern PFN_glDeleteTextures l3d_glDeleteTextures;
extern PFN_glActiveTexture l3d_glActiveTexture;
extern PFN_glCreateShader l3d_glCreateShader;
extern PFN_glShaderSource l3d_glShaderSource;
extern PFN_glCompileShader l3d_glCompileShader;
extern PFN_glGetShaderiv l3d_glGetShaderiv;
extern PFN_glGetShaderInfoLog l3d_glGetShaderInfoLog;
extern PFN_glDeleteShader l3d_glDeleteShader;
extern PFN_glCreateProgram l3d_glCreateProgram;
extern PFN_glAttachShader l3d_glAttachShader;
extern PFN_glLinkProgram l3d_glLinkProgram;
extern PFN_glGetProgramiv l3d_glGetProgramiv;
extern PFN_glGetProgramInfoLog l3d_glGetProgramInfoLog;
extern PFN_glUseProgram l3d_glUseProgram;
extern PFN_glDeleteProgram l3d_glDeleteProgram;
extern PFN_glGetUniformLocation l3d_glGetUniformLocation;
extern PFN_glGetAttribLocation l3d_glGetAttribLocation;
extern PFN_glUniform1i l3d_glUniform1i;
extern PFN_glUniform1f l3d_glUniform1f;
extern PFN_glUniform3f l3d_glUniform3f;
extern PFN_glUniform4f l3d_glUniform4f;
extern PFN_glUniformMatrix4fv l3d_glUniformMatrix4fv;
extern PFN_glEnableVertexAttribArray l3d_glEnableVertexAttribArray;
extern PFN_glDisableVertexAttribArray l3d_glDisableVertexAttribArray;
extern PFN_glVertexAttribPointer l3d_glVertexAttribPointer;
extern PFN_glGenBuffers l3d_glGenBuffers;
extern PFN_glBindBuffer l3d_glBindBuffer;
extern PFN_glBufferData l3d_glBufferData;
extern PFN_glBufferSubData l3d_glBufferSubData;
extern PFN_glDeleteBuffers l3d_glDeleteBuffers;
extern PFN_glGenVertexArrays l3d_glGenVertexArrays;
extern PFN_glBindVertexArray l3d_glBindVertexArray;
extern PFN_glDeleteVertexArrays l3d_glDeleteVertexArrays;
extern PFN_glReadPixels l3d_glReadPixels;
extern PFN_glFinish l3d_glFinish;

#define glEnable l3d_glEnable
#define glDisable l3d_glDisable
#define glBlendFunc l3d_glBlendFunc
#define glBlendEquation l3d_glBlendEquation
#define glViewport l3d_glViewport
#define glScissor l3d_glScissor
#define glClearColor l3d_glClearColor
#define glClear l3d_glClear
#define glDrawArrays l3d_glDrawArrays
#define glDrawElements l3d_glDrawElements
#define glGetError l3d_glGetError
#define glGetIntegerv l3d_glGetIntegerv
#define glGetString l3d_glGetString
#define glDepthFunc l3d_glDepthFunc
#define glDepthMask l3d_glDepthMask
#define glCullFace l3d_glCullFace
#define glFrontFace l3d_glFrontFace
#define glLineWidth l3d_glLineWidth
#define glPolygonOffset l3d_glPolygonOffset
#define glPixelStorei l3d_glPixelStorei
#define glTexImage2D l3d_glTexImage2D
#define glTexParameteri l3d_glTexParameteri
#define glGenTextures l3d_glGenTextures
#define glBindTexture l3d_glBindTexture
#define glDeleteTextures l3d_glDeleteTextures
#define glActiveTexture l3d_glActiveTexture
#define glCreateShader l3d_glCreateShader
#define glShaderSource l3d_glShaderSource
#define glCompileShader l3d_glCompileShader
#define glGetShaderiv l3d_glGetShaderiv
#define glGetShaderInfoLog l3d_glGetShaderInfoLog
#define glDeleteShader l3d_glDeleteShader
#define glCreateProgram l3d_glCreateProgram
#define glAttachShader l3d_glAttachShader
#define glLinkProgram l3d_glLinkProgram
#define glGetProgramiv l3d_glGetProgramiv
#define glGetProgramInfoLog l3d_glGetProgramInfoLog
#define glUseProgram l3d_glUseProgram
#define glDeleteProgram l3d_glDeleteProgram
#define glGetUniformLocation l3d_glGetUniformLocation
#define glGetAttribLocation l3d_glGetAttribLocation
#define glUniform1i l3d_glUniform1i
#define glUniform1f l3d_glUniform1f
#define glUniform3f l3d_glUniform3f
#define glUniform4f l3d_glUniform4f
#define glUniformMatrix4fv l3d_glUniformMatrix4fv
#define glEnableVertexAttribArray l3d_glEnableVertexAttribArray
#define glDisableVertexAttribArray l3d_glDisableVertexAttribArray
#define glVertexAttribPointer l3d_glVertexAttribPointer
#define glGenBuffers l3d_glGenBuffers
#define glBindBuffer l3d_glBindBuffer
#define glBufferData l3d_glBufferData
#define glBufferSubData l3d_glBufferSubData
#define glDeleteBuffers l3d_glDeleteBuffers
#define glGenVertexArrays l3d_glGenVertexArrays
#define glBindVertexArray l3d_glBindVertexArray
#define glDeleteVertexArrays l3d_glDeleteVertexArrays
#define glReadPixels l3d_glReadPixels
#define glFinish l3d_glFinish

/* Load every entry point through getproc; returns 0 and fills `missing`
 * with the first missing name on failure. */
int glapi_load(void *(*getproc)(const char *name), const char **missing);

#endif
