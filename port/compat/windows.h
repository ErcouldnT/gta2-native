// Minimal Win32 surface for the native port. Types keep their Win32 (ILP32/LLP64) sizes
// where game structures depend on them: DWORD/LONG are 32-bit even on LP64.
#pragma once

#include "port_prelude.h"
#include <unistd.h>
#include <sys/stat.h>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned short WORD;
typedef unsigned short USHORT;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int32_t INT;
typedef int16_t SHORT;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef float FLOAT;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint64_t DWORDLONG;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef void VOID;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef BOOL* LPBOOL;
typedef int* LPINT;
typedef char* LPSTR;
typedef char* PSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef char TCHAR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef LONG HRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef WORD ATOM;
typedef DWORD COLORREF;

#define DECLARE_HANDLE(name) typedef struct name##__* name
typedef void* HANDLE;
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HGDIOBJ_);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HACCEL);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HMONITOR);
typedef void* HGDIOBJ;
typedef HICON HCURSOR;
typedef HINSTANCE HMODULE;
typedef HKEY* PHKEY;
typedef HANDLE* LPHANDLE;
typedef int (*FARPROC)();
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define far
#define near
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#define STDMETHODCALLTYPE
#define WINAPIV

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEA MAKEINTRESOURCE

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))

#define ZeroMemory(d, l) memset((d), 0, (l))
#define FillMemory(d, l, f) memset((d), (f), (l))
#define CopyMemory(d, s, l) memcpy((d), (s), (l))
#define MoveMemory(d, s, l) memmove((d), (s), (l))

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *LPPOINT, *PPOINT;

typedef struct tagSIZE
{
    LONG cx;
    LONG cy;
} SIZE, *LPSIZE;

typedef struct tagRECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT, *PRECT;
typedef const RECT* LPCRECT;

typedef struct tagMSG
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *LPMSG;

typedef struct _GUID
{
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
} GUID;
typedef GUID IID;
typedef GUID CLSID;
typedef const GUID& REFGUID;
typedef const GUID& REFIID;
typedef const GUID& REFCLSID;
typedef GUID* LPGUID;
typedef const GUID* LPCGUID;
#ifdef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern "C" const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) extern "C" const GUID name
#endif
static inline bool operator==(const GUID& a, const GUID& b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
static inline bool operator!=(const GUID& a, const GUID& b) { return !(a == b); }
#define IsEqualGUID(a, b) ((a) == (b))

typedef union _LARGE_INTEGER
{
    struct
    {
        DWORD LowPart;
        LONG HighPart;
    };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _SYSTEMTIME
{
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

typedef struct _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef struct _OSVERSIONINFOA
{
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR szCSDVersion[128];
} OSVERSIONINFOA, OSVERSIONINFO, *LPOSVERSIONINFOA, *LPOSVERSIONINFO;
#define VER_PLATFORM_WIN32s 0
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2

typedef struct _PALETTEENTRY
{
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef struct tagRGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagWNDCLASSA
{
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, WNDCLASS, *LPWNDCLASSA;

typedef struct tagWNDCLASSEXA
{
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
    HICON hIconSm;
} WNDCLASSEXA, WNDCLASSEX;

typedef struct _SECURITY_ATTRIBUTES
{
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _STARTUPINFOA
{
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA, STARTUPINFO, *LPSTARTUPINFOA;

typedef struct _PROCESS_INFORMATION
{
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _WIN32_FIND_DATAA
{
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *LPWIN32_FIND_DATAA;

typedef struct _RTL_CRITICAL_SECTION
{
    void* impl;
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

// Registry
#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)0x80000000)
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)0x80000002)
#define HKEY_USERS ((HKEY)(ULONG_PTR)0x80000003)
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
#define REG_OPTION_NON_VOLATILE 0
#define REG_CREATED_NEW_KEY 1
#define REG_OPENED_EXISTING_KEY 2
#define KEY_QUERY_VALUE 0x0001
#define KEY_SET_VALUE 0x0002
#define KEY_ALL_ACCESS 0xF003F
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_MORE_DATA 234L
#define ERROR_NO_MORE_ITEMS 259L
#define ERROR_ALREADY_EXISTS 183L
typedef DWORD REGSAM;

// MessageBox
#define MB_OK 0x0
#define MB_OKCANCEL 0x1
#define MB_YESNO 0x4
#define MB_ICONHAND 0x10
#define MB_ICONERROR 0x10
#define MB_ICONSTOP 0x10
#define MB_ICONQUESTION 0x20
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONWARNING 0x30
#define MB_ICONINFORMATION 0x40
#define MB_SYSTEMMODAL 0x1000
#define MB_TASKMODAL 0x2000
#define MB_TOPMOST 0x40000
#define MB_SETFOREGROUND 0x10000
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

// Window messages and styles
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_GETMINMAXINFO 0x0024
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_SYSCOMMAND 0x0112
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_ENTERMENULOOP 0x0211
#define WM_EXITMENULOOP 0x0212
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define WM_USER 0x0400
#define WM_NCACTIVATE 0x0086
#define WM_NCPAINT 0x0085
#define WM_TIMER 0x0113
#define WM_COMMAND 0x0111
#define WM_POWERBROADCAST 0x0218
#define WM_DISPLAYCHANGE 0x007E
#define WM_INITDIALOG 0x0110
#define WM_MOVING 0x0216
#define WM_SIZING 0x0214
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NCHITTEST 0x0084
#define WM_SHOWWINDOW 0x0018
#define SC_SCREENSAVE 0xF140
#define SC_MONITORPOWER 0xF170
#define SC_KEYMENU 0xF100
#define SC_CLOSE 0xF060
#define SC_MAXIMIZE 0xF030
#define SC_MINIMIZE 0xF020
#define SC_RESTORE 0xF120
#define SC_MOVE 0xF010
#define SC_SIZE 0xF000
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#define SIZE_MINIMIZED 1
#define SIZE_RESTORED 0
#define SIZE_MAXIMIZED 2
#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_APPWINDOW 0x00040000L
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_OWNDC 0x0020
#define CS_DBLCLKS 0x0008
#define CW_USEDEFAULT ((int)0x80000000)
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWL_WNDPROC (-4)
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040
#define SWP_FRAMECHANGED 0x0020
#define HWND_TOP ((HWND)0)
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define BLACK_BRUSH 4
#define VK_ESCAPE 0x1B
#define VK_RETURN 0x0D
#define VK_F4 0x73
#define VK_F10 0x79
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_TAB 0x09
#define VK_SPACE 0x20
#define VK_CAPITAL 0x14
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define HORZRES 8
#define VERTRES 10
#define BITSPIXEL 12
#define PLANES 14
#define NUMCOLORS 24
#define RASTERCAPS 38
#define RC_PALETTE 0x0100

#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
#define DRIVE_REMOTE 4
#define DRIVE_CDROM 5
#define DRIVE_RAMDISK 6

#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define FILE_SHARE_READ 0x1
#define FILE_SHARE_WRITE 0x2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_READONLY 0x1
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define NORMAL_PRIORITY_CLASS 0x20
#define HIGH_PRIORITY_CLASS 0x80
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_HIGHEST 2
#define CP_ACP 0
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READWRITE 0x40
#define TIMERR_NOERROR 0
#define TIME_PERIODIC 1
#define LANG_NEUTRAL 0x00
#define LANG_ENGLISH 0x09
#define LANG_JAPANESE 0x11
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid) ((WORD)(lgid) >> 10)
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))

typedef struct _MEMORYSTATUS_REAL
{
    DWORD dwLength;
    DWORD dwMemoryLoad;
    size_t dwTotalPhys;
    size_t dwAvailPhys;
    size_t dwTotalPageFile;
    size_t dwAvailPageFile;
    size_t dwTotalVirtual;
    size_t dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

// ----------------------------------------------------------------------------
// API (implemented in port/platform/win32_shim.cpp)
// ----------------------------------------------------------------------------
DWORD timeGetTime(void);
DWORD GetTickCount(void);
UINT timeBeginPeriod(UINT);
UINT timeEndPeriod(UINT);
void Sleep(DWORD ms);
BOOL QueryPerformanceCounter(LARGE_INTEGER* p);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* p);
void GetLocalTime(LPSYSTEMTIME);
void GetSystemTime(LPSYSTEMTIME);
DWORD GetLastError(void);
void SetLastError(DWORD);
void OutputDebugStringA(LPCSTR s);
#define OutputDebugString OutputDebugStringA
int MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA
void ExitProcess(UINT code);
BOOL GetVersionExA(LPOSVERSIONINFOA);
#define GetVersionEx GetVersionExA
DWORD GetVersion(void);
void GlobalMemoryStatus(LPMEMORYSTATUS);

HMODULE LoadLibraryA(LPCSTR);
#define LoadLibrary LoadLibraryA
FARPROC GetProcAddress(HMODULE, LPCSTR);
BOOL FreeLibrary(HMODULE);
HMODULE GetModuleHandleA(LPCSTR);
#define GetModuleHandle GetModuleHandleA
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
#define GetModuleFileName GetModuleFileNameA

DWORD GetCurrentDirectoryA(DWORD, LPSTR);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR);
#define SetCurrentDirectory SetCurrentDirectoryA
UINT GetDriveTypeA(LPCSTR);
#define GetDriveType GetDriveTypeA
DWORD GetLogicalDrives(void);
DWORD GetLogicalDriveStringsA(DWORD, LPSTR);
BOOL GetVolumeInformationA(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD);
#define GetVolumeInformation GetVolumeInformationA
DWORD GetFileAttributesA(LPCSTR);
#define GetFileAttributes GetFileAttributesA
BOOL SetFileAttributesA(LPCSTR, DWORD);
#define SetFileAttributes SetFileAttributesA
BOOL CreateDirectoryA(LPCSTR, LPSECURITY_ATTRIBUTES);
#define CreateDirectory CreateDirectoryA
BOOL DeleteFileA(LPCSTR);
#define DeleteFile DeleteFileA
HANDLE FindFirstFileA(LPCSTR, LPWIN32_FIND_DATAA);
#define FindFirstFile FindFirstFileA
BOOL FindNextFileA(HANDLE, LPWIN32_FIND_DATAA);
#define FindNextFile FindNextFileA
BOOL FindClose(HANDLE);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
#define CreateFile CreateFileA
BOOL ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, void*);
BOOL WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, void*);
DWORD GetFileSize(HANDLE, LPDWORD);
DWORD SetFilePointer(HANDLE, LONG, PLONG, DWORD);
BOOL CloseHandle(HANDLE);
UINT GetWindowsDirectoryA(LPSTR, UINT);
UINT GetSystemDirectoryA(LPSTR, UINT);
DWORD GetTempPathA(DWORD, LPSTR);
DWORD GetPrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
UINT GetPrivateProfileIntA(LPCSTR, LPCSTR, int, LPCSTR);
BOOL WritePrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR, LPCSTR);
#define GetPrivateProfileString GetPrivateProfileStringA
#define GetPrivateProfileInt GetPrivateProfileIntA
#define WritePrivateProfileString WritePrivateProfileStringA

LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
LONG RegOpenKeyA(HKEY, LPCSTR, PHKEY);
LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
LONG RegCreateKeyA(HKEY, LPCSTR, PHKEY);
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD);
LONG RegDeleteValueA(HKEY, LPCSTR);
LONG RegDeleteKeyA(HKEY, LPCSTR);
LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, FILETIME*);
LONG RegEnumValueA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegCloseKey(HKEY);
LONG RegFlushKey(HKEY);
#define RegOpenKeyEx RegOpenKeyExA
#define RegOpenKey RegOpenKeyA
#define RegCreateKeyEx RegCreateKeyExA
#define RegCreateKey RegCreateKeyA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx RegSetValueExA
#define RegDeleteValue RegDeleteValueA
#define RegDeleteKey RegDeleteKeyA
#define RegEnumKeyEx RegEnumKeyExA
#define RegEnumValue RegEnumValueA

// Windowing (mostly no-ops: the SDL platform layer owns the real window)
BOOL PeekMessageA(LPMSG, HWND, UINT, UINT, UINT);
#define PeekMessage PeekMessageA
BOOL GetMessageA(LPMSG, HWND, UINT, UINT);
#define GetMessage GetMessageA
BOOL TranslateMessage(const MSG*);
LRESULT DispatchMessageA(const MSG*);
#define DispatchMessage DispatchMessageA
BOOL PostMessageA(HWND, UINT, WPARAM, LPARAM);
#define PostMessage PostMessageA
LRESULT SendMessageA(HWND, UINT, WPARAM, LPARAM);
#define SendMessage SendMessageA
void PostQuitMessage(int);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
#define DefWindowProc DefWindowProcA
ATOM RegisterClassA(const WNDCLASSA*);
#define RegisterClass RegisterClassA
ATOM RegisterClassExA(const WNDCLASSEXA*);
#define RegisterClassEx RegisterClassExA
BOOL UnregisterClassA(LPCSTR, HINSTANCE);
#define UnregisterClass UnregisterClassA
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
#define CreateWindowEx CreateWindowExA
#define CreateWindowA(cls, name, style, x, y, w, h, parent, menu, inst, param) \
    CreateWindowExA(0, cls, name, style, x, y, w, h, parent, menu, inst, param)
#define CreateWindow CreateWindowA
BOOL DestroyWindow(HWND);
BOOL ShowWindow(HWND, int);
BOOL UpdateWindow(HWND);
HWND SetFocus(HWND);
HWND GetFocus(void);
HWND GetActiveWindow(void);
HWND SetActiveWindow(HWND);
BOOL SetForegroundWindow(HWND);
HWND GetForegroundWindow(void);
HWND FindWindowA(LPCSTR, LPCSTR);
#define FindWindow FindWindowA
BOOL IsWindow(HWND);
BOOL IsIconic(HWND);
BOOL IsWindowVisible(HWND);
BOOL GetClientRect(HWND, LPRECT);
BOOL GetWindowRect(HWND, LPRECT);
BOOL ClientToScreen(HWND, LPPOINT);
BOOL ScreenToClient(HWND, LPPOINT);
BOOL AdjustWindowRect(LPRECT, DWORD, BOOL);
BOOL AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL MoveWindow(HWND, int, int, int, int, BOOL);
LONG GetWindowLongA(HWND, int);
LONG SetWindowLongA(HWND, int, LONG);
#define GetWindowLong GetWindowLongA
#define SetWindowLong SetWindowLongA
BOOL SetWindowTextA(HWND, LPCSTR);
#define SetWindowText SetWindowTextA
int GetSystemMetrics(int);
int ShowCursor(BOOL);
HCURSOR SetCursor(HCURSOR);
BOOL SetCursorPos(int, int);
BOOL GetCursorPos(LPPOINT);
BOOL ClipCursor(const RECT*);
HCURSOR LoadCursorA(HINSTANCE, LPCSTR);
#define LoadCursor LoadCursorA
HICON LoadIconA(HINSTANCE, LPCSTR);
#define LoadIcon LoadIconA
HGDIOBJ GetStockObject(int);
BOOL InvalidateRect(HWND, const RECT*, BOOL);
BOOL ValidateRect(HWND, const RECT*);
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
int GetDeviceCaps(HDC, int);
BOOL SetRect(LPRECT, int, int, int, int);
BOOL PtInRect(const RECT*, POINT);
SHORT GetAsyncKeyState(int);
SHORT GetKeyState(int);
BOOL GetKeyboardState(PBYTE);
UINT MapVirtualKeyA(UINT, UINT);
#define MapVirtualKey MapVirtualKeyA
int GetKeyNameTextA(LONG, LPSTR, int);
#define GetKeyNameText GetKeyNameTextA
WORD GetUserDefaultLangID(void);
WORD GetSystemDefaultLangID(void);
UINT GetACP(void);
int MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int);
int WideCharToMultiByte(UINT, DWORD, LPCWSTR, int, LPSTR, int, LPCSTR, LPBOOL);
BOOL SystemParametersInfoA(UINT, UINT, PVOID, UINT);
#define SystemParametersInfo SystemParametersInfoA
BOOL CreateProcessA(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
#define CreateProcess CreateProcessA
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD GetCurrentProcessId(void);
DWORD GetCurrentThreadId(void);
BOOL SetPriorityClass(HANDLE, DWORD);
BOOL SetThreadPriority(HANDLE, int);
DWORD WaitForSingleObject(HANDLE, DWORD);
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR);
#define CreateEvent CreateEventA
BOOL SetEvent(HANDLE);
BOOL ResetEvent(HANDLE);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
#define CreateMutex CreateMutexA
BOOL ReleaseMutex(HANDLE);
void InitializeCriticalSection(LPCRITICAL_SECTION);
void DeleteCriticalSection(LPCRITICAL_SECTION);
void EnterCriticalSection(LPCRITICAL_SECTION);
void LeaveCriticalSection(LPCRITICAL_SECTION);
LPVOID VirtualAlloc(LPVOID, size_t, DWORD, DWORD);
BOOL VirtualFree(LPVOID, size_t, DWORD);
BOOL VirtualProtect(LPVOID, size_t, DWORD, PDWORD);
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE, DWORD, size_t);
BOOL HeapFree(HANDLE, DWORD, LPVOID);
LONG InterlockedIncrement(LONG volatile*);
LONG InterlockedDecrement(LONG volatile*);
int wsprintfA(LPSTR, LPCSTR, ...);
#define wsprintf wsprintfA
int lstrlenA(LPCSTR);
#define lstrlen lstrlenA
LPSTR lstrcpyA(LPSTR, LPCSTR);
#define lstrcpy lstrcpyA
LPSTR lstrcatA(LPSTR, LPCSTR);
#define lstrcat lstrcatA
int lstrcmpiA(LPCSTR, LPCSTR);
#define lstrcmpi lstrcmpiA
int lstrcmpA(LPCSTR, LPCSTR);
#define lstrcmp lstrcmpA
LPSTR GetCommandLineA(void);
#define GetCommandLine GetCommandLineA
HANDLE FindFirstChangeNotificationA(LPCSTR, BOOL, DWORD);
BOOL ShellExecuteA_unused(void);
LPVOID TlsGetValue(DWORD);
int GetObjectA(HGDIOBJ, int, LPVOID);
BOOL DeleteObject(HGDIOBJ);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
HDC CreateCompatibleDC(HDC);
BOOL DeleteDC(HDC);
BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT);
#define LoadImage LoadImageA
#define SRCCOPY 0x00CC0020
#define IMAGE_BITMAP 0
#define LR_LOADFROMFILE 0x0010
#define LR_CREATEDIBSECTION 0x2000
#include "commctrl.h"
#include "mmsystem.h"

// ---- added during the port: misc APIs used by winmain/Frontend/InputRecorder/KeyboardLayout ----
#include "io.h"
#define _stat stat
#define _fstat fstat
#define FIELD_OFFSET(type, field) ((LONG)offsetof(type, field))
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA 8
#define SW_SHOWNOACTIVATE 4
#define SW_MAXIMIZE 3
#define ERROR_READ_FAULT 30L
#define ERROR_INVALID_ACCESS 12L
#define ERROR_ACCESS_DENIED 5L
#define MUTEX_ALL_ACCESS 0x1F0001
#define SYNCHRONIZE 0x00100000L
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define KL_NAMELENGTH 9
#define LANG_FRENCH 0x0c
#define LANG_GERMAN 0x07
#define LANG_ITALIAN 0x10
#define LANG_SPANISH 0x0a
#define LANG_PORTUGUESE 0x16
#define LANG_RUSSIAN 0x19
#define SUBLANG_DEFAULT 0x01
#define SUBLANG_ENGLISH_US 0x01
#define SUBLANG_ENGLISH_UK 0x02
typedef struct _RGNDATA* LPRGNDATA;
typedef struct tagWINDOWPOS
{
    HWND hwnd;
    HWND hwndInsertAfter;
    int x, y, cx, cy;
    UINT flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;
typedef struct tagVS_FIXEDFILEINFO
{
    DWORD dwSignature, dwStrucVersion, dwFileVersionMS, dwFileVersionLS, dwProductVersionMS, dwProductVersionLS;
    DWORD dwFileFlagsMask, dwFileFlags, dwFileOS, dwFileType, dwFileSubtype, dwFileDateMS, dwFileDateLS;
} VS_FIXEDFILEINFO;
int wvsprintfA(LPSTR, LPCSTR, va_list);
#define wvsprintf wvsprintfA
HANDLE OpenMutexA(DWORD, BOOL, LPCSTR);
#define OpenMutex OpenMutexA
HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
#define ShellExecute ShellExecuteA
HWND FindWindowExA(HWND, HWND, LPCSTR, LPCSTR);
#define FindWindowEx FindWindowExA
DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD);
BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID);
BOOL VerQueryValueA(LPCVOID, LPCSTR, LPVOID*, UINT*);
int GetWindowTextA(HWND, LPSTR, int);
#define GetWindowText GetWindowTextA
BOOL GetUserNameA(LPSTR, LPDWORD);
#define GetUserName GetUserNameA
BOOL GetKeyboardLayoutNameA(LPSTR);
#define GetKeyboardLayoutName GetKeyboardLayoutNameA
