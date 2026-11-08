#pragma once
#include "windows.h"
#include <fcntl.h>
// MSVC _findfirst family on top of opendir/fnmatch (implemented in win32_shim.cpp)
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH 0x20
struct _finddata_t
{
    unsigned attrib;
    long time_create;
    long time_access;
    long time_write;
    unsigned long size;
    char name[260];
};
#ifdef __cplusplus
extern "C" {
#endif
intptr_t port_findfirst(const char* spec, struct _finddata_t* data);
int port_findnext(intptr_t handle, struct _finddata_t* data);
int port_findclose(intptr_t handle);
#ifdef __cplusplus
}
#endif
#define _findfirst port_findfirst
#define _findnext port_findnext
#define _findclose port_findclose
#define O_BINARY 0
#define _O_BINARY 0
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
