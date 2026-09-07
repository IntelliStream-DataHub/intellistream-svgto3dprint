#include "glapi.h"

PFN_glEnable l3d_glEnable;
PFN_glDisable l3d_glDisable;
PFN_glBlendFunc l3d_glBlendFunc;
PFN_glBlendEquation l3d_glBlendEquation;
PFN_glViewport l3d_glViewport;
PFN_glScissor l3d_glScissor;
PFN_glClearColor l3d_glClearColor;
PFN_glClear l3d_glClear;
PFN_glDrawArrays l3d_glDrawArrays;
PFN_glDrawElements l3d_glDrawElements;
PFN_glGetError l3d_glGetError;
PFN_glGetIntegerv l3d_glGetIntegerv;
PFN_glGetString l3d_glGetString;
PFN_glDepthFunc l3d_glDepthFunc;
PFN_glDepthMask l3d_glDepthMask;
PFN_glCullFace l3d_glCullFace;
PFN_glFrontFace l3d_glFrontFace;
PFN_glLineWidth l3d_glLineWidth;
PFN_glPolygonOffset l3d_glPolygonOffset;
PFN_glPixelStorei l3d_glPixelStorei;
PFN_glTexImage2D l3d_glTexImage2D;
PFN_glTexParameteri l3d_glTexParameteri;
PFN_glGenTextures l3d_glGenTextures;
PFN_glBindTexture l3d_glBindTexture;
PFN_glDeleteTextures l3d_glDeleteTextures;
PFN_glActiveTexture l3d_glActiveTexture;
PFN_glCreateShader l3d_glCreateShader;
PFN_glShaderSource l3d_glShaderSource;
PFN_glCompileShader l3d_glCompileShader;
PFN_glGetShaderiv l3d_glGetShaderiv;
PFN_glGetShaderInfoLog l3d_glGetShaderInfoLog;
PFN_glDeleteShader l3d_glDeleteShader;
PFN_glCreateProgram l3d_glCreateProgram;
PFN_glAttachShader l3d_glAttachShader;
PFN_glLinkProgram l3d_glLinkProgram;
PFN_glGetProgramiv l3d_glGetProgramiv;
PFN_glGetProgramInfoLog l3d_glGetProgramInfoLog;
PFN_glUseProgram l3d_glUseProgram;
PFN_glDeleteProgram l3d_glDeleteProgram;
PFN_glGetUniformLocation l3d_glGetUniformLocation;
PFN_glGetAttribLocation l3d_glGetAttribLocation;
PFN_glBindAttribLocation l3d_glBindAttribLocation;
PFN_glUniform1i l3d_glUniform1i;
PFN_glUniform1f l3d_glUniform1f;
PFN_glUniform3f l3d_glUniform3f;
PFN_glUniform4f l3d_glUniform4f;
PFN_glUniformMatrix4fv l3d_glUniformMatrix4fv;
PFN_glEnableVertexAttribArray l3d_glEnableVertexAttribArray;
PFN_glDisableVertexAttribArray l3d_glDisableVertexAttribArray;
PFN_glVertexAttribPointer l3d_glVertexAttribPointer;
PFN_glGenBuffers l3d_glGenBuffers;
PFN_glBindBuffer l3d_glBindBuffer;
PFN_glBufferData l3d_glBufferData;
PFN_glBufferSubData l3d_glBufferSubData;
PFN_glDeleteBuffers l3d_glDeleteBuffers;
PFN_glGenVertexArrays l3d_glGenVertexArrays;
PFN_glBindVertexArray l3d_glBindVertexArray;
PFN_glDeleteVertexArrays l3d_glDeleteVertexArrays;
PFN_glReadPixels l3d_glReadPixels;
PFN_glFinish l3d_glFinish;

int glapi_load(void *(*getproc)(const char *name), const char **missing)
{
    if (missing) *missing = NULL;
    l3d_glEnable = (PFN_glEnable)getproc("glEnable");
    if (!l3d_glEnable) { if (missing) *missing = "glEnable"; return 0; }
    l3d_glDisable = (PFN_glDisable)getproc("glDisable");
    if (!l3d_glDisable) { if (missing) *missing = "glDisable"; return 0; }
    l3d_glBlendFunc = (PFN_glBlendFunc)getproc("glBlendFunc");
    if (!l3d_glBlendFunc) { if (missing) *missing = "glBlendFunc"; return 0; }
    l3d_glBlendEquation = (PFN_glBlendEquation)getproc("glBlendEquation");
    if (!l3d_glBlendEquation) { if (missing) *missing = "glBlendEquation"; return 0; }
    l3d_glViewport = (PFN_glViewport)getproc("glViewport");
    if (!l3d_glViewport) { if (missing) *missing = "glViewport"; return 0; }
    l3d_glScissor = (PFN_glScissor)getproc("glScissor");
    if (!l3d_glScissor) { if (missing) *missing = "glScissor"; return 0; }
    l3d_glClearColor = (PFN_glClearColor)getproc("glClearColor");
    if (!l3d_glClearColor) { if (missing) *missing = "glClearColor"; return 0; }
    l3d_glClear = (PFN_glClear)getproc("glClear");
    if (!l3d_glClear) { if (missing) *missing = "glClear"; return 0; }
    l3d_glDrawArrays = (PFN_glDrawArrays)getproc("glDrawArrays");
    if (!l3d_glDrawArrays) { if (missing) *missing = "glDrawArrays"; return 0; }
    l3d_glDrawElements = (PFN_glDrawElements)getproc("glDrawElements");
    if (!l3d_glDrawElements) { if (missing) *missing = "glDrawElements"; return 0; }
    l3d_glGetError = (PFN_glGetError)getproc("glGetError");
    if (!l3d_glGetError) { if (missing) *missing = "glGetError"; return 0; }
    l3d_glGetIntegerv = (PFN_glGetIntegerv)getproc("glGetIntegerv");
    if (!l3d_glGetIntegerv) { if (missing) *missing = "glGetIntegerv"; return 0; }
    l3d_glGetString = (PFN_glGetString)getproc("glGetString");
    if (!l3d_glGetString) { if (missing) *missing = "glGetString"; return 0; }
    l3d_glDepthFunc = (PFN_glDepthFunc)getproc("glDepthFunc");
    if (!l3d_glDepthFunc) { if (missing) *missing = "glDepthFunc"; return 0; }
    l3d_glDepthMask = (PFN_glDepthMask)getproc("glDepthMask");
    if (!l3d_glDepthMask) { if (missing) *missing = "glDepthMask"; return 0; }
    l3d_glCullFace = (PFN_glCullFace)getproc("glCullFace");
    if (!l3d_glCullFace) { if (missing) *missing = "glCullFace"; return 0; }
    l3d_glFrontFace = (PFN_glFrontFace)getproc("glFrontFace");
    if (!l3d_glFrontFace) { if (missing) *missing = "glFrontFace"; return 0; }
    l3d_glLineWidth = (PFN_glLineWidth)getproc("glLineWidth");
    if (!l3d_glLineWidth) { if (missing) *missing = "glLineWidth"; return 0; }
    l3d_glPolygonOffset = (PFN_glPolygonOffset)getproc("glPolygonOffset");
    if (!l3d_glPolygonOffset) { if (missing) *missing = "glPolygonOffset"; return 0; }
    l3d_glPixelStorei = (PFN_glPixelStorei)getproc("glPixelStorei");
    if (!l3d_glPixelStorei) { if (missing) *missing = "glPixelStorei"; return 0; }
    l3d_glTexImage2D = (PFN_glTexImage2D)getproc("glTexImage2D");
    if (!l3d_glTexImage2D) { if (missing) *missing = "glTexImage2D"; return 0; }
    l3d_glTexParameteri = (PFN_glTexParameteri)getproc("glTexParameteri");
    if (!l3d_glTexParameteri) { if (missing) *missing = "glTexParameteri"; return 0; }
    l3d_glGenTextures = (PFN_glGenTextures)getproc("glGenTextures");
    if (!l3d_glGenTextures) { if (missing) *missing = "glGenTextures"; return 0; }
    l3d_glBindTexture = (PFN_glBindTexture)getproc("glBindTexture");
    if (!l3d_glBindTexture) { if (missing) *missing = "glBindTexture"; return 0; }
    l3d_glDeleteTextures = (PFN_glDeleteTextures)getproc("glDeleteTextures");
    if (!l3d_glDeleteTextures) { if (missing) *missing = "glDeleteTextures"; return 0; }
    l3d_glActiveTexture = (PFN_glActiveTexture)getproc("glActiveTexture");
    if (!l3d_glActiveTexture) { if (missing) *missing = "glActiveTexture"; return 0; }
    l3d_glCreateShader = (PFN_glCreateShader)getproc("glCreateShader");
    if (!l3d_glCreateShader) { if (missing) *missing = "glCreateShader"; return 0; }
    l3d_glShaderSource = (PFN_glShaderSource)getproc("glShaderSource");
    if (!l3d_glShaderSource) { if (missing) *missing = "glShaderSource"; return 0; }
    l3d_glCompileShader = (PFN_glCompileShader)getproc("glCompileShader");
    if (!l3d_glCompileShader) { if (missing) *missing = "glCompileShader"; return 0; }
    l3d_glGetShaderiv = (PFN_glGetShaderiv)getproc("glGetShaderiv");
    if (!l3d_glGetShaderiv) { if (missing) *missing = "glGetShaderiv"; return 0; }
    l3d_glGetShaderInfoLog = (PFN_glGetShaderInfoLog)getproc("glGetShaderInfoLog");
    if (!l3d_glGetShaderInfoLog) { if (missing) *missing = "glGetShaderInfoLog"; return 0; }
    l3d_glDeleteShader = (PFN_glDeleteShader)getproc("glDeleteShader");
    if (!l3d_glDeleteShader) { if (missing) *missing = "glDeleteShader"; return 0; }
    l3d_glCreateProgram = (PFN_glCreateProgram)getproc("glCreateProgram");
    if (!l3d_glCreateProgram) { if (missing) *missing = "glCreateProgram"; return 0; }
    l3d_glAttachShader = (PFN_glAttachShader)getproc("glAttachShader");
    if (!l3d_glAttachShader) { if (missing) *missing = "glAttachShader"; return 0; }
    l3d_glLinkProgram = (PFN_glLinkProgram)getproc("glLinkProgram");
    if (!l3d_glLinkProgram) { if (missing) *missing = "glLinkProgram"; return 0; }
    l3d_glGetProgramiv = (PFN_glGetProgramiv)getproc("glGetProgramiv");
    if (!l3d_glGetProgramiv) { if (missing) *missing = "glGetProgramiv"; return 0; }
    l3d_glGetProgramInfoLog = (PFN_glGetProgramInfoLog)getproc("glGetProgramInfoLog");
    if (!l3d_glGetProgramInfoLog) { if (missing) *missing = "glGetProgramInfoLog"; return 0; }
    l3d_glUseProgram = (PFN_glUseProgram)getproc("glUseProgram");
    if (!l3d_glUseProgram) { if (missing) *missing = "glUseProgram"; return 0; }
    l3d_glDeleteProgram = (PFN_glDeleteProgram)getproc("glDeleteProgram");
    if (!l3d_glDeleteProgram) { if (missing) *missing = "glDeleteProgram"; return 0; }
    l3d_glGetUniformLocation = (PFN_glGetUniformLocation)getproc("glGetUniformLocation");
    if (!l3d_glGetUniformLocation) { if (missing) *missing = "glGetUniformLocation"; return 0; }
    l3d_glGetAttribLocation = (PFN_glGetAttribLocation)getproc("glGetAttribLocation");
    if (!l3d_glGetAttribLocation) { if (missing) *missing = "glGetAttribLocation"; return 0; }
    l3d_glBindAttribLocation = (PFN_glBindAttribLocation)getproc("glBindAttribLocation");
    if (!l3d_glBindAttribLocation) { if (missing) *missing = "glBindAttribLocation"; return 0; }
    l3d_glUniform1i = (PFN_glUniform1i)getproc("glUniform1i");
    if (!l3d_glUniform1i) { if (missing) *missing = "glUniform1i"; return 0; }
    l3d_glUniform1f = (PFN_glUniform1f)getproc("glUniform1f");
    if (!l3d_glUniform1f) { if (missing) *missing = "glUniform1f"; return 0; }
    l3d_glUniform3f = (PFN_glUniform3f)getproc("glUniform3f");
    if (!l3d_glUniform3f) { if (missing) *missing = "glUniform3f"; return 0; }
    l3d_glUniform4f = (PFN_glUniform4f)getproc("glUniform4f");
    if (!l3d_glUniform4f) { if (missing) *missing = "glUniform4f"; return 0; }
    l3d_glUniformMatrix4fv = (PFN_glUniformMatrix4fv)getproc("glUniformMatrix4fv");
    if (!l3d_glUniformMatrix4fv) { if (missing) *missing = "glUniformMatrix4fv"; return 0; }
    l3d_glEnableVertexAttribArray = (PFN_glEnableVertexAttribArray)getproc("glEnableVertexAttribArray");
    if (!l3d_glEnableVertexAttribArray) { if (missing) *missing = "glEnableVertexAttribArray"; return 0; }
    l3d_glDisableVertexAttribArray = (PFN_glDisableVertexAttribArray)getproc("glDisableVertexAttribArray");
    if (!l3d_glDisableVertexAttribArray) { if (missing) *missing = "glDisableVertexAttribArray"; return 0; }
    l3d_glVertexAttribPointer = (PFN_glVertexAttribPointer)getproc("glVertexAttribPointer");
    if (!l3d_glVertexAttribPointer) { if (missing) *missing = "glVertexAttribPointer"; return 0; }
    l3d_glGenBuffers = (PFN_glGenBuffers)getproc("glGenBuffers");
    if (!l3d_glGenBuffers) { if (missing) *missing = "glGenBuffers"; return 0; }
    l3d_glBindBuffer = (PFN_glBindBuffer)getproc("glBindBuffer");
    if (!l3d_glBindBuffer) { if (missing) *missing = "glBindBuffer"; return 0; }
    l3d_glBufferData = (PFN_glBufferData)getproc("glBufferData");
    if (!l3d_glBufferData) { if (missing) *missing = "glBufferData"; return 0; }
    l3d_glBufferSubData = (PFN_glBufferSubData)getproc("glBufferSubData");
    if (!l3d_glBufferSubData) { if (missing) *missing = "glBufferSubData"; return 0; }
    l3d_glDeleteBuffers = (PFN_glDeleteBuffers)getproc("glDeleteBuffers");
    if (!l3d_glDeleteBuffers) { if (missing) *missing = "glDeleteBuffers"; return 0; }
    l3d_glGenVertexArrays = (PFN_glGenVertexArrays)getproc("glGenVertexArrays");
    if (!l3d_glGenVertexArrays) { if (missing) *missing = "glGenVertexArrays"; return 0; }
    l3d_glBindVertexArray = (PFN_glBindVertexArray)getproc("glBindVertexArray");
    if (!l3d_glBindVertexArray) { if (missing) *missing = "glBindVertexArray"; return 0; }
    l3d_glDeleteVertexArrays = (PFN_glDeleteVertexArrays)getproc("glDeleteVertexArrays");
    if (!l3d_glDeleteVertexArrays) { if (missing) *missing = "glDeleteVertexArrays"; return 0; }
    l3d_glReadPixels = (PFN_glReadPixels)getproc("glReadPixels");
    if (!l3d_glReadPixels) { if (missing) *missing = "glReadPixels"; return 0; }
    l3d_glFinish = (PFN_glFinish)getproc("glFinish");
    if (!l3d_glFinish) { if (missing) *missing = "glFinish"; return 0; }
    return 1;
}
