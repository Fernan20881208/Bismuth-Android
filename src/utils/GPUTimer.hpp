#pragma once

#include <common.hpp>
#include <profiler.hpp>

class GPUTimer {
public:
    inline GPUTimer(const char* name)
        : name(name)
    {
        glGenQueries(1, &queryObject);
        profiler::addTimer(this);
    }

    inline ~GPUTimer() {
        glDeleteQueries(1, &queryObject);
        profiler::removeTimer(this);
    }

    inline void start() {
        if (profiler::isTakingGPUTime)
            glBeginQuery(GL_TIME_ELAPSED, queryObject);
    }

    inline void stop() {
        if (profiler::isTakingGPUTime) {
            glEndQuery(GL_TIME_ELAPSED);
            glGetQueryObjecti64v(queryObject, GL_QUERY_RESULT, &time);
        }
    }

    inline const char* getName() const { return name; }
    inline i64 getTime() const { return time; }

private:
    i64 time;
    const char* name;
    u32 queryObject;
};