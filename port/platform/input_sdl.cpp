// DirectInput 7 keyboard on top of SDL3 keyboard state, plus the SDL event pump that feeds
// the game's window procedure.
#include "port_internal.hpp"
#include <dinput.h>
#include <deque>
#include <string>
#include <vector>

namespace
{
unsigned char gDik[256];

// Buffered keyboard events for IDirectInputDevice::GetDeviceData
struct KeyEvent
{
    DWORD ofs, data, time, seq;
};
std::deque<KeyEvent>& KeyEvents()
{
    static std::deque<KeyEvent> q;
    return q;
}
DWORD gKeySeq = 0;

// Each input source (keyboard, touch overlay, gamepad, debug auto-keys) keeps its own key
// map; the DirectInput state is their union.
unsigned char gSources[port::kInputSourceCount][256];

void SetKey(unsigned char dik, bool down);

void SetSourceKey(int src, unsigned char dik, bool down)
{
    gSources[src][dik] = down ? 1 : 0;
    bool any = false;
    for (int i = 0; i < port::kInputSourceCount; i++)
    {
        any = any || gSources[i][dik];
    }
    SetKey(dik, any);
}

void SetKey(unsigned char dik, bool down)
{
    unsigned char v = down ? 0x80 : 0;
    if (gDik[dik] == v)
    {
        return;
    }
    gDik[dik] = v;
    KeyEvents().push_back({dik, v, static_cast<DWORD>(SDL_GetTicks()), gKeySeq++});
    if (KeyEvents().size() > 256)
    {
        KeyEvents().pop_front();
    }
}

struct ScanMap
{
    SDL_Scancode sc;
    unsigned char dik;
};

const ScanMap kScanMap[] = {
    {SDL_SCANCODE_ESCAPE, DIK_ESCAPE},
    {SDL_SCANCODE_1, DIK_1},
    {SDL_SCANCODE_2, DIK_2},
    {SDL_SCANCODE_3, DIK_3},
    {SDL_SCANCODE_4, DIK_4},
    {SDL_SCANCODE_5, DIK_5},
    {SDL_SCANCODE_6, DIK_6},
    {SDL_SCANCODE_7, DIK_7},
    {SDL_SCANCODE_8, DIK_8},
    {SDL_SCANCODE_9, DIK_9},
    {SDL_SCANCODE_0, DIK_0},
    {SDL_SCANCODE_MINUS, DIK_MINUS},
    {SDL_SCANCODE_EQUALS, DIK_EQUALS},
    {SDL_SCANCODE_BACKSPACE, DIK_BACK},
    {SDL_SCANCODE_TAB, DIK_TAB},
    {SDL_SCANCODE_Q, DIK_Q},
    {SDL_SCANCODE_W, DIK_W},
    {SDL_SCANCODE_E, DIK_E},
    {SDL_SCANCODE_R, DIK_R},
    {SDL_SCANCODE_T, DIK_T},
    {SDL_SCANCODE_Y, DIK_Y},
    {SDL_SCANCODE_U, DIK_U},
    {SDL_SCANCODE_I, DIK_I},
    {SDL_SCANCODE_O, DIK_O},
    {SDL_SCANCODE_P, DIK_P},
    {SDL_SCANCODE_LEFTBRACKET, DIK_LBRACKET},
    {SDL_SCANCODE_RIGHTBRACKET, DIK_RBRACKET},
    {SDL_SCANCODE_RETURN, DIK_RETURN},
    {SDL_SCANCODE_LCTRL, DIK_LCONTROL},
    {SDL_SCANCODE_A, DIK_A},
    {SDL_SCANCODE_S, DIK_S},
    {SDL_SCANCODE_D, DIK_D},
    {SDL_SCANCODE_F, DIK_F},
    {SDL_SCANCODE_G, DIK_G},
    {SDL_SCANCODE_H, DIK_H},
    {SDL_SCANCODE_J, DIK_J},
    {SDL_SCANCODE_K, DIK_K},
    {SDL_SCANCODE_L, DIK_L},
    {SDL_SCANCODE_SEMICOLON, DIK_SEMICOLON},
    {SDL_SCANCODE_APOSTROPHE, DIK_APOSTROPHE},
    {SDL_SCANCODE_GRAVE, DIK_GRAVE},
    {SDL_SCANCODE_LSHIFT, DIK_LSHIFT},
    {SDL_SCANCODE_BACKSLASH, DIK_BACKSLASH},
    {SDL_SCANCODE_Z, DIK_Z},
    {SDL_SCANCODE_X, DIK_X},
    {SDL_SCANCODE_C, DIK_C},
    {SDL_SCANCODE_V, DIK_V},
    {SDL_SCANCODE_B, DIK_B},
    {SDL_SCANCODE_N, DIK_N},
    {SDL_SCANCODE_M, DIK_M},
    {SDL_SCANCODE_COMMA, DIK_COMMA},
    {SDL_SCANCODE_PERIOD, DIK_PERIOD},
    {SDL_SCANCODE_SLASH, DIK_SLASH},
    {SDL_SCANCODE_RSHIFT, DIK_RSHIFT},
    {SDL_SCANCODE_KP_MULTIPLY, DIK_MULTIPLY},
    {SDL_SCANCODE_LALT, DIK_LMENU},
    {SDL_SCANCODE_SPACE, DIK_SPACE},
    {SDL_SCANCODE_CAPSLOCK, DIK_CAPITAL},
    {SDL_SCANCODE_F1, DIK_F1},
    {SDL_SCANCODE_F2, DIK_F2},
    {SDL_SCANCODE_F3, DIK_F3},
    {SDL_SCANCODE_F4, DIK_F4},
    {SDL_SCANCODE_F5, DIK_F5},
    {SDL_SCANCODE_F6, DIK_F6},
    {SDL_SCANCODE_F7, DIK_F7},
    {SDL_SCANCODE_F8, DIK_F8},
    {SDL_SCANCODE_F9, DIK_F9},
    {SDL_SCANCODE_F10, DIK_F10},
    {SDL_SCANCODE_NUMLOCKCLEAR, DIK_NUMLOCK},
    {SDL_SCANCODE_SCROLLLOCK, DIK_SCROLL},
    {SDL_SCANCODE_KP_7, DIK_NUMPAD7},
    {SDL_SCANCODE_KP_8, DIK_NUMPAD8},
    {SDL_SCANCODE_KP_9, DIK_NUMPAD9},
    {SDL_SCANCODE_KP_MINUS, DIK_SUBTRACT},
    {SDL_SCANCODE_KP_4, DIK_NUMPAD4},
    {SDL_SCANCODE_KP_5, DIK_NUMPAD5},
    {SDL_SCANCODE_KP_6, DIK_NUMPAD6},
    {SDL_SCANCODE_KP_PLUS, DIK_ADD},
    {SDL_SCANCODE_KP_1, DIK_NUMPAD1},
    {SDL_SCANCODE_KP_2, DIK_NUMPAD2},
    {SDL_SCANCODE_KP_3, DIK_NUMPAD3},
    {SDL_SCANCODE_KP_0, DIK_NUMPAD0},
    {SDL_SCANCODE_KP_PERIOD, DIK_DECIMAL},
    {SDL_SCANCODE_F11, DIK_F11},
    {SDL_SCANCODE_F12, DIK_F12},
    {SDL_SCANCODE_KP_ENTER, DIK_NUMPADENTER},
    {SDL_SCANCODE_RCTRL, DIK_RCONTROL},
    {SDL_SCANCODE_KP_DIVIDE, DIK_DIVIDE},
    {SDL_SCANCODE_RALT, DIK_RMENU},
    {SDL_SCANCODE_HOME, DIK_HOME},
    {SDL_SCANCODE_UP, DIK_UP},
    {SDL_SCANCODE_PAGEUP, DIK_PRIOR},
    {SDL_SCANCODE_LEFT, DIK_LEFT},
    {SDL_SCANCODE_RIGHT, DIK_RIGHT},
    {SDL_SCANCODE_END, DIK_END},
    {SDL_SCANCODE_DOWN, DIK_DOWN},
    {SDL_SCANCODE_PAGEDOWN, DIK_NEXT},
    {SDL_SCANCODE_INSERT, DIK_INSERT},
    {SDL_SCANCODE_DELETE, DIK_DELETE},
    {SDL_SCANCODE_LGUI, DIK_LWIN},
    {SDL_SCANCODE_RGUI, DIK_RWIN},
};

// Virtual key codes for WM_KEYDOWN/WM_SYSKEYDOWN (only the ones WindowProc looks at)
WPARAM SdlToVk(SDL_Keycode k)
{
    switch (k)
    {
        case SDLK_ESCAPE:
            return VK_ESCAPE;
        case SDLK_RETURN:
            return VK_RETURN;
        case SDLK_F4:
            return VK_F4;
        case SDLK_F10:
            return VK_F10;
        case SDLK_TAB:
            return VK_TAB;
        case SDLK_SPACE:
            return VK_SPACE;
        default:
            return k < 0x80 ? static_cast<WPARAM>(toupper(static_cast<int>(k))) : 0;
    }
}
} // namespace

namespace port
{
const unsigned char* DikState()
{
    return gDik;
}

unsigned char SdlToDik(SDL_Scancode sc)
{
    for (const ScanMap& m : kScanMap)
    {
        if (m.sc == sc)
        {
            return m.dik;
        }
    }
    return 0;
}

// Debug: PORT_AUTOKEYS="120:1c,200:d0" presses DIK 0x1c at frame 120 (held ~8 frames), ...
void AutoKeys(int frame)
{
    struct AutoKey
    {
        int frame, dik, hold;
    };
    static std::vector<AutoKey> keys;
    static bool parsed = false;
    if (!parsed)
    {
        parsed = true;
        if (const char* env = getenv("PORT_AUTOKEYS"))
        {
            std::string s(env);
            size_t pos = 0;
            while (pos < s.size())
            {
                size_t comma = s.find(',', pos);
                std::string item = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                size_t colon = item.find(':');
                if (colon != std::string::npos)
                {
                    size_t colon2 = item.find(':', colon + 1);
                    int hold = colon2 != std::string::npos ? atoi(item.c_str() + colon2 + 1) : 8;
                    keys.push_back({atoi(item.c_str()), static_cast<int>(strtol(item.c_str() + colon + 1, NULL, 16)), hold});
                }
                if (comma == std::string::npos)
                {
                    break;
                }
                pos = comma + 1;
            }
        }
    }
    for (auto& k : keys)
    {
        if (frame == k.frame)
        {
            SetSourceKey(port::kInputAuto, static_cast<unsigned char>(k.dik), true);
        }
        else if (frame == k.frame + k.hold)
        {
            SetSourceKey(port::kInputAuto, static_cast<unsigned char>(k.dik), false);
        }
    }
}

// Debug: PORT_AUTOTOUCH="frame:x:y:hold,..." (x,y normalised 0..1) injects SDL finger events,
// exercising the touch overlay exactly like real touches.
void AutoTouch(int frame)
{
    struct T
    {
        int frame;
        float x, y;
        int hold;
        float dx, dy; // optional drag after touching down (joystick)
        SDL_FingerID id;
    };
    static std::vector<T> touches;
    static bool parsed = false;
    if (!parsed)
    {
        parsed = true;
        if (const char* env = getenv("PORT_AUTOTOUCH"))
        {
            std::string s(env);
            size_t pos = 0;
            SDL_FingerID nextId = 1000;
            while (pos < s.size())
            {
                size_t comma = s.find(',', pos);
                std::string item = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                T t{};
                if (sscanf(item.c_str(), "%d:%f:%f:%d:%f:%f", &t.frame, &t.x, &t.y, &t.hold, &t.dx, &t.dy) >= 3)
                {
                    if (t.hold <= 0)
                    {
                        t.hold = 8;
                    }
                    t.id = nextId++;
                    touches.push_back(t);
                }
                if (comma == std::string::npos)
                {
                    break;
                }
                pos = comma + 1;
            }
        }
    }
    for (const T& t : touches)
    {
        if (frame == t.frame || frame == t.frame + t.hold)
        {
            SDL_Event e;
            SDL_zero(e);
            e.type = frame == t.frame ? SDL_EVENT_FINGER_DOWN : SDL_EVENT_FINGER_UP;
            e.tfinger.fingerID = t.id;
            e.tfinger.x = t.x;
            e.tfinger.y = t.y;
            e.tfinger.pressure = 1.0f;
            SDL_PushEvent(&e);
        }
        if ((t.dx != 0 || t.dy != 0) && frame == t.frame + 2)
        {
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_FINGER_MOTION;
            e.tfinger.fingerID = t.id;
            e.tfinger.x = t.x + t.dx;
            e.tfinger.y = t.y + t.dy;
            e.tfinger.pressure = 1.0f;
            SDL_PushEvent(&e);
        }
    }
}

void PumpEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                PostMsg(WM_CLOSE, 0, 0);
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                const bool down = e.type == SDL_EVENT_KEY_DOWN;
                unsigned char dik = SdlToDik(e.key.scancode);
                if (dik)
                {
                    SetSourceKey(kInputKeyboard, dik, down);
                }
                // Cmd+Q / Alt+F4 style quit
                if (down && e.key.key == SDLK_Q && (e.key.mod & SDL_KMOD_GUI))
                {
                    PostMsg(WM_CLOSE, 0, 0);
                }
                // Alt+Enter toggles full screen
                if (down && e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT) && Window())
                {
                    bool fs = (SDL_GetWindowFlags(Window()) & SDL_WINDOW_FULLSCREEN) != 0;
                    SDL_SetWindowFullscreen(Window(), !fs);
                }
                WPARAM vk = SdlToVk(e.key.key);
                if (vk && !e.key.repeat)
                {
                    const bool alt = (e.key.mod & SDL_KMOD_ALT) != 0;
                    PostMsg(down ? (alt ? WM_SYSKEYDOWN : WM_KEYDOWN) : (alt ? WM_SYSKEYUP : WM_KEYUP), vk, 0);
                }
                break;
            }

            // iOS: the app going to the background pauses the game like losing focus on Windows
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                for (int i = 0; i < 256; i++)
                {
                    SetSourceKey(kInputTouch, static_cast<unsigned char>(i), false);
                    SetSourceKey(kInputKeyboard, static_cast<unsigned char>(i), false);
                }
                PostMsg(WM_ACTIVATE, WA_INACTIVE, 0);
                break;

            case SDL_EVENT_DID_ENTER_FOREGROUND:
                PostMsg(WM_ACTIVATE, WA_ACTIVE, 0);
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                if (!getenv("PORT_IGNORE_FOCUS"))
                {
                    PostMsg(WM_ACTIVATE, WA_ACTIVE, 0);
                }
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (!getenv("PORT_IGNORE_FOCUS"))
                {
                    for (int i = 0; i < 256; i++)
                    {
                        SetSourceKey(kInputKeyboard, static_cast<unsigned char>(i), false);
                    }
                    PostMsg(WM_ACTIVATE, WA_INACTIVE, 0);
                }
                break;

            default:
                HandleTouchEvent(e);
                HandleGamepadEvent(e);
                break;
        }
    }
}

void SetInput(int source, unsigned char dik, bool down)
{
    SetSourceKey(source, dik, down);
}
} // namespace port

// ===========================================================================
// DirectInput 7
// ===========================================================================
namespace
{
struct PortKeyboardDevice : public IDirectInputDeviceA
{
    ULONG mRefs = 1;

    HRESULT QueryInterface(REFIID, LPVOID* out) override
    {
        *out = this;
        AddRef();
        return DI_OK;
    }
    ULONG AddRef() override
    {
        return ++mRefs;
    }
    ULONG Release() override
    {
        return --mRefs;
    }
    HRESULT GetCapabilities(LPDIDEVCAPS) override
    {
        return DI_OK;
    }
    HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) override
    {
        return DI_OK;
    }
    HRESULT GetProperty(REFGUID, LPDIPROPHEADER hdr) override
    {
        if (hdr && hdr->dwSize >= sizeof(DIPROPDWORD))
        {
            reinterpret_cast<DIPROPDWORD*>(hdr)->dwData = 16;
        }
        return DI_OK;
    }
    HRESULT SetProperty(REFGUID, LPCDIPROPHEADER) override
    {
        return DI_OK;
    }
    HRESULT Acquire() override
    {
        return DI_OK;
    }
    HRESULT Unacquire() override
    {
        return DI_OK;
    }
    HRESULT GetDeviceState(DWORD size, LPVOID data) override
    {
        port::PumpEvents();
        memset(data, 0, size);
        memcpy(data, port::DikState(), size < 256 ? size : 256);
        return DI_OK;
    }
    HRESULT GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags) override
    {
        port::PumpEvents();
        if (!count)
        {
            return DI_OK;
        }
        auto& q = KeyEvents();
        const bool peek = (flags & DIGDD_PEEK) != 0;
        if (!data)
        {
            // Query (count == INFINITE flushes unless peeking)
            DWORD n = static_cast<DWORD>(q.size());
            if (!peek && *count == static_cast<DWORD>(-1))
            {
                q.clear();
            }
            *count = n;
            return DI_OK;
        }
        DWORD n = 0;
        while (n < *count && n < q.size())
        {
            const KeyEvent& e = q[n];
            DIDEVICEOBJECTDATA* d = reinterpret_cast<DIDEVICEOBJECTDATA*>(reinterpret_cast<unsigned char*>(data) + n * cbObjectData);
            d->dwOfs = e.ofs;
            d->dwData = e.data;
            d->dwTimeStamp = e.time;
            d->dwSequence = e.seq;
            n++;
        }
        if (!peek)
        {
            q.erase(q.begin(), q.begin() + n);
        }
        *count = n;
        return DI_OK;
    }
    HRESULT SetDataFormat(LPCDIDATAFORMAT) override
    {
        return DI_OK;
    }
    HRESULT SetEventNotification(HANDLE) override
    {
        return DI_OK;
    }
    HRESULT SetCooperativeLevel(HWND, DWORD) override
    {
        return DI_OK;
    }
    HRESULT GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) override
    {
        return DIERR_OBJECTNOTFOUND;
    }
    HRESULT GetDeviceInfo(LPDIDEVICEINSTANCEA) override
    {
        return DI_OK;
    }
    HRESULT RunControlPanel(HWND, DWORD) override
    {
        return DI_OK;
    }
    HRESULT Initialize(HINSTANCE, DWORD, REFGUID) override
    {
        return DI_OK;
    }
};

PortKeyboardDevice gKeyboard;

struct PortDirectInput : public IDirectInputA
{
    ULONG mRefs = 1;

    HRESULT QueryInterface(REFIID, LPVOID* out) override
    {
        *out = this;
        AddRef();
        return DI_OK;
    }
    ULONG AddRef() override
    {
        return ++mRefs;
    }
    ULONG Release() override
    {
        return --mRefs;
    }
    HRESULT CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICEA* out, LPUNKNOWN) override
    {
        if (guid == GUID_SysKeyboard)
        {
            gKeyboard.AddRef();
            *out = &gKeyboard;
            return DI_OK;
        }
        *out = NULL;
        return DIERR_DEVICENOTREG;
    }
    HRESULT EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) override
    {
        // No joysticks through DirectInput (game controllers are mapped to the keyboard)
        return DI_OK;
    }
    HRESULT GetDeviceStatus(REFGUID) override
    {
        return DI_OK;
    }
    HRESULT RunControlPanel(HWND, DWORD) override
    {
        return DI_OK;
    }
    HRESULT Initialize(HINSTANCE, DWORD) override
    {
        return DI_OK;
    }
};

PortDirectInput gDirectInput;
} // namespace

extern "C" HRESULT DirectInputCreateA(HINSTANCE, DWORD, LPDIRECTINPUTA* out, LPUNKNOWN)
{
    gDirectInput.AddRef();
    *out = &gDirectInput;
    return DI_OK;
}
