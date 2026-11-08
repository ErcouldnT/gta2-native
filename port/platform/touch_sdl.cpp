// On-screen touch controls and game controller support. Both are translated into the
// game's default keyboard bindings (gDefaultControls_61A9E4), so no game code changes:
//   arrows = move/steer, LCtrl = attack, Return = enter/exit car (and menu select),
//   Space = jump/handbrake, Z/X = previous/next weapon, Tab = special, LAlt = camera pan, Escape = menu.
#include "port_internal.hpp"
#include <dinput.h>
#include <cmath>
#include <map>
#include <vector>
#include "touch_icons.h"

namespace
{
struct Button
{
    const char* label;
    unsigned char dik;
    float cx, cy, r; // in units of the screen height, cx from the right edge when cx < 0
    const unsigned char* icon; // 96x96 alpha mask (touch_icons.h)
};

// Right-hand cluster, positions relative to screen height (h) from the bottom-right corner
const Button kButtons[] = {
    {"FIRE", DIK_LCONTROL, -0.16f, 0.80f, 0.11f, kIcon_fire},
    {"JUMP", DIK_SPACE, -0.40f, 0.86f, 0.085f, kIcon_jump},
    {"REV", DIK_DOWN, -0.62f, 0.88f, 0.065f, kIcon_rev}, // reverse in a car (held: back up, stick steers)
    {"CAR", DIK_RETURN, -0.12f, 0.55f, 0.085f, kIcon_car},
    {"<W", DIK_Z, -0.42f, 0.62f, 0.06f, kIcon_wprev},
    {"W>", DIK_X, -0.30f, 0.45f, 0.06f, kIcon_wnext},
    {"SPC", DIK_TAB, -0.10f, 0.32f, 0.06f, kIcon_special},
    {"CAM", DIK_LMENU, -0.30f, 0.22f, 0.06f, kIcon_camera}, // held: the stick pans the camera
    {"MENU", DIK_ESCAPE, 0.08f, 0.10f, 0.06f, kIcon_menu},
};
const int kButtonCount = sizeof(kButtons) / sizeof(kButtons[0]);

// Joystick: appears where the left thumb touches (left 40% of the screen)
struct Stick
{
    bool active = false;
    SDL_FingerID finger = 0;
    float ox = 0, oy = 0; // origin (pixels)
    float x = 0, y = 0; // current (pixels)
};
Stick gStick;
std::map<SDL_FingerID, int> gFingerButton; // finger -> button index
bool gButtonDown[16];
bool gTouchSeen = false;
// The overlay hides once a controller is actually used and comes back on the next touch
// (a connected-but-idle controller, like the one the iOS simulator reports, keeps it visible).
bool gPadInUse = false;
float gLastTouchTime = 0;

bool OverlayEnabled()
{
#if defined(SDL_PLATFORM_IOS)
    return !gPadInUse;
#else
    return (gTouchSeen || getenv("PORT_TOUCH") != NULL) && !gPadInUse;
#endif
}

void OutputSize(float& w, float& h)
{
    int iw = 0, ih = 0;
    SDL_GetWindowSizeInPixels(port::Window(), &iw, &ih);
    w = static_cast<float>(iw);
    h = static_cast<float>(ih);
}

void ButtonCenter(const Button& b, float w, float h, float& x, float& y, float& r)
{
    x = b.cx < 0 ? w + b.cx * h : b.cx * h;
    y = b.cy * h;
    r = b.r * h;
}

int HitButton(float px, float py)
{
    float w, h;
    OutputSize(w, h);
    for (int i = 0; i < kButtonCount; i++)
    {
        float x, y, r;
        ButtonCenter(kButtons[i], w, h, x, y, r);
        float dx = px - x, dy = py - y;
        if (dx * dx + dy * dy <= (r * 1.25f) * (r * 1.25f))
        {
            return i;
        }
    }
    return -1;
}

void UpdateStickKeys()
{
    float w, h;
    OutputSize(w, h);
    float x = 0, y = 0, mag = 0;
    if (gStick.active)
    {
        float dx = gStick.x - gStick.ox, dy = gStick.y - gStick.oy;
        float len = sqrtf(dx * dx + dy * dy);
        const float maxR = 0.12f * h;
        if (len > 0.0001f)
        {
            x = dx / len;
            y = dy / len;
            mag = fminf(1.0f, len / maxR);
        }
    }
    if (port::DirectControlEnabled())
    {
        // the per-frame direct controller turns this into keys
        port::SetTouchStick(gStick.active, x, y, mag);
        return;
    }
    bool up = false, down = false, left = false, right = false;
    if (mag > 0.3f)
    {
        right = x > 0.38f;
        left = x < -0.38f;
        down = y > 0.38f;
        up = y < -0.38f;
    }
    port::SetInput(port::kInputTouch, DIK_UP, up);
    port::SetInput(port::kInputTouch, DIK_DOWN, down);
    port::SetInput(port::kInputTouch, DIK_LEFT, left);
    port::SetInput(port::kInputTouch, DIK_RIGHT, right);
}

void SetButton(int idx, bool down)
{
    if (idx < 0)
    {
        return;
    }
    gButtonDown[idx] = down;
    // a key may be on several buttons: recompute it from all of them
    bool any = false;
    for (int i = 0; i < kButtonCount; i++)
    {
        if (kButtons[i].dik == kButtons[idx].dik && gButtonDown[i])
        {
            any = true;
        }
    }
    port::SetInput(port::kInputTouch, kButtons[idx].dik, any);
}

// ---- drawing helpers ----
void FillCircle(float cx, float cy, float r, SDL_FColor c)
{
    const int seg = 40;
    SDL_Vertex v[seg + 2];
    int idx[seg * 3];
    v[0].position = {cx, cy};
    v[0].color = c;
    v[0].tex_coord = {0, 0};
    for (int i = 0; i <= seg; i++)
    {
        float a = static_cast<float>(i) / seg * 6.2831853f;
        v[i + 1].position = {cx + cosf(a) * r, cy + sinf(a) * r};
        v[i + 1].color = c;
        v[i + 1].tex_coord = {0, 0};
    }
    for (int i = 0; i < seg; i++)
    {
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(port::Renderer(), NULL, v, seg + 2, idx, seg * 3);
}

void RingCircle(float cx, float cy, float r, float t, SDL_FColor c)
{
    const int seg = 48;
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    for (int i = 0; i <= seg; i++)
    {
        float a = static_cast<float>(i) / seg * 6.2831853f;
        float ca = cosf(a), sa = sinf(a);
        SDL_Vertex o, in;
        o.position = {cx + ca * r, cy + sa * r};
        in.position = {cx + ca * (r - t), cy + sa * (r - t)};
        o.color = in.color = c;
        o.tex_coord = in.tex_coord = {0, 0};
        v.push_back(o);
        v.push_back(in);
    }
    for (int i = 0; i < seg; i++)
    {
        int a = i * 2;
        idx.insert(idx.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
    }
    SDL_RenderGeometry(port::Renderer(), NULL, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
}

void Label(const char* text, float cx, float cy, float scale)
{
    SDL_Renderer* r = port::Renderer();
    SDL_SetRenderScale(r, scale, scale);
    float tw = static_cast<float>(strlen(text)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 230);
    SDL_RenderDebugText(r, cx / scale - tw / 2, cy / scale - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE / 2.0f, text);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

SDL_Texture* IconTexture(const unsigned char* mask)
{
    static std::map<const unsigned char*, SDL_Texture*> cache;
    auto it = cache.find(mask);
    if (it != cache.end())
    {
        return it->second;
    }
    std::vector<Uint32> px(static_cast<size_t>(kIconSize) * kIconSize);
    for (int i = 0; i < kIconSize * kIconSize; i++)
    {
        px[i] = (static_cast<Uint32>(mask[i]) << 24) | 0xFFFFFF; // white, alpha from the mask
    }
    SDL_Texture* t = SDL_CreateTexture(port::Renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kIconSize, kIconSize);
    if (t)
    {
        SDL_UpdateTexture(t, NULL, px.data(), kIconSize * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    cache[mask] = t;
    return t;
}

// ---- gamepad ----
std::vector<SDL_Gamepad*> gPads;

struct PadMap
{
    SDL_GamepadButton button;
    unsigned char dik;
};
const PadMap kPadMap[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_UP, DIK_UP},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, DIK_DOWN},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, DIK_LEFT},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, DIK_RIGHT},
    {SDL_GAMEPAD_BUTTON_SOUTH, DIK_RETURN}, // A / Cross: enter/exit car, menu select
    {SDL_GAMEPAD_BUTTON_WEST, DIK_LCONTROL}, // X / Square: attack
    {SDL_GAMEPAD_BUTTON_EAST, DIK_SPACE}, // B / Circle: jump / handbrake
    {SDL_GAMEPAD_BUTTON_NORTH, DIK_TAB}, // Y / Triangle: special
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, DIK_Z}, // previous weapon
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, DIK_X}, // next weapon
    {SDL_GAMEPAD_BUTTON_START, DIK_ESCAPE},
    {SDL_GAMEPAD_BUTTON_BACK, DIK_RSHIFT},
};

void PadAxes(SDL_Gamepad* pad)
{
    const float lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    const float ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    const float rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    const float lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    const bool dpadUp = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    const bool dpadDown = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    const bool dpadLeft = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    const bool dpadRight = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    const float mag = sqrtf(lx * lx + ly * ly);
    if (port::DirectControlEnabled() && !dpadUp && !dpadDown && !dpadLeft && !dpadRight && rt < 0.3f && lt < 0.3f)
    {
        // left stick: absolute direction (see direct_control.cpp)
        port::SetPadStick(mag > 0.3f, mag > 0 ? lx / mag : 0, mag > 0 ? ly / mag : 0, mag > 1 ? 1 : mag);
        return;
    }
    port::SetPadStick(false, 0, 0, 0);
    // D-pad / triggers: the original tank controls (R2 accelerate, L2 brake/reverse)
    port::SetInput(port::kInputGamepad, DIK_UP, dpadUp || ly < -0.45f || rt > 0.3f);
    port::SetInput(port::kInputGamepad, DIK_DOWN, dpadDown || ly > 0.45f || lt > 0.3f);
    port::SetInput(port::kInputGamepad, DIK_LEFT, dpadLeft || lx < -0.45f);
    port::SetInput(port::kInputGamepad, DIK_RIGHT, dpadRight || lx > 0.45f);
}
} // namespace

namespace port
{
bool TouchButtonDown(unsigned char dik)
{
    for (int i = 0; i < kButtonCount; i++)
    {
        if (kButtons[i].dik == dik && gButtonDown[i])
        {
            return true;
        }
    }
    return false;
}

bool GamepadConnected()
{
    return !gPads.empty();
}

void HandleGamepadEvent(const SDL_Event& e)
{
    switch (e.type)
    {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which))
            {
                gPads.push_back(pad);
                Log("Gamepad connected: %s\n", SDL_GetGamepadName(pad));
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            for (size_t i = 0; i < gPads.size(); i++)
            {
                if (SDL_GetGamepadID(gPads[i]) == e.gdevice.which)
                {
                    SDL_CloseGamepad(gPads[i]);
                    gPads.erase(gPads.begin() + i);
                    break;
                }
            }
            for (int k = 0; k < 256; k++)
            {
                SetInput(kInputGamepad, static_cast<unsigned char>(k), false);
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            gPadInUse = true;
            // fall through
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            for (const PadMap& m : kPadMap)
            {
                if (m.button == e.gbutton.button && m.dik != DIK_UP && m.dik != DIK_DOWN && m.dik != DIK_LEFT && m.dik != DIK_RIGHT)
                {
                    SetInput(kInputGamepad, m.dik, e.gbutton.down);
                }
            }
            for (SDL_Gamepad* pad : gPads)
            {
                PadAxes(pad);
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e.gaxis.value > 16000 || e.gaxis.value < -16000)
            {
                gPadInUse = true;
            }
            for (SDL_Gamepad* pad : gPads)
            {
                if (SDL_GetGamepadID(pad) == e.gaxis.which)
                {
                    PadAxes(pad);
                }
            }
            break;
        default:
            break;
    }
}

void HandleTouchEvent(const SDL_Event& e)
{
    if (e.type != SDL_EVENT_FINGER_DOWN && e.type != SDL_EVENT_FINGER_MOTION && e.type != SDL_EVENT_FINGER_UP &&
        e.type != SDL_EVENT_FINGER_CANCELED)
    {
        return;
    }
    gTouchSeen = true;
    gPadInUse = false;
    float w, h;
    OutputSize(w, h);
    const float px = e.tfinger.x * w;
    const float py = e.tfinger.y * h;
    const SDL_FingerID id = e.tfinger.fingerID;

    if (e.type == SDL_EVENT_FINGER_DOWN)
    {
        int b = HitButton(px, py);
        if (b >= 0)
        {
            gFingerButton[id] = b;
            SetButton(b, true);
        }
        else if (px < w * 0.45f && !gStick.active)
        {
            gStick.active = true;
            gStick.finger = id;
            gStick.ox = gStick.x = px;
            gStick.oy = gStick.y = py;
            UpdateStickKeys();
        }
    }
    else if (e.type == SDL_EVENT_FINGER_MOTION)
    {
        if (gStick.active && gStick.finger == id)
        {
            gStick.x = px;
            gStick.y = py;
            // drag the origin along when the thumb goes past the rim
            const float maxR = 0.12f * h;
            float dx = gStick.x - gStick.ox, dy = gStick.y - gStick.oy;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > maxR)
            {
                gStick.ox = gStick.x - dx / len * maxR;
                gStick.oy = gStick.y - dy / len * maxR;
            }
            UpdateStickKeys();
        }
        else
        {
            // sliding between buttons
            auto it = gFingerButton.find(id);
            int now = HitButton(px, py);
            if (it != gFingerButton.end() && it->second != now)
            {
                SetButton(it->second, false);
                if (now >= 0)
                {
                    it->second = now;
                    SetButton(now, true);
                }
                else
                {
                    gFingerButton.erase(it);
                }
            }
        }
    }
    else
    {
        if (gStick.active && gStick.finger == id)
        {
            gStick.active = false;
            UpdateStickKeys();
        }
        auto it = gFingerButton.find(id);
        if (it != gFingerButton.end())
        {
            SetButton(it->second, false);
            gFingerButton.erase(it);
        }
    }
}

void DrawTouchOverlay()
{
    SDL_Renderer* r = Renderer();
    if (!r || !OverlayEnabled())
    {
        return;
    }
    // Draw in window pixels, outside the game's logical presentation
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(r, &lw, &lh, &mode);
    SDL_SetRenderLogicalPresentation(r, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    float w, h;
    OutputSize(w, h);
    const SDL_FColor fill = {1, 1, 1, 0.16f};
    const SDL_FColor fillDown = {1, 1, 1, 0.38f};
    const SDL_FColor rim = {1, 1, 1, 0.45f};

    for (int i = 0; i < kButtonCount; i++)
    {
        float x, y, rad;
        ButtonCenter(kButtons[i], w, h, x, y, rad);
        FillCircle(x, y, rad, gButtonDown[i] ? fillDown : fill);
        RingCircle(x, y, rad, 0.006f * h, rim);
        if (SDL_Texture* icon = kButtons[i].icon ? IconTexture(kButtons[i].icon) : NULL)
        {
            const float is = rad * 1.25f;
            SDL_FRect dst = {x - is / 2, y - is / 2, is, is};
            SDL_SetTextureAlphaModFloat(icon, gButtonDown[i] ? 1.0f : 0.85f);
            SDL_RenderTexture(r, icon, NULL, &dst);
        }
        else
        {
            Label(kButtons[i].label, x, y, fmaxf(1.0f, h / 400.0f));
        }
    }

    // Joystick: a hint ring at rest, the live stick while touched
    const float baseR = 0.12f * h;
    float ox = gStick.active ? gStick.ox : 0.17f * h + 0.03f * w;
    float oy = gStick.active ? gStick.oy : 0.75f * h;
    RingCircle(ox, oy, baseR, 0.006f * h, rim);
    FillCircle(ox, oy, baseR, {1, 1, 1, 0.08f});
    float kx = gStick.active ? gStick.x : ox, ky = gStick.active ? gStick.y : oy;
    FillCircle(kx, ky, 0.055f * h, gStick.active ? fillDown : fill);

    SDL_SetRenderLogicalPresentation(r, lw, lh, mode);
}
} // namespace port
