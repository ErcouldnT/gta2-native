// State shared between the native platform layer files.
#pragma once

#include <windows.h>
#include <SDL3/SDL.h>
#include <string>

namespace port
{
// Game data root: the folder that holds "data/" (gta2 install dir)
const std::string& GameRoot();
void SetGameRoot(const std::string& root);

// Writable folder for settings/saves (registry file, player.dat, saves)
const std::string& PrefRoot();

// The single SDL window/renderer used for the game "HWND"
SDL_Window* Window();
SDL_Renderer* Renderer();
bool CreateWindowAndRenderer(const char* title, int w, int h);

// Display-shaped game resolution (0x0 when not used)
void WidescreenSize(int& w, int& h);
void ApplyWidescreenDefault();

// Logical game resolution (what the game thinks the screen is)
void SetLogicalSize(int w, int h);
int LogicalW();
int LogicalH();

// Window procedure registered by the game (WindowProc_5E4EE0)
void SetWndProc(WNDPROC proc);
WNDPROC GetWndProc();
HWND GameHwnd();

// Pump SDL events, turning them into window messages queued for PeekMessageA
void PumpEvents();
void AutoKeys(int frame);
void AutoTouch(int frame);
void MaybeScreenshot(int frame, const char* tag);
void ProbePlayer(int frame);
void DebugSpawnCar(int frame);
void UpdateDirectControl();
bool DirectControlEnabled();
void SetTouchStick(bool active, float x, float y, float mag);
void SetPadStick(bool active, float x, float y, float mag);
void PostMsg(UINT msg, WPARAM wParam, LPARAM lParam);
bool PopMsg(MSG* pMsg, bool remove);

// Keyboard state in DirectInput scan codes (DIK_*), 0x80 when down
const unsigned char* DikState();
// Translate SDL scancode -> DIK code
unsigned char SdlToDik(SDL_Scancode sc);

void Log(const char* fmt, ...);

// Input sources merged into the DirectInput keyboard state
enum
{
    kInputKeyboard = 0,
    kInputTouch = 1,
    kInputGamepad = 2,
    kInputAuto = 3,
    kInputSourceCount = 4
};
void SetInput(int source, unsigned char dik, bool down);
void HandleTouchEvent(const SDL_Event& e);
void HandleGamepadEvent(const SDL_Event& e);
// Draws the on-screen controls over the finished game frame (before present)
void DrawTouchOverlay();
bool GamepadConnected();
bool TouchButtonDown(unsigned char dik);

// Mixer access for movie audio
void* PlayPcm(const short* data, int frames, int channels, int rate);
void StopPcm(void* h);

std::string ResolveRead(const char* path);
} // namespace port
