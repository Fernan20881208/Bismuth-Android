#pragma once

#include <Geode/cocos/platform/CCGL.h>
#if defined(GEODE_IS_ANDROID)
#include <GLES3/gl31.h>
#endif

/*
    This file adds the required OpenGL 4.3 functions as
    the GLEW in Cocos2d only supports up to OpenGL 4.2
*/

#ifndef GL_BUFFER
#define GL_BUFFER 0x82E0
#endif
#ifndef GL_SHADER
#define GL_SHADER 0x82E1
#endif
#ifndef GL_PROGRAM
#define GL_PROGRAM 0x82E2
#endif
#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V
#define GL_SHADER_BINARY_FORMAT_SPIR_V 0x9551
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_SHADER_STORAGE_BLOCK
#define GL_SHADER_STORAGE_BLOCK 0x92E6
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER 0x8A11
#endif
#ifndef GL_DYNAMIC_COPY
#define GL_DYNAMIC_COPY 0x88EA
#endif
#ifndef GL_READ_WRITE
#define GL_READ_WRITE 0x88BA
#endif

#if defined(GEODE_IS_ANDROID)
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
// Timer queries are optional on OpenGL ES. Keep the profiling hooks harmless
// until EXT_disjoint_timer_query is implemented and detected.
#define glBeginQuery(target, id) ((void)0)
#define glEndQuery(target) ((void)0)
#define glGetQueryObjecti64v(id, pname, params) (*(params) = 0)
#endif

#ifndef GLAD_API_CALL
#define GLAD_API_CALL extern
#endif

typedef void (*PFNGLMULTIDRAWELEMENTSINDIRECTPROC)(GLenum mode, GLenum type, const void * indirect, GLsizei drawcount, GLsizei stride);
typedef void (*PFNGLMULTIDRAWARRAYSINDIRECTPROC)(GLenum mode, const void * indirect, GLsizei drawcount, GLsizei stride);
typedef void (*PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (*PFNGLSPECIALIZESHADERPROC)(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue);
typedef GLuint (*PFNGLGETPROGRAMRESOURCEINDEXPROC)(GLuint program, GLenum programInterface, const GLchar * name);
typedef void (*PFNGLSHADERSTORAGEBLOCKBINDINGPROC)(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);

GLAD_API_CALL PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_glMultiDrawElementsIndirect;
#define glMultiDrawElementsIndirect glad_glMultiDrawElementsIndirect
GLAD_API_CALL PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect;
#define glMultiDrawArraysIndirect glad_glMultiDrawArraysIndirect
GLAD_API_CALL PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
#define glDispatchCompute glad_glDispatchCompute
GLAD_API_CALL PFNGLSPECIALIZESHADERPROC glad_glSpecializeShader;
#define glSpecializeShader glad_glSpecializeShader
GLAD_API_CALL PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex;
#define glGetProgramResourceIndex glad_glGetProgramResourceIndex
GLAD_API_CALL PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_glShaderStorageBlockBinding;
#define glShaderStorageBlockBinding glad_glShaderStorageBlockBinding

bool hasLoadedNewerOpenGLFunctions();
void loadNewerOpenGLFunctions();