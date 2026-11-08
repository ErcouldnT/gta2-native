// Force-included into every translation unit of the native (clang, 64-bit) port.
// Provides the MSVC/Win32 vocabulary the decompiled sources expect.
#pragma once
// Files that must not see the Win32/MSVC vocabulary (Objective-C++ glue) define PORT_NO_PRELUDE
#ifndef PORT_NO_PRELUDE

#ifndef GTA2_PORT
#define GTA2_PORT 1
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <wchar.h>

// Calling conventions are meaningless on arm64/x86_64 SysV.
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#define _stdcall
#define _cdecl
#define __forceinline inline __attribute__((always_inline))
#define __debugbreak() __builtin_trap()
#ifdef __cplusplus
// Dynamic exception specifications (throw()) are gone in C++17; MSVC 6 used them freely
// and didn't require declarations and definitions to agree.
#define throw() noexcept_unused_marker
#define noexcept_unused_marker
#endif
#define _alloca __builtin_alloca
#define __declspec(x)
#define __pascal
#define _export
#define __export
#define cdecl
// Miles Sound System header: pick its Win32 branch and keep its integer types 32-bit.
#ifndef __NT__
#define __NT__ 1
#endif
#define U32 uint32_t
#define S32 int32_t
#define STATIC_ASSERT(x, msg) static_assert(x, msg)

#ifndef __int8
#define __int8 char
#endif
#ifndef __int16
#define __int16 short
#endif
#ifndef __int32
#define __int32 int
#endif
#ifndef __int64
#define __int64 long long
#endif

#define _MAX_PATH 260
#define MAX_PATH 260

#ifdef __cplusplus
extern "C" {
#endif

// All game file access goes through these: '\\' separators, paths relative to the game
// root, case-insensitive lookup (iOS file systems are case-sensitive).
FILE* port_fopen(const char* path, const char* mode);
const char* port_resolve_path(const char* path, char* out, size_t outSize);
int port_remove(const char* path);
#ifndef PORT_PLATFORM_CODE
#define fopen port_fopen
#endif

// MSVC CRT names
#define _stricmp strcasecmp
#define stricmp strcasecmp
#define strcmpi strcasecmp
#define _strcmpi strcasecmp
#define _strnicmp strncasecmp
#define strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _strdup strdup
#define _access access
#define _unlink unlink
#define _mkdir(p) mkdir((p), 0755)
#define _getcwd getcwd
#define _chdir chdir
#define _fileno fileno
#define _close close
#define _open open
#define _read read
#define _write write
#define _lseek lseek

char* port_strupr(char* s);
char* port_strlwr(char* s);
char* port_itoa(int value, char* str, int radix);
char* port_ltoa(long value, char* str, int radix);
char* port_ultoa(unsigned long value, char* str, int radix);
#define _strupr port_strupr
#define strupr port_strupr
#define _strlwr port_strlwr
#define strlwr port_strlwr
#define _itoa port_itoa
#define itoa port_itoa
#define _ltoa port_ltoa
#define _ultoa port_ultoa

long port_filelength(int fd);
unsigned long long port_cycle_counter(void);
// Renderer x offset (widescreen HUD centring)
void port_set_draw_offset(float x);
#define _filelength port_filelength
#define filelength port_filelength

// ---------------------------------------------------------------------------
// Wide strings. The game keeps UTF-16 text (GXT files) in memory as wchar_t,
// so the port is built with -fshort-wchar and every wide CRT function used by
// the sources is re-implemented here for 16-bit wchar_t.
// ---------------------------------------------------------------------------
size_t port_wcslen(const wchar_t* s);
wchar_t* port_wcscpy(wchar_t* d, const wchar_t* s);
wchar_t* port_wcsncpy(wchar_t* d, const wchar_t* s, size_t n);
wchar_t* port_wcscat(wchar_t* d, const wchar_t* s);
wchar_t* port_wcsncat(wchar_t* d, const wchar_t* s, size_t n);
int port_wcscmp(const wchar_t* a, const wchar_t* b);
int port_wcsncmp(const wchar_t* a, const wchar_t* b, size_t n);
int port_wcsicmp(const wchar_t* a, const wchar_t* b);
wchar_t* port_wcschr(const wchar_t* s, wchar_t c);
wchar_t* port_wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* port_wcsstr(const wchar_t* s, const wchar_t* n);
int port_swprintf(wchar_t* buf, const wchar_t* fmt, ...);
int port_vswprintf(wchar_t* buf, const wchar_t* fmt, va_list ap);
int port_snwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...);
wchar_t* port_itow(int value, wchar_t* str, int radix);
int port_wtoi(const wchar_t* s);
long port_wcstol(const wchar_t* s, wchar_t** end, int base);
wchar_t port_towupper(wchar_t c);
wchar_t port_towlower(wchar_t c);
int port_iswspace(wchar_t c);
int port_iswdigit(wchar_t c);
size_t port_mbstowcs(wchar_t* d, const char* s, size_t n);
size_t port_wcstombs(char* d, const wchar_t* s, size_t n);
int port_wprintf(const wchar_t* fmt, ...);

#define wcslen port_wcslen
#define wcscpy port_wcscpy
#define wcsncpy port_wcsncpy
#define wcscat port_wcscat
#define wcsncat port_wcsncat
#define wcscmp port_wcscmp
#define wcsncmp port_wcsncmp
#define _wcsicmp port_wcsicmp
#define wcsicmp port_wcsicmp
#define wcschr port_wcschr
#define wcsrchr port_wcsrchr
#define wcsstr port_wcsstr
#define swprintf port_swprintf
#define vswprintf port_vswprintf
#define _snwprintf port_snwprintf
#define _itow port_itow
#define _wtoi port_wtoi
#define wcstol port_wcstol
#define towupper port_towupper
#define towlower port_towlower
#define iswspace port_iswspace
#define iswdigit port_iswdigit
#define mbstowcs port_mbstowcs
#define wcstombs port_wcstombs
#define wprintf port_wprintf

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <algorithm>
// MSVC binds temporaries to non-const lvalue references; PORT_LV() gives the same behaviour at a call
// site (the temporary lives until the end of the full expression).
template <typename T> static inline T& port_lv(T&& t) { return t; }
#define PORT_LV(x) port_lv(x)
// MSVC's min/max macros from windows.h are used as plain functions in places.
template <typename A, typename B> static inline auto port_min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <typename A, typename B> static inline auto port_max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
#endif

#endif // PORT_NO_PRELUDE
