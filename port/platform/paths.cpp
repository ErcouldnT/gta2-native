// File system glue: Windows-style relative paths ("data\\bil.gmp") resolved against the game
// root, case-insensitively (the iOS file system is case-sensitive, the game data is not
// consistently cased). Files the game writes go to the preferences folder, and reads look
// there first so written files shadow shipped ones.
#include "port_internal.hpp"
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include <vector>

namespace
{
// Function-local statics: game globals use the file system from their constructors, before
// this file's own globals are guaranteed to be initialised (Darwin ignores init priorities).
std::string& GameRootRef()
{
    static std::string root;
    return root;
}
std::string& PrefRootRef()
{
    static std::string pref;
    return pref;
}

std::string Normalize(const char* path)
{
    std::string s(path ? path : "");
    for (char& c : s)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    // Drive letters ("c:/...") have no meaning here. The music CD folder (X:\GTAUDIO\) is
    // data/GTAudio in the installed game.
    if (s.size() >= 2 && s[1] == ':')
    {
        s = s.substr(2);
        if (strncasecmp(s.c_str(), "/gtaudio/", 9) == 0)
        {
            s = "data/GTAudio/" + s.substr(9);
        }
        else if (!s.empty() && s[0] == '/')
        {
            // The CD's root (movies etc.) is the installed game's data folder
            s = "data" + s;
        }
    }
    while (s.compare(0, 2, "./") == 0)
    {
        s = s.substr(2);
    }
    return s;
}

bool Exists(const std::string& p)
{
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

// Find a directory entry matching name case-insensitively
bool MatchInDir(const std::string& dir, const std::string& name, std::string& out)
{
    DIR* d = opendir(dir.empty() ? "." : dir.c_str());
    if (!d)
    {
        return false;
    }
    bool found = false;
    while (dirent* e = readdir(d))
    {
        if (strcasecmp(e->d_name, name.c_str()) == 0)
        {
            out = e->d_name;
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

// Resolve rel under base, fixing the case of each component. Returns false if some
// component other than the last doesn't exist (the last may be created).
bool ResolveUnder(const std::string& base, const std::string& rel, std::string& out, bool mustExist)
{
    std::string cur = base;
    size_t pos = 0;
    while (pos <= rel.size())
    {
        size_t slash = rel.find('/', pos);
        std::string comp = rel.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
        bool last = slash == std::string::npos;
        if (!comp.empty() && comp != ".")
        {
            std::string candidate = cur.empty() ? comp : cur + "/" + comp;
            if (!Exists(candidate))
            {
                std::string fixed;
                if (MatchInDir(cur, comp, fixed))
                {
                    candidate = cur.empty() ? fixed : cur + "/" + fixed;
                }
                else if (!last || mustExist)
                {
                    if (!last)
                    {
                        // keep going with the name as given (directory may be created later)
                        cur = candidate;
                        pos = slash + 1;
                        continue;
                    }
                    out = candidate;
                    return false;
                }
            }
            cur = candidate;
        }
        if (last)
        {
            break;
        }
        pos = slash + 1;
    }
    out = cur;
    return Exists(out);
}

bool IsWriteMode(const char* mode)
{
    return mode && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'));
}

void MakeParentDirs(const std::string& path)
{
    for (size_t i = 1; i < path.size(); i++)
    {
        if (path[i] == '/')
        {
            mkdir(path.substr(0, i).c_str(), 0755);
        }
    }
}
} // namespace

namespace port
{
std::string DiscoverGameRoot();

const std::string& GameRoot()
{
    std::string& r = GameRootRef();
    if (r.empty())
    {
        r = DiscoverGameRoot();
        if (r.empty())
        {
            r = ".";
        }
    }
    return r;
}

void SetGameRoot(const std::string& root)
{
    GameRootRef() = root;
}

const std::string& PrefRoot()
{
    std::string& pref = PrefRootRef();
    if (pref.empty())
    {
        char* p = SDL_GetPrefPath("GTA2Port", "GTA2");
        pref = p ? p : "./";
        SDL_free(p);
        if (!pref.empty() && pref.back() == '/')
        {
            pref.pop_back();
        }
    }
    return pref;
}

// For reading: preferences folder first, then game root
std::string ResolveRead(const char* path)
{
    std::string n = Normalize(path);
    if (!n.empty() && n[0] == '/')
    {
        std::string out;
        ResolveUnder("", n.substr(1), out, true);
        return "/" + (out.empty() ? n.substr(1) : out);
    }
    std::string out;
    if (ResolveUnder(PrefRoot(), n, out, true))
    {
        return out;
    }
    ResolveUnder(GameRoot(), n, out, true);
    return out;
}

// For writing: always the preferences folder
std::string ResolveWrite(const char* path)
{
    std::string n = Normalize(path);
    if (!n.empty() && n[0] == '/')
    {
        return n;
    }
    std::string out;
    ResolveUnder(PrefRoot(), n, out, false);
    MakeParentDirs(out);
    return out;
}
} // namespace port

extern "C" {

FILE* port_fopen(const char* path, const char* mode)
{
    if (IsWriteMode(mode))
    {
        std::string w = port::ResolveWrite(path);
        // "r+" / "a" need the existing contents: copy the shipped file across first
        if ((strchr(mode, 'a') || (strchr(mode, 'r') && strchr(mode, '+'))) && !Exists(w))
        {
            std::string r = port::ResolveRead(path);
            if (Exists(r))
            {
                FILE* src = ::fopen(r.c_str(), "rb");
                FILE* dst = ::fopen(w.c_str(), "wb");
                if (src && dst)
                {
                    char buf[65536];
                    size_t n;
                    while ((n = ::fread(buf, 1, sizeof(buf), src)) > 0)
                    {
                        ::fwrite(buf, 1, n, dst);
                    }
                }
                if (src)
                {
                    ::fclose(src);
                }
                if (dst)
                {
                    ::fclose(dst);
                }
            }
        }
        return ::fopen(w.c_str(), mode);
    }
    std::string r = port::ResolveRead(path);
    FILE* f = ::fopen(r.c_str(), mode);
    if (!f)
    {
        port::Log("fopen failed: %s (%s)\n", path, r.c_str());
    }
    return f;
}

const char* port_resolve_path(const char* path, char* out, size_t outSize)
{
    std::string r = port::ResolveRead(path);
    snprintf(out, outSize, "%s", r.c_str());
    return out;
}

int port_remove(const char* path)
{
    std::string w = port::ResolveWrite(path);
    return ::remove(w.c_str());
}

long port_filelength(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        return -1;
    }
    return static_cast<long>(st.st_size);
}

// ---- _findfirst / _findnext ----
struct FindState
{
    std::vector<_finddata_t> entries;
    size_t next;
};

static void AddMatches(const std::string& dir, const std::string& pattern, std::vector<_finddata_t>& out)
{
    DIR* d = opendir(dir.empty() ? "." : dir.c_str());
    if (!d)
    {
        return;
    }
    while (dirent* e = readdir(d))
    {
        if (fnmatch(pattern.c_str(), e->d_name, FNM_CASEFOLD) != 0)
        {
            continue;
        }
        bool dup = false;
        for (const _finddata_t& x : out)
        {
            if (strcasecmp(x.name, e->d_name) == 0)
            {
                dup = true;
            }
        }
        if (dup)
        {
            continue;
        }
        _finddata_t fd;
        memset(&fd, 0, sizeof(fd));
        snprintf(fd.name, sizeof(fd.name), "%s", e->d_name);
        struct stat st;
        std::string full = dir + "/" + e->d_name;
        if (::stat(full.c_str(), &st) == 0)
        {
            fd.size = static_cast<unsigned long>(st.st_size);
            fd.time_write = static_cast<long>(st.st_mtime);
            fd.attrib = S_ISDIR(st.st_mode) ? _A_SUBDIR : _A_NORMAL;
        }
        out.push_back(fd);
    }
    closedir(d);
}

intptr_t port_findfirst(const char* spec, struct _finddata_t* data)
{
    std::string n = Normalize(spec);
    // Bink movies are played from an .mp4 copy; without one, hide the movie so the game skips it
    if (n.size() > 4 && strcasecmp(n.c_str() + n.size() - 4, ".bik") == 0)
    {
        std::string mp4 = n.substr(0, n.size() - 4) + ".mp4";
        std::string r = port::ResolveRead(mp4.c_str());
        struct stat st;
        if (getenv("PORT_NO_MOVIES") || ::stat(r.c_str(), &st) != 0)
        {
            return -1;
        }
    }
    size_t slash = n.rfind('/');
    std::string dirPart = slash == std::string::npos ? "" : n.substr(0, slash);
    std::string pattern = slash == std::string::npos ? n : n.substr(slash + 1);

    FindState* st = new FindState();
    st->next = 0;
    std::string dir;
    ResolveUnder(port::PrefRoot(), dirPart, dir, true);
    AddMatches(dir, pattern, st->entries);
    ResolveUnder(port::GameRoot(), dirPart, dir, true);
    AddMatches(dir, pattern, st->entries);

    if (st->entries.empty())
    {
        delete st;
        return -1;
    }
    *data = st->entries[st->next++];
    return reinterpret_cast<intptr_t>(st);
}

int port_findnext(intptr_t handle, struct _finddata_t* data)
{
    FindState* st = reinterpret_cast<FindState*>(handle);
    if (!st || st->next >= st->entries.size())
    {
        return -1;
    }
    *data = st->entries[st->next++];
    return 0;
}

int port_findclose(intptr_t handle)
{
    delete reinterpret_cast<FindState*>(handle);
    return 0;
}

} // extern "C"
