#define GLAD_API_CALL
#include "gl.hpp"

#if defined(GEODE_IS_ANDROID)
#include <EGL/egl.h>
#endif

struct CastableFuncPtr {
    void* ptr;

    template <typename T>
    operator T() {
        return (T)ptr;
    }
};

static bool loaded = false;

inline void* glGetProcAddressRaw(const char* name) {
#if defined(GEODE_IS_ANDROID)
    return (void*)eglGetProcAddress(name);
#elif defined(GEODE_IS_WINDOWS)
    return (void*)wglGetProcAddress(name);
#else
    (void)name;
    return nullptr;
#endif
}

inline CastableFuncPtr glGetProcAddress(const char* name) {
    return (CastableFuncPtr)glGetProcAddressRaw(name);
}

bool hasLoadedNewerOpenGLFunctions() {
    return loaded;
}

void loadNewerOpenGLFunctions() {
    glMultiDrawElementsIndirect = glGetProcAddress("glMultiDrawElementsIndirect");
    glMultiDrawArraysIndirect = glGetProcAddress("glMultiDrawArraysIndirect");
    glDispatchCompute = glGetProcAddress("glDispatchCompute");
    glSpecializeShader = glGetProcAddress("glSpecializeShader");
    glGetProgramResourceIndex = glGetProcAddress("glGetProgramResourceIndex");
    glShaderStorageBlockBinding = glGetProcAddress("glShaderStorageBlockBinding");

    loaded = true;
}