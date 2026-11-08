// 32-bit pointer slot for the 64-bit port.
//
// GTA2 keeps object pointers inside data whose layout comes from files: script commands
// (SCR_POINTER and friends live in the .scr byte stream), sprite/delta indexes from .sty,
// GXT key tables. Those slots are 4 bytes wide. ptr32<T> keeps them 4 bytes wide by storing
// a handle into a process-wide table instead of the pointer itself.
//
// - Handle 0 is NULL, so zero-filled data and "is it set?" tests keep working.
// - The type is trivial (no constructors), so it can live in unions and be memcpy'd.
// - raw()/set_raw() expose the stored 32-bit value for the places that keep a file offset
//   in the slot before converting it to a pointer.
#pragma once

#include <stddef.h>
#include <stdint.h>

extern void** gPortHandleTable;
extern uint32_t gPortHandleCount;
uint32_t port_ptr_to_handle(const void* p);

static inline void* port_handle_to_ptr(uint32_t h)
{
    return h < gPortHandleCount ? gPortHandleTable[h] : NULL;
}

template <typename T> struct ptr32
{
    uint32_t mHandle;

    T* get() const
    {
        return static_cast<T*>(port_handle_to_ptr(mHandle));
    }
    operator T*() const
    {
        return get();
    }
    T* operator->() const
    {
        return get();
    }
    template <typename U = T> U& operator*() const
    {
        return *get();
    }
    template <typename I> T& operator[](I idx) const
    {
        return get()[idx];
    }
    ptr32& operator=(T* p)
    {
        mHandle = port_ptr_to_handle(p);
        return *this;
    }
    ptr32& operator=(decltype(nullptr))
    {
        mHandle = 0;
        return *this;
    }
    // "= NULL" / "= 0" in the original code
    ptr32& operator=(int zero)
    {
        mHandle = 0;
        (void)zero;
        return *this;
    }
    ptr32& operator=(long zero)
    {
        mHandle = 0;
        (void)zero;
        return *this;
    }
    uint32_t raw() const
    {
        return mHandle;
    }
    void set_raw(uint32_t v)
    {
        mHandle = v;
    }
};
static_assert(sizeof(ptr32<int>) == 4, "ptr32 must stay 4 bytes");
