// Win32 API surface used by the decompiled game, implemented on POSIX + SDL3.
#include "port_internal.hpp"
#include <objbase.h>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <mach/mach_time.h>

namespace port
{
std::string ResolveRead(const char* path);
std::string ResolveWrite(const char* path);

// Log to stderr and to gta2.log in the preferences folder (readable from the device without
// a debugger: xcrun devicectl device copy from ... Library/Application Support/GTA2Port/GTA2/gta2.log)
void Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stderr);
    static FILE* f = NULL;
    static bool tried = false;
    static size_t written = 0;
    if (!tried)
    {
        tried = true;
        f = ::fopen((PrefRoot() + "/gta2.log").c_str(), "w");
    }
    if (f && written < (8u << 20))
    {
        written += fputs(buf, f) >= 0 ? strlen(buf) : 0;
        fflush(f);
    }
}
} // namespace port

extern "C" unsigned long long port_cycle_counter(void)
{
    return mach_absolute_time();
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
static uint64_t NowNs()
{
    return SDL_GetTicksNS();
}

DWORD timeGetTime(void)
{
    return static_cast<DWORD>(NowNs() / 1000000ull);
}

DWORD GetTickCount(void)
{
    return timeGetTime();
}

UINT timeBeginPeriod(UINT)
{
    return TIMERR_NOERROR;
}

UINT timeEndPeriod(UINT)
{
    return TIMERR_NOERROR;
}

void Sleep(DWORD ms)
{
    SDL_Delay(ms);
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* p)
{
    p->QuadPart = static_cast<LONGLONG>(SDL_GetPerformanceCounter());
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* p)
{
    p->QuadPart = static_cast<LONGLONG>(SDL_GetPerformanceFrequency());
    return TRUE;
}

static void FillSystemTime(LPSYSTEMTIME st, bool local)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    struct tm tmv;
    if (local)
    {
        localtime_r(&t, &tmv);
    }
    else
    {
        gmtime_r(&t, &tmv);
    }
    st->wYear = static_cast<WORD>(tmv.tm_year + 1900);
    st->wMonth = static_cast<WORD>(tmv.tm_mon + 1);
    st->wDayOfWeek = static_cast<WORD>(tmv.tm_wday);
    st->wDay = static_cast<WORD>(tmv.tm_mday);
    st->wHour = static_cast<WORD>(tmv.tm_hour);
    st->wMinute = static_cast<WORD>(tmv.tm_min);
    st->wSecond = static_cast<WORD>(tmv.tm_sec);
    st->wMilliseconds = static_cast<WORD>(tv.tv_usec / 1000);
}

void GetLocalTime(LPSYSTEMTIME st)
{
    FillSystemTime(st, true);
}

void GetSystemTime(LPSYSTEMTIME st)
{
    FillSystemTime(st, false);
}

// ---------------------------------------------------------------------------
// Misc process / system
// ---------------------------------------------------------------------------
static DWORD gLastError = 0;

DWORD GetLastError(void)
{
    return gLastError;
}

void SetLastError(DWORD e)
{
    gLastError = e;
}

void OutputDebugStringA(LPCSTR s)
{
    port::Log("%s", s);
}

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    port::Log("[MessageBox] %s: %s\n", caption ? caption : "", text ? text : "");
    if (getenv("PORT_NO_MSGBOX"))
    {
        return IDOK;
    }
    SDL_ShowSimpleMessageBox((type & MB_ICONERROR) ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, caption ? caption : "GTA2",
                             text ? text : "", port::Window());
    if ((type & 0xF) == MB_YESNO)
    {
        return IDYES;
    }
    return IDOK;
}

void ExitProcess(UINT code)
{
    exit(static_cast<int>(code));
}

BOOL GetVersionExA(LPOSVERSIONINFOA v)
{
    v->dwMajorVersion = 5;
    v->dwMinorVersion = 1;
    v->dwBuildNumber = 2600;
    v->dwPlatformId = VER_PLATFORM_WIN32_NT;
    v->szCSDVersion[0] = 0;
    return TRUE;
}

DWORD GetVersion(void)
{
    return 0x0A280105; // 5.1 build 2600
}

void GlobalMemoryStatus(LPMEMORYSTATUS m)
{
    memset(m, 0, sizeof(*m));
    m->dwLength = sizeof(*m);
    m->dwTotalPhys = m->dwAvailPhys = 512u * 1024 * 1024;
    m->dwTotalVirtual = m->dwAvailVirtual = 1024u * 1024 * 1024;
}

HANDLE GetCurrentProcess(void)
{
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

HANDLE GetCurrentThread(void)
{
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2));
}

DWORD GetCurrentProcessId(void)
{
    return static_cast<DWORD>(getpid());
}

DWORD GetCurrentThreadId(void)
{
    return static_cast<DWORD>(SDL_GetCurrentThreadID());
}

BOOL SetPriorityClass(HANDLE, DWORD)
{
    return TRUE;
}

BOOL SetThreadPriority(HANDLE, int)
{
    return TRUE;
}

// Synchronisation objects: the game is single threaded, these only need to "work"
struct PortSyncObject
{
    int kind;
};

DWORD WaitForSingleObject(HANDLE, DWORD)
{
    return WAIT_OBJECT_0;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR)
{
    return new PortSyncObject{1};
}

BOOL SetEvent(HANDLE)
{
    return TRUE;
}

BOOL ResetEvent(HANDLE)
{
    return TRUE;
}

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR)
{
    return new PortSyncObject{2};
}

HANDLE OpenMutexA(DWORD, BOOL, LPCSTR)
{
    // The "another instance is running" check: never true here
    return NULL;
}

BOOL ReleaseMutex(HANDLE)
{
    return TRUE;
}

void InitializeCriticalSection(LPCRITICAL_SECTION cs)
{
    cs->impl = new std::recursive_mutex();
}

void DeleteCriticalSection(LPCRITICAL_SECTION cs)
{
    delete static_cast<std::recursive_mutex*>(cs->impl);
    cs->impl = NULL;
}

void EnterCriticalSection(LPCRITICAL_SECTION cs)
{
    static_cast<std::recursive_mutex*>(cs->impl)->lock();
}

void LeaveCriticalSection(LPCRITICAL_SECTION cs)
{
    static_cast<std::recursive_mutex*>(cs->impl)->unlock();
}

LPVOID VirtualAlloc(LPVOID, size_t size, DWORD, DWORD)
{
    return calloc(1, size);
}

BOOL VirtualFree(LPVOID p, size_t, DWORD)
{
    free(p);
    return TRUE;
}

BOOL VirtualProtect(LPVOID, size_t, DWORD, PDWORD old)
{
    if (old)
    {
        *old = PAGE_READWRITE;
    }
    return TRUE;
}

HANDLE GetProcessHeap(void)
{
    return reinterpret_cast<HANDLE>(1);
}

LPVOID HeapAlloc(HANDLE, DWORD flags, size_t n)
{
    return (flags & 8) ? calloc(1, n) : malloc(n);
}

BOOL HeapFree(HANDLE, DWORD, LPVOID p)
{
    free(p);
    return TRUE;
}

LONG InterlockedIncrement(LONG volatile* p)
{
    return __sync_add_and_fetch(p, 1);
}

LONG InterlockedDecrement(LONG volatile* p)
{
    return __sync_sub_and_fetch(p, 1);
}

int wsprintfA(LPSTR buf, LPCSTR fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int wvsprintfA(LPSTR buf, LPCSTR fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}

int lstrlenA(LPCSTR s)
{
    return s ? static_cast<int>(strlen(s)) : 0;
}

LPSTR lstrcpyA(LPSTR d, LPCSTR s)
{
    return strcpy(d, s);
}

LPSTR lstrcatA(LPSTR d, LPCSTR s)
{
    return strcat(d, s);
}

int lstrcmpiA(LPCSTR a, LPCSTR b)
{
    return strcasecmp(a, b);
}

int lstrcmpA(LPCSTR a, LPCSTR b)
{
    return strcmp(a, b);
}

static char gCommandLine[512] = "";

LPSTR GetCommandLineA(void)
{
    return gCommandLine;
}

LPVOID TlsGetValue(DWORD)
{
    return NULL;
}

BOOL GetUserNameA(LPSTR buf, LPDWORD len)
{
    const char* n = "Player";
    snprintf(buf, *len, "%s", n);
    *len = static_cast<DWORD>(strlen(n) + 1);
    return TRUE;
}

WORD GetUserDefaultLangID(void)
{
    return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

WORD GetSystemDefaultLangID(void)
{
    return GetUserDefaultLangID();
}

UINT GetACP(void)
{
    return 1252;
}

BOOL GetKeyboardLayoutNameA(LPSTR buf)
{
    strcpy(buf, "00000409");
    return TRUE;
}

int MultiByteToWideChar(UINT, DWORD, LPCSTR s, int n, LPWSTR d, int dn)
{
    if (n < 0)
    {
        n = static_cast<int>(strlen(s)) + 1;
    }
    if (!d || !dn)
    {
        return n;
    }
    int c = n < dn ? n : dn;
    for (int i = 0; i < c; i++)
    {
        d[i] = static_cast<unsigned char>(s[i]);
    }
    return c;
}

int WideCharToMultiByte(UINT, DWORD, LPCWSTR s, int n, LPSTR d, int dn, LPCSTR, LPBOOL)
{
    if (n < 0)
    {
        n = static_cast<int>(wcslen(s)) + 1;
    }
    if (!d || !dn)
    {
        return n;
    }
    int c = n < dn ? n : dn;
    for (int i = 0; i < c; i++)
    {
        d[i] = s[i] < 0x100 ? static_cast<char>(s[i]) : '?';
    }
    return c;
}

BOOL SystemParametersInfoA(UINT, UINT, PVOID, UINT)
{
    return TRUE;
}

BOOL CreateProcessA(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION)
{
    return FALSE;
}

HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR file, LPCSTR, LPCSTR, INT)
{
    port::Log("ShellExecute(%s) ignored\n", file ? file : "");
    return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(2)); // "file not found"
}

DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD h)
{
    if (h)
    {
        *h = 0;
    }
    return sizeof(VS_FIXEDFILEINFO);
}

BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD len, LPVOID data)
{
    if (len < sizeof(VS_FIXEDFILEINFO))
    {
        return FALSE;
    }
    VS_FIXEDFILEINFO* fi = static_cast<VS_FIXEDFILEINFO*>(data);
    memset(fi, 0, sizeof(*fi));
    fi->dwSignature = 0xFEEF04BD;
    fi->dwFileVersionMS = (10 << 16) | 5; // GTA2 10.5
    fi->dwFileVersionLS = 0;
    fi->dwProductVersionMS = fi->dwFileVersionMS;
    return TRUE;
}

BOOL VerQueryValueA(LPCVOID block, LPCSTR, LPVOID* out, UINT* len)
{
    *out = const_cast<LPVOID>(block);
    if (len)
    {
        *len = sizeof(VS_FIXEDFILEINFO);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Modules: LoadLibrary/GetProcAddress resolve against the built-in replacements of
// the game's DLLs (d3ddll.dll, dmavideo.dll, ...)
// ---------------------------------------------------------------------------
struct PortExport
{
    const char* name;
    void* fn;
};
extern const PortExport* PortGetVideoExports();

HMODULE LoadLibraryA(LPCSTR name)
{
    port::Log("LoadLibrary(%s)\n", name ? name : "");
    return reinterpret_cast<HMODULE>(static_cast<intptr_t>(0x1000));
}

FARPROC GetProcAddress(HMODULE, LPCSTR name)
{
    for (const PortExport* e = PortGetVideoExports(); e->name; e++)
    {
        if (strcmp(e->name, name) == 0)
        {
            return reinterpret_cast<FARPROC>(e->fn);
        }
    }
    port::Log("GetProcAddress: missing %s\n", name);
    return NULL;
}

BOOL FreeLibrary(HMODULE)
{
    return TRUE;
}

HMODULE GetModuleHandleA(LPCSTR)
{
    return reinterpret_cast<HMODULE>(static_cast<intptr_t>(0x400000));
}

DWORD GetModuleFileNameA(HMODULE, LPSTR buf, DWORD n)
{
    snprintf(buf, n, "%s/gta2.exe", port::GameRoot().c_str());
    return static_cast<DWORD>(strlen(buf));
}

// ---------------------------------------------------------------------------
// Files / directories
// ---------------------------------------------------------------------------
DWORD GetCurrentDirectoryA(DWORD n, LPSTR buf)
{
    snprintf(buf, n, "%s", port::GameRoot().c_str());
    return static_cast<DWORD>(strlen(buf));
}

BOOL SetCurrentDirectoryA(LPCSTR)
{
    return TRUE;
}

UINT GetDriveTypeA(LPCSTR)
{
    return DRIVE_FIXED;
}

DWORD GetLogicalDrives(void)
{
    return 1 << 2; // C:
}

DWORD GetLogicalDriveStringsA(DWORD n, LPSTR buf)
{
    if (n < 5)
    {
        return 5;
    }
    memcpy(buf, "C:\\\0\0", 5);
    return 4;
}

BOOL GetVolumeInformationA(LPCSTR, LPSTR name, DWORD nameLen, LPDWORD serial, LPDWORD maxComp, LPDWORD flags, LPSTR fs, DWORD fsLen)
{
    if (name && nameLen)
    {
        snprintf(name, nameLen, "GTA2");
    }
    if (serial)
    {
        *serial = 0x12345678;
    }
    if (maxComp)
    {
        *maxComp = 255;
    }
    if (flags)
    {
        *flags = 0;
    }
    if (fs && fsLen)
    {
        snprintf(fs, fsLen, "NTFS");
    }
    return TRUE;
}

DWORD GetFileAttributesA(LPCSTR path)
{
    std::string p = port::ResolveRead(path);
    struct stat st;
    if (::stat(p.c_str(), &st) != 0)
    {
        return INVALID_FILE_ATTRIBUTES;
    }
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

BOOL SetFileAttributesA(LPCSTR, DWORD)
{
    return TRUE;
}

BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES)
{
    std::string p = port::ResolveWrite(path);
    return mkdir(p.c_str(), 0755) == 0 ? TRUE : FALSE;
}

BOOL DeleteFileA(LPCSTR path)
{
    std::string p = port::ResolveWrite(path);
    return ::remove(p.c_str()) == 0 ? TRUE : FALSE;
}

struct FindFileState
{
    intptr_t h;
};

static void ToFindData(const _finddata_t& fd, LPWIN32_FIND_DATAA out)
{
    memset(out, 0, sizeof(*out));
    out->dwFileAttributes = (fd.attrib & _A_SUBDIR) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    out->nFileSizeLow = static_cast<DWORD>(fd.size);
    snprintf(out->cFileName, sizeof(out->cFileName), "%s", fd.name);
}

HANDLE FindFirstFileA(LPCSTR spec, LPWIN32_FIND_DATAA out)
{
    _finddata_t fd;
    intptr_t h = port_findfirst(spec, &fd);
    if (h == -1)
    {
        return INVALID_HANDLE_VALUE;
    }
    ToFindData(fd, out);
    return new FindFileState{h};
}

BOOL FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA out)
{
    _finddata_t fd;
    if (port_findnext(static_cast<FindFileState*>(h)->h, &fd) != 0)
    {
        return FALSE;
    }
    ToFindData(fd, out);
    return TRUE;
}

BOOL FindClose(HANDLE h)
{
    if (h && h != INVALID_HANDLE_VALUE)
    {
        port_findclose(static_cast<FindFileState*>(h)->h);
        delete static_cast<FindFileState*>(h);
    }
    return TRUE;
}

struct FileHandle
{
    FILE* f;
};

HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD, HANDLE)
{
    const char* mode = "rb";
    if (access & GENERIC_WRITE)
    {
        mode = (disposition == CREATE_ALWAYS || disposition == CREATE_NEW) ? "wb" : "r+b";
        if (disposition == OPEN_ALWAYS)
        {
            mode = "a+b";
        }
    }
    FILE* f = port_fopen(path, mode);
    if (!f && (access & GENERIC_WRITE) && disposition == OPEN_ALWAYS)
    {
        f = port_fopen(path, "w+b");
    }
    if (!f)
    {
        gLastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    return new FileHandle{f};
}

BOOL ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, void*)
{
    size_t r = ::fread(buf, 1, n, static_cast<FileHandle*>(h)->f);
    if (read)
    {
        *read = static_cast<DWORD>(r);
    }
    return TRUE;
}

BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD written, void*)
{
    size_t w = ::fwrite(buf, 1, n, static_cast<FileHandle*>(h)->f);
    if (written)
    {
        *written = static_cast<DWORD>(w);
    }
    return w == n;
}

DWORD GetFileSize(HANDLE h, LPDWORD high)
{
    FILE* f = static_cast<FileHandle*>(h)->f;
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, cur, SEEK_SET);
    if (high)
    {
        *high = 0;
    }
    return static_cast<DWORD>(size);
}

DWORD SetFilePointer(HANDLE h, LONG dist, PLONG, DWORD method)
{
    FILE* f = static_cast<FileHandle*>(h)->f;
    fseek(f, dist, method == FILE_BEGIN ? SEEK_SET : method == FILE_CURRENT ? SEEK_CUR : SEEK_END);
    return static_cast<DWORD>(ftell(f));
}

BOOL CloseHandle(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    // Only file handles are closed for real; sync objects are left alone
    // (they're created once and live for the whole run).
    return TRUE;
}

UINT GetWindowsDirectoryA(LPSTR buf, UINT n)
{
    snprintf(buf, n, "%s", port::PrefRoot().c_str());
    return static_cast<UINT>(strlen(buf));
}

UINT GetSystemDirectoryA(LPSTR buf, UINT n)
{
    return GetWindowsDirectoryA(buf, n);
}

DWORD GetTempPathA(DWORD n, LPSTR buf)
{
    snprintf(buf, n, "%s/", port::PrefRoot().c_str());
    return static_cast<DWORD>(strlen(buf));
}

DWORD GetPrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR def, LPSTR out, DWORD n, LPCSTR)
{
    snprintf(out, n, "%s", def ? def : "");
    return static_cast<DWORD>(strlen(out));
}

UINT GetPrivateProfileIntA(LPCSTR, LPCSTR, int def, LPCSTR)
{
    return static_cast<UINT>(def);
}

BOOL WritePrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR, LPCSTR)
{
    return TRUE;
}

// ---------------------------------------------------------------------------
// Registry, kept in a text file in the preferences folder.
// Keys are full paths ("HKLM\\Software\\DMA Design Ltd\\GTA2\\Screen").
// ---------------------------------------------------------------------------
namespace
{
struct RegValue
{
    DWORD type;
    std::string data;
};
typedef std::map<std::string, RegValue> RegKeyValues;
// Function-local statics: game globals call into the registry from their constructors
std::map<std::string, RegKeyValues>& RegistryMap()
{
    static std::map<std::string, RegKeyValues> m;
    return m;
}
std::map<HKEY, std::string>& OpenKeys()
{
    static std::map<HKEY, std::string> m;
    return m;
}
#define gRegistry RegistryMap()
#define gOpenKeys OpenKeys()
intptr_t gNextKey = 0x100;
bool gRegistryLoaded = false;

std::string RegFile()
{
    return port::PrefRoot() + "/registry.txt";
}

std::string Lower(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Hex(const std::string& s)
{
    static const char* d = "0123456789abcdef";
    std::string r;
    for (unsigned char c : s)
    {
        r.push_back(d[c >> 4]);
        r.push_back(d[c & 15]);
    }
    return r;
}

std::string Unhex(const std::string& s)
{
    std::string r;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
    {
        r.push_back(static_cast<char>(strtol(s.substr(i, 2).c_str(), NULL, 16)));
    }
    return r;
}

void LoadRegistry()
{
    if (gRegistryLoaded)
    {
        return;
    }
    gRegistryLoaded = true;

    // Values the freeware installer writes (see Scripts/gta2_data_setup.py): all three
    // districts installed. The saved file below can override them.
    auto setDword = [](const char* key, const char* name, DWORD v) {
        RegValue rv;
        rv.type = REG_DWORD;
        rv.data.assign(reinterpret_cast<const char*>(&v), sizeof(v));
        gRegistry[key][name] = rv;
    };
    auto setString = [](const char* key, const char* name, const char* v) {
        RegValue rv;
        rv.type = REG_SZ;
        rv.data.assign(v, strlen(v) + 1);
        gRegistry[key][name] = rv;
    };
    setString("hklm\\software\\freeloader.com", "webroot", "http://www.freeloader.com/?page_id=%d");
    setString("hklm\\software\\freeloader.com\\gta2", "installpath", port::GameRoot().c_str());
    setString("hklm\\software\\freeloader.com\\gta2", "firstname", "");
    setString("hklm\\software\\freeloader.com\\gta2", "lastname", "");
    setDword("hklm\\software\\freeloader.com\\gta2", "userid", 0);
    setDword("hklm\\software\\freeloader.com\\gta2", "page_id", 0);
    setDword("hklm\\software\\freeloader.com\\gta2", "lang", 0);
    setDword("hklm\\software\\freeloader.com\\gta2", "f_l_t", 0);
    setDword("hklm\\software\\freeloader.com\\gta2", "cityinstalled", 2);

    FILE* f = ::fopen(RegFile().c_str(), "r");
    if (!f)
    {
        return;
    }
    char line[4096];
    while (fgets(line, sizeof(line), f))
    {
        // key \t name \t type \t hexdata
        std::string l(line);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
        {
            l.pop_back();
        }
        size_t a = l.find('\t');
        size_t b = a == std::string::npos ? a : l.find('\t', a + 1);
        size_t c = b == std::string::npos ? b : l.find('\t', b + 1);
        if (c == std::string::npos)
        {
            continue;
        }
        RegValue v;
        v.type = static_cast<DWORD>(atoi(l.substr(b + 1, c - b - 1).c_str()));
        v.data = Unhex(l.substr(c + 1));
        gRegistry[l.substr(0, a)][l.substr(a + 1, b - a - 1)] = v;
    }
    fclose(f);
}

void SaveRegistry();

} // namespace

// Widescreen default, applied once when the video system starts (SDL knows the display then);
// later the player's own choice in the options wins.
void port::ApplyWidescreenDefault()
{
    LoadRegistry();
    int ww = 0, wh = 0;
    port::WidescreenSize(ww, wh);
    const char* key = "hklm\\software\\dma design ltd\\gta2\\screen";
    if (!ww || gRegistry[key].count("port_widescreen_init"))
    {
        return;
    }
    auto setDword = [&](const char* name, DWORD v) {
        RegValue rv;
        rv.type = REG_DWORD;
        rv.data.assign(reinterpret_cast<const char*>(&v), sizeof(v));
        gRegistry[key][name] = rv;
    };
    setDword("full_width", static_cast<DWORD>(ww));
    setDword("full_height", static_cast<DWORD>(wh));
    setDword("start_mode", 1);
    setDword("port_widescreen_init", 1);
    SaveRegistry();
}

namespace
{
void SaveRegistry()
{
    FILE* f = ::fopen(RegFile().c_str(), "w");
    if (!f)
    {
        return;
    }
    for (auto& k : gRegistry)
    {
        for (auto& v : k.second)
        {
            fprintf(f, "%s\t%s\t%u\t%s\n", k.first.c_str(), v.first.c_str(), v.second.type, Hex(v.second.data).c_str());
        }
    }
    fclose(f);
}

std::string KeyPath(HKEY parent, LPCSTR sub)
{
    std::string base;
    if (parent == HKEY_LOCAL_MACHINE || parent == HKEY_CURRENT_USER || parent == HKEY_CLASSES_ROOT || parent == HKEY_USERS)
    {
        base = "hklm"; // treat all roots alike
    }
    else
    {
        auto it = gOpenKeys.find(parent);
        base = it != gOpenKeys.end() ? it->second : "hklm";
    }
    if (sub && *sub)
    {
        base += "\\" + Lower(sub);
    }
    return base;
}

HKEY NewKey(const std::string& path)
{
    HKEY k = reinterpret_cast<HKEY>(gNextKey++);
    gOpenKeys[k] = path;
    return k;
}
} // namespace

LONG RegOpenKeyExA(HKEY parent, LPCSTR sub, DWORD, REGSAM, PHKEY out)
{
    LoadRegistry();
    std::string p = KeyPath(parent, sub);
    if (gRegistry.find(p) == gRegistry.end())
    {
        // Keys are created implicitly: the game only reads values it may then write
        gRegistry[p];
    }
    *out = NewKey(p);
    return ERROR_SUCCESS;
}

LONG RegOpenKeyA(HKEY parent, LPCSTR sub, PHKEY out)
{
    return RegOpenKeyExA(parent, sub, 0, 0, out);
}

LONG RegCreateKeyExA(HKEY parent, LPCSTR sub, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY out, LPDWORD disp)
{
    LoadRegistry();
    std::string p = KeyPath(parent, sub);
    bool existed = gRegistry.find(p) != gRegistry.end();
    gRegistry[p];
    *out = NewKey(p);
    if (disp)
    {
        *disp = existed ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
    }
    return ERROR_SUCCESS;
}

LONG RegCreateKeyA(HKEY parent, LPCSTR sub, PHKEY out)
{
    return RegCreateKeyExA(parent, sub, 0, NULL, 0, 0, NULL, out, NULL);
}

LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD len)
{
    LoadRegistry();
    auto k = gRegistry.find(KeyPath(key, NULL));
    if (k == gRegistry.end())
    {
        return ERROR_FILE_NOT_FOUND;
    }
    auto v = k->second.find(Lower(name ? name : ""));
    if (v == k->second.end())
    {
        return ERROR_FILE_NOT_FOUND;
    }
    if (type)
    {
        *type = v->second.type;
    }
    DWORD need = static_cast<DWORD>(v->second.data.size());
    if (data && len)
    {
        if (*len < need)
        {
            *len = need;
            return ERROR_MORE_DATA;
        }
        memcpy(data, v->second.data.data(), need);
    }
    if (len)
    {
        *len = need;
    }
    return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD, DWORD type, const BYTE* data, DWORD len)
{
    LoadRegistry();
    RegValue v;
    v.type = type;
    v.data.assign(reinterpret_cast<const char*>(data), len);
    gRegistry[KeyPath(key, NULL)][Lower(name ? name : "")] = v;
    SaveRegistry();
    return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, LPCSTR name)
{
    LoadRegistry();
    gRegistry[KeyPath(key, NULL)].erase(Lower(name ? name : ""));
    SaveRegistry();
    return ERROR_SUCCESS;
}

LONG RegDeleteKeyA(HKEY key, LPCSTR sub)
{
    LoadRegistry();
    gRegistry.erase(KeyPath(key, sub));
    SaveRegistry();
    return ERROR_SUCCESS;
}

LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, FILETIME*)
{
    return ERROR_NO_MORE_ITEMS;
}

LONG RegEnumValueA(HKEY key, DWORD idx, LPSTR name, LPDWORD nameLen, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD len)
{
    LoadRegistry();
    auto k = gRegistry.find(KeyPath(key, NULL));
    if (k == gRegistry.end() || idx >= k->second.size())
    {
        return ERROR_NO_MORE_ITEMS;
    }
    auto it = k->second.begin();
    std::advance(it, idx);
    snprintf(name, *nameLen, "%s", it->first.c_str());
    *nameLen = static_cast<DWORD>(it->first.size());
    if (type)
    {
        *type = it->second.type;
    }
    if (data && len && *len >= it->second.data.size())
    {
        memcpy(data, it->second.data.data(), it->second.data.size());
    }
    if (len)
    {
        *len = static_cast<DWORD>(it->second.data.size());
    }
    return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key)
{
    gOpenKeys.erase(key);
    return ERROR_SUCCESS;
}

LONG RegFlushKey(HKEY)
{
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// COM
// ---------------------------------------------------------------------------
HRESULT CoInitialize(LPVOID)
{
    return S_OK;
}

HRESULT CoInitializeEx(LPVOID, DWORD)
{
    return S_OK;
}

void CoUninitialize(void)
{
}

HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* out)
{
    *out = NULL;
    return E_NOINTERFACE;
}

// ---------------------------------------------------------------------------
// Windows and messages
// ---------------------------------------------------------------------------
namespace
{
WNDPROC gWndProc = NULL;
std::deque<MSG>& MsgQueue()
{
    static std::deque<MSG> q;
    return q;
}
#define gMsgQueue MsgQueue()
RECT gWindowRect = {0, 0, 640, 480};
LONG gWindowStyle = WS_OVERLAPPEDWINDOW;
int gCursorShowCount = 0;
} // namespace

namespace port
{
void SetWndProc(WNDPROC proc)
{
    gWndProc = proc;
}

WNDPROC GetWndProc()
{
    return gWndProc;
}

HWND GameHwnd()
{
    return reinterpret_cast<HWND>(static_cast<intptr_t>(0x2000));
}

void PostMsg(UINT msg, WPARAM wParam, LPARAM lParam)
{
    MSG m;
    memset(&m, 0, sizeof(m));
    m.hwnd = GameHwnd();
    m.message = msg;
    m.wParam = wParam;
    m.lParam = lParam;
    m.time = timeGetTime();
    gMsgQueue.push_back(m);
}

bool PopMsg(MSG* pMsg, bool remove)
{
    if (gMsgQueue.empty())
    {
        return false;
    }
    *pMsg = gMsgQueue.front();
    if (remove)
    {
        gMsgQueue.pop_front();
    }
    return true;
}
} // namespace port

BOOL PeekMessageA(LPMSG msg, HWND, UINT, UINT, UINT remove)
{
    port::PumpEvents();
    return port::PopMsg(msg, (remove & PM_REMOVE) != 0) ? TRUE : FALSE;
}

BOOL GetMessageA(LPMSG msg, HWND, UINT, UINT)
{
    for (;;)
    {
        port::PumpEvents();
        if (port::PopMsg(msg, true))
        {
            return msg->message != WM_QUIT;
        }
        SDL_Delay(5);
    }
}

BOOL TranslateMessage(const MSG* msg)
{
    // WM_CHAR is generated from SDL text input events directly
    (void)msg;
    return TRUE;
}

LRESULT DispatchMessageA(const MSG* msg)
{
    if (gWndProc && msg->message != WM_QUIT)
    {
        return gWndProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
    }
    return 0;
}

BOOL PostMessageA(HWND, UINT msg, WPARAM w, LPARAM l)
{
    port::PostMsg(msg, w, l);
    return TRUE;
}

LRESULT SendMessageA(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (gWndProc)
    {
        return gWndProc(hwnd, msg, w, l);
    }
    return 0;
}

void PostQuitMessage(int code)
{
    port::PostMsg(WM_QUIT, static_cast<WPARAM>(code), 0);
}

LRESULT DefWindowProcA(HWND, UINT msg, WPARAM, LPARAM)
{
    if (msg == WM_CLOSE)
    {
        DestroyWindow(port::GameHwnd());
    }
    return 0;
}

ATOM RegisterClassA(const WNDCLASSA* wc)
{
    gWndProc = wc->lpfnWndProc;
    return 1;
}

ATOM RegisterClassExA(const WNDCLASSEXA* wc)
{
    gWndProc = wc->lpfnWndProc;
    return 1;
}

BOOL UnregisterClassA(LPCSTR, HINSTANCE)
{
    return TRUE;
}

HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR title, DWORD style, int, int, int w, int h, HWND, HMENU, HINSTANCE, LPVOID)
{
    gWindowStyle = static_cast<LONG>(style);
    if (!port::Window())
    {
        if (!port::CreateWindowAndRenderer(title, 640, 480))
        {
            return NULL;
        }
    }
    gWindowRect.right = gWindowRect.left + (w > 0 ? w : 640);
    gWindowRect.bottom = gWindowRect.top + (h > 0 ? h : 480);
    if (gWndProc)
    {
        gWndProc(port::GameHwnd(), WM_CREATE, 0, 0);
    }
    return port::GameHwnd();
}

BOOL DestroyWindow(HWND)
{
    if (gWndProc)
    {
        gWndProc(port::GameHwnd(), WM_DESTROY, 0, 0);
    }
    return TRUE;
}

BOOL ShowWindow(HWND, int)
{
    return TRUE;
}

BOOL UpdateWindow(HWND)
{
    return TRUE;
}

HWND SetFocus(HWND h)
{
    return h;
}

HWND GetFocus(void)
{
    return port::GameHwnd();
}

HWND GetActiveWindow(void)
{
    return port::GameHwnd();
}

HWND SetActiveWindow(HWND h)
{
    return h;
}

BOOL SetForegroundWindow(HWND)
{
    return TRUE;
}

HWND GetForegroundWindow(void)
{
    return port::GameHwnd();
}

HWND FindWindowA(LPCSTR, LPCSTR)
{
    return NULL;
}

HWND FindWindowExA(HWND, HWND, LPCSTR, LPCSTR)
{
    return NULL;
}

BOOL IsWindow(HWND h)
{
    return h == port::GameHwnd();
}

BOOL IsIconic(HWND)
{
    return FALSE;
}

BOOL IsWindowVisible(HWND)
{
    return TRUE;
}

// The game sees a window whose client area is exactly the logical resolution
BOOL GetClientRect(HWND, LPRECT r)
{
    r->left = 0;
    r->top = 0;
    r->right = port::LogicalW();
    r->bottom = port::LogicalH();
    return TRUE;
}

BOOL GetWindowRect(HWND, LPRECT r)
{
    *r = gWindowRect;
    return TRUE;
}

BOOL ClientToScreen(HWND, LPPOINT)
{
    return TRUE;
}

BOOL ScreenToClient(HWND, LPPOINT)
{
    return TRUE;
}

BOOL AdjustWindowRect(LPRECT, DWORD, BOOL)
{
    return TRUE;
}

BOOL AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD)
{
    return TRUE;
}

BOOL SetWindowPos(HWND, HWND, int x, int y, int w, int h, UINT flags)
{
    if (!(flags & SWP_NOMOVE))
    {
        gWindowRect.left = x;
        gWindowRect.top = y;
    }
    if (!(flags & SWP_NOSIZE))
    {
        gWindowRect.right = gWindowRect.left + w;
        gWindowRect.bottom = gWindowRect.top + h;
    }
    return TRUE;
}

BOOL MoveWindow(HWND h, int x, int y, int w, int hh, BOOL)
{
    return SetWindowPos(h, NULL, x, y, w, hh, 0);
}

LONG GetWindowLongA(HWND, int idx)
{
    return idx == GWL_STYLE ? gWindowStyle : 0;
}

LONG SetWindowLongA(HWND, int idx, LONG v)
{
    LONG old = GetWindowLongA(NULL, idx);
    if (idx == GWL_STYLE)
    {
        gWindowStyle = v;
    }
    return old;
}

BOOL SetWindowTextA(HWND, LPCSTR t)
{
    if (port::Window())
    {
        SDL_SetWindowTitle(port::Window(), t);
    }
    return TRUE;
}

int GetWindowTextA(HWND, LPSTR buf, int n)
{
    if (n > 0)
    {
        buf[0] = 0;
    }
    return 0;
}

int GetSystemMetrics(int idx)
{
    if (idx == SM_CXSCREEN)
    {
        return 1920;
    }
    if (idx == SM_CYSCREEN)
    {
        return 1080;
    }
    return 0;
}

int ShowCursor(BOOL show)
{
    gCursorShowCount += show ? 1 : -1;
    if (port::Window())
    {
        if (gCursorShowCount >= 0)
        {
            SDL_ShowCursor();
        }
        else
        {
            SDL_HideCursor();
        }
    }
    return gCursorShowCount;
}

HCURSOR SetCursor(HCURSOR c)
{
    return c;
}

BOOL SetCursorPos(int, int)
{
    return TRUE;
}

BOOL GetCursorPos(LPPOINT p)
{
    float x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    p->x = static_cast<LONG>(x);
    p->y = static_cast<LONG>(y);
    return TRUE;
}

BOOL ClipCursor(const RECT*)
{
    return TRUE;
}

HCURSOR LoadCursorA(HINSTANCE, LPCSTR)
{
    return NULL;
}

HICON LoadIconA(HINSTANCE, LPCSTR)
{
    return NULL;
}

HGDIOBJ GetStockObject(int)
{
    return NULL;
}

BOOL InvalidateRect(HWND, const RECT*, BOOL)
{
    return TRUE;
}

BOOL ValidateRect(HWND, const RECT*)
{
    return TRUE;
}

HDC GetDC(HWND)
{
    return reinterpret_cast<HDC>(static_cast<intptr_t>(0x3000));
}

int ReleaseDC(HWND, HDC)
{
    return 1;
}

int GetDeviceCaps(HDC, int idx)
{
    switch (idx)
    {
        case BITSPIXEL:
            return 16; // makes the game offer windowed mode
        case HORZRES:
            return 1920;
        case VERTRES:
            return 1080;
        case PLANES:
            return 1;
        default:
            return 0;
    }
}

BOOL SetRect(LPRECT r, int l, int t, int ri, int b)
{
    r->left = l;
    r->top = t;
    r->right = ri;
    r->bottom = b;
    return TRUE;
}

BOOL PtInRect(const RECT* r, POINT p)
{
    return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom;
}

SHORT GetAsyncKeyState(int vk)
{
    // Only the keys the game asks about need mapping
    const unsigned char* dik = port::DikState();
    switch (vk)
    {
        case VK_SHIFT:
            return (dik[0x2A] | dik[0x36]) ? static_cast<SHORT>(0x8000) : 0;
        case VK_CONTROL:
            return (dik[0x1D] | dik[0x9D]) ? static_cast<SHORT>(0x8000) : 0;
        case VK_MENU:
        case VK_LMENU:
            return dik[0x38] ? static_cast<SHORT>(0x8000) : 0;
        case VK_ESCAPE:
            return dik[0x01] ? static_cast<SHORT>(0x8000) : 0;
        case VK_RETURN:
            return dik[0x1C] ? static_cast<SHORT>(0x8000) : 0;
        default:
            return 0;
    }
}

SHORT GetKeyState(int vk)
{
    return GetAsyncKeyState(vk);
}

BOOL GetKeyboardState(PBYTE state)
{
    memset(state, 0, 256);
    return TRUE;
}

UINT MapVirtualKeyA(UINT code, UINT)
{
    return code;
}

int GetKeyNameTextA(LONG, LPSTR buf, int n)
{
    if (n > 0)
    {
        buf[0] = 0;
    }
    return 0;
}

// GDI bits the game touches for the (unused) software paths
int GetObjectA(HGDIOBJ, int, LPVOID)
{
    return 0;
}

BOOL DeleteObject(HGDIOBJ)
{
    return TRUE;
}

HGDIOBJ SelectObject(HDC, HGDIOBJ)
{
    return NULL;
}

HDC CreateCompatibleDC(HDC)
{
    return NULL;
}

BOOL DeleteDC(HDC)
{
    return TRUE;
}

BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD)
{
    return TRUE;
}

HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT)
{
    return NULL;
}

// Dialogs (network lobby): not available in the port
HWND GetDlgItem(HWND, int)
{
    return NULL;
}

int GetDlgCtrlID(HWND)
{
    return 0;
}

BOOL SetDlgItemTextA(HWND, int, LPCSTR)
{
    return TRUE;
}

UINT GetDlgItemTextA(HWND, int, LPSTR buf, int n)
{
    if (n > 0)
    {
        buf[0] = 0;
    }
    return 0;
}

LRESULT SendDlgItemMessageA(HWND, int, UINT, WPARAM, LPARAM)
{
    return 0;
}

INT_PTR DialogBoxParamA(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM)
{
    return IDCANCEL;
}

BOOL EndDialog(HWND, INT_PTR)
{
    return TRUE;
}

HWND GetParent(HWND)
{
    return NULL;
}

LRESULT CallWindowProcA(WNDPROC proc, HWND h, UINT m, WPARAM w, LPARAM l)
{
    return proc ? proc(h, m, w, l) : 0;
}

UINT_PTR SetTimer(HWND, UINT_PTR id, UINT, TIMERPROC)
{
    return id;
}

BOOL KillTimer(HWND, UINT_PTR)
{
    return TRUE;
}

BOOL EnableWindow(HWND, BOOL)
{
    return TRUE;
}

void InitCommonControls(void)
{
}
