// The screens around network play, drawn with SDL's built-in font so they work with touch:
// the start screen (single player / multiplayer) and the lobby that replaces GTA2's Win32 dialog.
#include "Function.hpp"
#include "NetPlay.hpp"
#include "NetUi.hpp"
#include "net_port.hpp"
#include "port_internal.hpp"
#include <cstdio>
#include <string>
#include <vector>

namespace
{
const float kBase = 360.0f; // the layout is designed for a screen this many units high

struct Ui
{
    SDL_Renderer* r = NULL;
    float scale = 1, width = 640;
    float px = 0, py = 0;
    bool tapped = false;
    std::string* field = NULL;
    bool quit = false;

    void Begin()
    {
        tapped = false;
        SDL_SetRenderLogicalPresentation(r, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderViewport(r, NULL);
        SDL_SetRenderClipRect(r, NULL);
        int w = 640, h = 360;
        SDL_GetCurrentRenderOutputSize(r, &w, &h);
        scale = h / kBase;
        width = w / scale;
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
                case SDL_EVENT_FINGER_DOWN:
                    px = e.tfinger.x * width;
                    py = e.tfinger.y * kBase;
                    tapped = true;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (e.button.which != SDL_TOUCH_MOUSEID)
                    {
                        float density = SDL_GetWindowPixelDensity(port::Window());
                        px = e.button.x * density / scale;
                        py = e.button.y * density / scale;
                        tapped = true;
                    }
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    if (field && field->size() < 40)
                    {
                        *field += e.text.text;
                    }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (field && e.key.key == SDLK_BACKSPACE && !field->empty())
                    {
                        field->pop_back();
                    }
                    if (e.key.key == SDLK_RETURN)
                    {
                        SetField(NULL);
                    }
                    break;
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                default:
                    break;
            }
        }
        DebugTap();
        SDL_SetRenderScale(r, 1, 1);
        SDL_SetRenderDrawColor(r, 14, 14, 18, 255);
        SDL_RenderClear(r);
    }

    // Debug: PORT_LOBBY_TAPS="frame:x:y,..." taps in layout units; PORT_LOBBY_SHOTS="frame,..." + PORT_SHOT_DIR saves BMPs
    static int& Frame()
    {
        static int frame = 0;
        return frame;
    }

    void DebugTap()
    {
        Frame()++;
        if (const char* env = getenv("PORT_LOBBY_TAPS"))
        {
            int f;
            float x, y;
            int used = 0;
            while (sscanf(env, "%d:%f:%f%n", &f, &x, &y, &used) == 3)
            {
                if (f == Frame())
                {
                    px = x;
                    py = y;
                    tapped = true;
                }
                env += used;
                if (*env == ',')
                {
                    env++;
                }
            }
        }
    }

    void DebugShot()
    {
        const char* env = getenv("PORT_LOBBY_SHOTS");
        const char* dir = getenv("PORT_SHOT_DIR");
        if (!env || !dir)
        {
            return;
        }
        std::string list = std::string(",") + env + ",";
        if (list.find("," + std::to_string(Frame()) + ",") != std::string::npos)
        {
            if (SDL_Surface* shot = SDL_RenderReadPixels(r, NULL))
            {
                SDL_SaveBMP(shot, (std::string(dir) + "/lobby_" + std::to_string(Frame()) + ".bmp").c_str());
                SDL_DestroySurface(shot);
            }
        }
    }

    void End()
    {
        SDL_SetRenderScale(r, 1, 1);
        DebugShot();
        SDL_RenderPresent(r);
        SDL_Delay(16);
    }

    void SetField(std::string* f)
    {
        field = f;
        if (f)
        {
            SDL_StartTextInput(port::Window());
        }
        else
        {
            SDL_StopTextInput(port::Window());
        }
    }

    void Text(float x, float y, const std::string& s, float size = 2, Uint8 c = 235)
    {
        float k = size * scale;
        SDL_SetRenderScale(r, k, k);
        SDL_SetRenderDrawColor(r, c, c, c, 255);
        SDL_RenderDebugText(r, x * scale / k, y * scale / k, s.c_str());
        SDL_SetRenderScale(r, 1, 1);
    }

    void Rect(float x, float y, float w, float h, Uint8 cr, Uint8 cg, Uint8 cb)
    {
        SDL_FRect f = {x * scale, y * scale, w * scale, h * scale};
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        SDL_RenderFillRect(r, &f);
    }

    bool Hit(float x, float y, float w, float h) const { return tapped && px >= x && px < x + w && py >= y && py < y + h; }

    bool Button(float x, float y, float w, float h, const std::string& label, bool enabled = true)
    {
        Rect(x, y, w, h, enabled ? 150 : 45, enabled ? 28 : 45, enabled ? 28 : 50);
        float tw = label.size() * 16.0f;
        Text(x + (w - tw) / 2, y + (h - 16) / 2, label, 2, enabled ? 255 : 130);
        return enabled && Hit(x, y, w, h);
    }

    void Input(float x, float y, float w, const std::string& label, std::string& value)
    {
        Text(x, y, label, 1.5f, 170);
        bool active = field == &value;
        Rect(x, y + 16, w, 34, active ? 60 : 38, active ? 60 : 38, active ? 72 : 46);
        Text(x + 8, y + 25, value + (active && (SDL_GetTicks() / 500) % 2 ? "_" : ""));
        if (Hit(x, y + 16, w, 34))
        {
            SetField(&value);
        }
    }
};

std::string gName = "Oyuncu", gServer;

std::string ConfigPath()
{
    return port::PrefRoot() + "/network.txt";
}

void LoadConfig()
{
    if (FILE* f = ::fopen(ConfigPath().c_str(), "r"))
    {
        char a[128] = "", b[128] = "";
        if (fgets(a, sizeof(a), f) && fgets(b, sizeof(b), f))
        {
            a[strcspn(a, "\r\n")] = 0;
            b[strcspn(b, "\r\n")] = 0;
            gName = a;
            gServer = b;
        }
        fclose(f);
    }
}

void SaveConfig()
{
    if (FILE* f = ::fopen(ConfigPath().c_str(), "w"))
    {
        fprintf(f, "%s\n%s\n", gName.c_str(), gServer.c_str());
        fclose(f);
    }
}

void ToWide(const std::string& s, wchar_t* out, size_t n)
{
    size_t i = 0;
    for (; i < s.size() && i + 1 < n; i++)
    {
        out[i] = static_cast<unsigned char>(s[i]);
    }
    out[i] = 0;
}

std::string FromWide(const wchar_t* s)
{
    std::string out;
    for (; s && *s; s++)
    {
        out += *s < 128 ? static_cast<char>(*s) : '?';
    }
    return out;
}

const char* Env(const char* name)
{
    return getenv(name);
}

// Filled by the callbacks NetPlay invokes from NetworkTick
struct Lobby
{
    NetUi* ui = NULL;
    std::vector<std::string> sessions, players;
    bool started = false, disconnected = false;
};
Lobby gLobby;
bool gSessionLost = false;

void __stdcall OnSessions(void*, Network_NameList* list)
{
    gLobby.sessions.clear();
    for (u32 i = 0; i < list->field_40_count; i++)
    {
        gLobby.sessions.push_back(FromWide(list->field_0_names[i]));
    }
}

void __stdcall OnPlayerAdded(void*, wchar_t* name, void*)
{
    gLobby.players.push_back(FromWide(name));
}

void __stdcall OnPlayerRemoved(void*, wchar_t* name)
{
    std::string n = FromWide(name);
    for (size_t i = 0; i < gLobby.players.size(); i++)
    {
        if (gLobby.players[i] == n)
        {
            gLobby.players.erase(gLobby.players.begin() + static_cast<long>(i));
            break;
        }
    }
}

void __stdcall OnDisconnected(void*)
{
    gLobby.disconnected = true;
}

void __stdcall OnSettings(void*, const char** data)
{
    gLobby.ui->ApplyHostGameSettings_51B810(*data);
}

void __stdcall OnStarted(void*)
{
    gLobby.started = true;
}

void __stdcall OnIgnored(void*, void*)
{
}

void PushSettings(NetUi* ui)
{
    NetworkGameSettings& s = ui->field_20088_game_settings;
    strcpy(s.field_20088_default_map, ui->field_4_maps[s.field_2018C_map_idx].field_410_mmp_name);
    Network_Buffer data;
    data.field_0_data = &s;
    data.field_4_len = sizeof(s);
    gNetPlay_7071E8.SetGameSettings_521170(&data);
}

const char* GameTypeName(u32 type)
{
    return type == 1 ? "FRAG" : type == 2 ? "PUAN" : "EBELEME";
}

bool Host(NetUi* ui)
{
    NetworkGameSettings settings = ui->field_20088_game_settings;
    strcpy(settings.field_20088_default_map, ui->field_4_maps[settings.field_2018C_map_idx].field_410_mmp_name);
    Network_Buffer group, player;
    group.field_0_data = &settings;
    group.field_4_len = sizeof(settings);
    player.field_0_data = NULL;
    player.field_4_len = 0;
    wchar_t name[64], session[80];
    ToWide(gName, name, 64);
    ToWide(gName + "'s Game", session, 80);
    return gNetPlay_7071E8.HostSession_51E7A0(session, name, &group, &player) != 0;
}

bool Join(NetUi* ui, u32 idx)
{
    Network_Buffer group, player;
    group.field_0_data = NULL;
    group.field_4_len = 0;
    player.field_0_data = NULL;
    player.field_4_len = 0;
    wchar_t name[64];
    ToWide(gName, name, 64);
    if (!gNetPlay_7071E8.JoinSession_520570(idx, name, &player, &group))
    {
        return false;
    }
    ui->ApplyHostGameSettings_51B810(static_cast<const char*>(group.field_0_data));
    return true;
}

u32 RequiredPlayers(NetUi* ui)
{
    return ui->field_4_maps[ui->field_20088_game_settings.field_2018C_map_idx].field_514_player_count;
}

void StartGame()
{
    gNetPlay_7071E8.NetworkTick_51ED00();
    SDL_Delay(100);
    gNetPlay_7071E8.DisableJoining_521220();
}
} // namespace

namespace port
{
void NetNotifySessionLost()
{
    gSessionLost = true;
}

bool RunStartScreen()
{
    LoadConfig();
    if (const char* n = Env("PORT_NET_NAME"))
    {
        gName = n;
    }
    if (const char* s = Env("PORT_NET_SERVER"))
    {
        gServer = s;
    }
    if (Env("PORT_NET_AUTO"))
    {
        std::string error;
        return NetConnect(gServer, error);
    }
    if (Env("GTA2_SINGLE") || !CreateWindowAndRenderer("GTA2", 640, 480))
    {
        return false;
    }
    Ui ui;
    ui.r = Renderer();
    bool setup = false;
    std::string error;
    for (;;)
    {
        ui.Begin();
        if (ui.quit)
        {
            return false;
        }
        float cx = ui.width / 2;
        ui.Text(cx - 60, 30, "GTA2", 5);
        if (!setup)
        {
            if (ui.Button(cx - 150, 130, 300, 60, "TEK OYUNCU"))
            {
                ui.SetField(NULL);
                ui.End();
                return false;
            }
            if (ui.Button(cx - 150, 210, 300, 60, "COK OYUNCULU"))
            {
                setup = true;
            }
        }
        else
        {
            ui.Input(cx - 200, 90, 400, "ADIN", gName);
            ui.Input(cx - 200, 155, 400, "SUNUCU (adres:port)", gServer);
            if (!error.empty())
            {
                ui.Text(cx - 200, 216, error, 1.5f, 255);
            }
            if (ui.Button(cx - 200, 244, 190, 52, "GERI"))
            {
                ui.SetField(NULL);
                setup = false;
                error.clear();
            }
            if (ui.Button(cx + 10, 244, 190, 52, "BAGLAN", !gName.empty() && !gServer.empty()))
            {
                ui.SetField(NULL);
                ui.Text(cx - 200, 216, "Baglaniliyor...", 1.5f, 255);
                ui.End();
                if (NetConnect(gServer, error))
                {
                    SaveConfig();
                    return true;
                }
                continue;
            }
        }
        ui.End();
    }
}

bool RunNetLobby(NetUi* netUi)
{
    gLobby = Lobby();
    gLobby.ui = netUi;
    gSessionLost = false;
    netUi->EnumerateMaps_51BFA0();
    if (netUi->field_1FD64_total_map_count == 0)
    {
        Log("net: no multiplayer maps found\n");
        return false;
    }
    netUi->LoadGameSettingsFromRegistry_51C830();
    NetworkGameSettings& s = netUi->field_20088_game_settings;
    if (s.field_20194_frag_limit == 0)
    {
        s.field_20194_frag_limit = 10;
    }

    NetPlay& np = gNetPlay_7071E8;
    np.Set15_51ECD0(reinterpret_cast<sptr>(OnSessions), netUi);
    np.Set6_520530(reinterpret_cast<void*>(OnPlayerAdded), netUi);
    np.Set9_520E60(reinterpret_cast<sptr>(OnPlayerRemoved), reinterpret_cast<sptr>(netUi));
    np.Set3_Disconnect_520E80(reinterpret_cast<sptr>(OnDisconnected), reinterpret_cast<sptr>(netUi));
    np.Set18_520F50(reinterpret_cast<sptr>(OnIgnored), reinterpret_cast<sptr>(netUi));
    np.Set21_5210D0(reinterpret_cast<sptr>(OnIgnored), reinterpret_cast<sptr>(netUi));
    np.Set24_521140(reinterpret_cast<sptr>(OnSettings), reinterpret_cast<sptr>(netUi));
    np.Set27SavePlayerName_5211F0(reinterpret_cast<sptr>(OnStarted), reinterpret_cast<sptr>(netUi));

    enum
    {
        Browse,
        HostLobby,
        GuestLobby
    } screen = Browse;

    // Automated test: PORT_NET_AUTO=host:<map description part> or join
    const char* autoMode = Env("PORT_NET_AUTO");
    if (autoMode && strncmp(autoMode, "host", 4) == 0)
    {
        const char* want = strchr(autoMode, ':');
        for (int i = 0; want && i < netUi->field_1FD64_total_map_count; i++)
        {
            if (strstr(netUi->field_4_maps[i].field_30C_description, want + 1))
            {
                s.field_2018C_map_idx = i;
                break;
            }
        }
        if (!Host(netUi))
        {
            return false;
        }
        screen = HostLobby;
    }

    if (!CreateWindowAndRenderer("GTA2", 640, 480))
    {
        return false;
    }
    Ui ui;
    ui.r = Renderer();
    std::string status;
    for (;;)
    {
        np.NetworkTick_51ED00();
        NetKeepAlive();
        if (gLobby.started)
        {
            return true;
        }
        if (gSessionLost || gLobby.disconnected || !NetConnected())
        {
            Log("net: lobby closed (lost=%d kicked=%d)\n", gSessionLost, gLobby.disconnected);
            NetClose();
            return false;
        }
        if (autoMode)
        {
            if (screen == Browse && !gLobby.sessions.empty() && strncmp(autoMode, "join", 4) == 0 && Join(netUi, 0))
            {
                screen = GuestLobby;
            }
            if (screen == HostLobby && gLobby.players.size() == RequiredPlayers(netUi))
            {
                StartGame();
                return true;
            }
        }

        ui.Begin();
        if (ui.quit)
        {
            return false;
        }
        float w = ui.width;
        if (screen == Browse)
        {
            ui.Text(24, 18, "COK OYUNCULU", 3);
            ui.Text(24, 62, "Acik oyunlar:", 1.5f, 170);
            float y = 84;
            for (size_t i = 0; i < gLobby.sessions.size() && i < 5; i++)
            {
                if (ui.Button(24, y, w - 48, 40, gLobby.sessions[i]))
                {
                    if (Join(netUi, static_cast<u32>(i)))
                    {
                        screen = GuestLobby;
                    }
                    else
                    {
                        status = "Oyuna katilinamadi";
                    }
                    break;
                }
                y += 46;
            }
            if (gLobby.sessions.empty())
            {
                ui.Text(24, 100, "Henuz acik oyun yok. Yeni bir oyun kurabilirsin.", 1.5f, 170);
            }
            ui.Text(24, 306, status, 1.5f, 255);
            if (ui.Button(24, 322 - 40 + 30, 170, 42, "CIK"))
            {
                NetClose();
                return false;
            }
            if (ui.Button(w - 224, 312, 200, 42, "OYUN KUR"))
            {
                if (Host(netUi))
                {
                    screen = HostLobby;
                }
                else
                {
                    status = "Oyun kurulamadi";
                }
            }
        }
        else
        {
            bool host = screen == HostLobby;
            ui.Text(24, 14, host ? "OYUNUN" : "OYUNA KATILDIN", 2.5f);
            ui.Text(24, 54, "Oyuncular:", 1.5f, 170);
            float y = 74;
            for (const std::string& p : gLobby.players)
            {
                ui.Text(32, y, p);
                y += 24;
            }
            float cx = w < 560 ? 190 : 280;
            const Network_Enumerated_Map& map = netUi->field_4_maps[s.field_2018C_map_idx];
            ui.Text(cx, 54, "Harita:", 1.5f, 170);
            ui.Text(cx, 72, map.field_30C_description, 1.5f);
            u32 need = map.field_514_player_count;
            char line[128];
            snprintf(line, sizeof(line), "Gereken oyuncu: %u   Simdi: %u", need, static_cast<u32>(gLobby.players.size()));
            ui.Text(cx, 92, line, 1.5f, 170);
            snprintf(line, sizeof(line), "Tur: %s   Limit: %u", GameTypeName(s.field_20198_game_type), s.field_20194_frag_limit);
            ui.Text(cx, 110, line, 1.5f, 170);
            snprintf(line, sizeof(line), "Polis: %s", s.field_201A0_police_on ? "ACIK" : "KAPALI");
            ui.Text(cx, 128, line, 1.5f, 170);
            if (host)
            {
                bool changed = false;
                int count = netUi->field_1FD64_total_map_count;
                if (ui.Button(cx, 152, 60, 40, "<"))
                {
                    s.field_2018C_map_idx = (s.field_2018C_map_idx + count - 1) % count;
                    changed = true;
                }
                if (ui.Button(cx + 70, 152, 60, 40, ">"))
                {
                    s.field_2018C_map_idx = (s.field_2018C_map_idx + 1) % count;
                    changed = true;
                }
                if (ui.Button(cx, 200, 60, 40, "TUR"))
                {
                    s.field_20198_game_type = s.field_20198_game_type == 1 ? 2 : s.field_20198_game_type == 2 ? 0 : 1;
                    changed = true;
                }
                if (ui.Button(cx + 70, 200, 90, 40, "POLIS"))
                {
                    s.field_201A0_police_on = !s.field_201A0_police_on;
                    changed = true;
                }
                if (ui.Button(cx, 248, 60, 40, "-") && s.field_20194_frag_limit > 1)
                {
                    s.field_20194_frag_limit--;
                    changed = true;
                }
                if (ui.Button(cx + 70, 248, 60, 40, "+") && s.field_20194_frag_limit < 99)
                {
                    s.field_20194_frag_limit++;
                    changed = true;
                }
                if (changed)
                {
                    PushSettings(netUi);
                }
                if (ui.Button(w - 224, 312, 200, 42, "BASLAT", gLobby.players.size() == need))
                {
                    ui.End();
                    StartGame();
                    return true;
                }
            }
            else
            {
                ui.Text(cx, 170, "Kurucunun baslatmasi bekleniyor...", 1.5f, 170);
            }
            if (ui.Button(24, 312, 170, 42, "CIK"))
            {
                np.Disconnect_520D10();
                NetClose();
                return false;
            }
        }
        ui.End();
    }
}
} // namespace port
