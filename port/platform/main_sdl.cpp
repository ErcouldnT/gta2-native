// Entry point of the native port: sets up SDL and the game data location, then runs the
// game's own WinMain.
#include "port_internal.hpp"
#include <SDL3/SDL_main.h>
#include "net_port.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

int __stdcall WinMain_5E53F0(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);

static bool IsGameRoot(const std::string& dir)
{
    char resolved[1024];
    std::string probe = dir + "/data";
    struct stat st;
    if (::stat(probe.c_str(), &st) != 0)
    {
        return false;
    }
    (void)resolved;
    return true;
}

static std::string FindGameRoot(int argc, char** argv)
{
    std::vector<std::string> candidates;
    if (const char* env = getenv("GTA2_ROOT"))
    {
        candidates.push_back(env);
    }
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc)
        {
            candidates.push_back(argv[i + 1]);
        }
    }
    // Next to the app / inside the app bundle, then the user's preference folder (iOS: Documents)
    if (const char* base = SDL_GetBasePath())
    {
        candidates.push_back(std::string(base) + "GTA2");
        candidates.push_back(base);
    }
    // iOS: files copied through Finder/Files into the app's Documents folder
    if (const char* home = getenv("HOME"))
    {
        candidates.push_back(std::string(home) + "/Documents/GTA2");
        candidates.push_back(std::string(home) + "/Documents");
    }
    candidates.push_back(port::PrefRoot() + "/GTA2");
    candidates.push_back(".");
    for (const std::string& c : candidates)
    {
        if (IsGameRoot(c))
        {
            return c;
        }
    }
    return "";
}

namespace port
{
// Used by GameRoot() the first time anything asks for it (possibly before main)
std::string DiscoverGameRoot()
{
    return FindGameRoot(0, NULL);
}
} // namespace port

int main(int argc, char** argv)
{
    SDL_SetHint(SDL_HINT_APP_NAME, "GTA2");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    std::string root = FindGameRoot(argc, argv);
    if (root.empty())
    {
#if defined(SDL_PLATFORM_IOS)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "GTA2",
                                 "GTA2 game data not found.\n\n"
                                 "Copy the 'data' folder of the PC version of GTA2 (the free Rockstar release) into "
                                 "this app's 'GTA2' folder:\n"
                                 " - Files app: On My iPhone > GTA2 > GTA2 > data\n"
                                 " - or Finder (Mac): iPhone > Files > GTA2\n\n"
                                 "Then start the app again.",
                                 NULL);
        // Create the folder so it shows up in Files right away
        if (const char* home = getenv("HOME"))
        {
            mkdir((std::string(home) + "/Documents/GTA2").c_str(), 0755);
        }
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GTA2",
                                 "GTA2 game data not found.\n\nSet GTA2_ROOT or pass --root <folder that contains 'data'>.", NULL);
#endif
        return 1;
    }
    port::SetGameRoot(root);
    chdir(root.c_str());
    port::Log("GTA2 data: %s\nPreferences: %s\n", root.c_str(), port::PrefRoot().c_str());

    // Pass through everything after the program name except our own options
    std::string cmdline;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--root") == 0)
        {
            i++;
            continue;
        }
        if (strncmp(argv[i], "-psn_", 5) == 0)
        {
            continue; // macOS Finder launch argument
        }
        if (!cmdline.empty())
        {
            cmdline += " ";
        }
        cmdline += argv[i];
    }
    if (port::RunStartScreen())
    {
        cmdline += cmdline.empty() ? "-N" : " -N"; // the game's "network game" switch
    }
    static char cmdbuf[1024];
    snprintf(cmdbuf, sizeof(cmdbuf), "%s", cmdline.c_str());

    int ret = WinMain_5E53F0(GetModuleHandleA(NULL), NULL, cmdbuf, SW_SHOW);
    SDL_Quit();
    return ret;
}
