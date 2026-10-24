#pragma once

#include "Buffer.hpp"

template <typename T>
struct RefLikePtr {
    T* ptr;

    operator T() const {
        return *ptr;
    }

    T& operator=(const T& value) {
        *ptr = value;
        return *ptr;
    }
};

#define __SAO_VARS_AS_TEMPLATE_ARGS(I, B, T, N) , T
#define __SAO_VARS_AS_ENTRY_MEMBERS(I, B, T, N) RefLikePtr<T> N;
#define __SAO_VARS_AS_ARRAY_ENTRY(I, B, T, N) B,
#define __SAO_VARS_AS_NAME_ENTRY(I, B, T, N) #N,
#define __SAO_VARS_COUNT(I, B, T, N) + 1

#define SAO_BUFFER(Vars_, Name_) \
    struct Name_ { \
        constexpr static const char* __name = #Name_; \
        constexpr static u32 __bindingIds[] = {Vars_(, __SAO_VARS_AS_ARRAY_ENTRY)}; \
        constexpr static const char* __varNames[] = {Vars_(, __SAO_VARS_AS_NAME_ENTRY)}; \
        using Buffer = SAOBuffer<Name_ Vars_(, __SAO_VARS_AS_TEMPLATE_ARGS)>; \
        union { \
            u8* __variables[0 Vars_(, __SAO_VARS_COUNT)]; \
            struct { \
                Vars_(, __SAO_VARS_AS_ENTRY_MEMBERS) \
            }; \
        }; \
    };

template <typename Entry_, typename ...Vars_>
class SAOBuffer;

#define TOTAL_SPRITE_COUNT 0

#include "../../resources/shaders/headers/shared.h"

/*
    This allows for better organization of struct-of-arrays GPU buffers.
*/
template <typename Entry_, typename ...Vars_>
class SAOBuffer {
public:
    Entry_ at(usize index) {
        assert(mappedData);
        Entry_ entry;
        fillEntryRecursive<Vars_...>(entry, 0, index);
        return entry;
    }

    Entry_ operator[](size_t index) { return at(index); }

    void bind() {
        bindBuffersRecursive<Vars_...>(0);
    }

    void flushAll() {
        for (usize i = 0; i < sizeof...(Vars_); i++)
            buffers[i]->flush(0, buffers[i]->getSize());
    }

    void flushAllAndUnmap() {
        flushAll();
        for (usize i = 0; i < sizeof...(Vars_); i++) {
            buffers[i]->unmap();
            mappedBuffer[i] = nullptr;
        }
    }

    static UPtr<SAOBuffer<Entry_, Vars_...>> create(usize entryCount, GLenum usage, bool shouldMap = true) {
        auto ret = std::make_unique<SAOBuffer<Entry_, Vars_...>>();
        if (ret->init(entryCount, usage, shouldMap))
            return ret;
        else
            return nullptr;
    }

private:
    /*
    template <typename Var_>
    usize addVariable(usize index) {

        variableOffsets[index] = offset;
        return offset + sizeof(Var_) * entryCount;
    }

    template <typename Var_, typename ...OtherVars_>
    usize addVariables(usize index, usize offset) {
        offset = addVariable<Var_>(index, offset);
        if constexpr (sizeof...(OtherVars_))
            offset = addVariables<OtherVars_...>(index + 1, offset);
        return offset;
    }
    */

    template <typename Var_, typename ...OtherVars_>
    bool createBuffersRecursive(usize index, bool shouldMap) {
        usize size = sizeof(Var_) * entryCount;
        std::string name = std::string(Entry_::__name) + "_" + Entry_::__varNames[index];
        buffers[index] = Buffer::create(name.c_str(), size, usage);
        if (!buffers[index])
            return false;

        if (shouldMap)
            mappedBuffer[index] = (u8*)buffers[index]->mapReadWrite();

        if constexpr (sizeof...(OtherVars_) > 0)
            return createBuffersRecursive<OtherVars_...>(index + 1, shouldMap);
        return true;
    }

    bool init(usize entryCount, GLenum usage, bool shouldMap) {
        this->entryCount = entryCount;
        this->usage = usage;
        return createBuffersRecursive<Vars_...>(0, shouldMap);
    }

    template <typename Var_, typename ...OtherVars_>
    void fillEntryRecursive(Entry_& entry, usize varIndex, usize dataIndex) {
        entry.__variables[varIndex] = mappedBuffer[varIndex] + sizeof(Var_) * dataIndex;

        if constexpr (sizeof...(OtherVars_) > 0)
            fillEntryRecursive<OtherVars_...>(entry, varIndex + 1, dataIndex);
    }

    template <typename Var_, typename ...OtherVars_>
    void bindBuffersRecursive(usize varIndex) {
        buffers[varIndex]->bindAsStorageBuffer(Entry_::__bindingIds[varIndex]);

        if constexpr (sizeof...(OtherVars_) > 0)
            bindBuffersRecursive<OtherVars_...>(varIndex + 1);
    }

private:
    GLenum usage;
    UPtr<Buffer> buffers[sizeof...(Vars_)];
    u8* mappedBuffer[sizeof...(Vars_)] = {nullptr};
    usize entryCount;
};