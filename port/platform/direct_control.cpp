// Touch-friendly movement: the joystick gives an absolute direction on the screen and the
// player (on foot or in a car) turns towards it, instead of GTA2's tank controls
// (left/right = rotate, up = forward).
#include "Game.hpp"
#include "Player.hpp"
#include "Ped.hpp"
#include "char.hpp"
#include "Car.hpp"
#include "sprite.hpp"
#include "Frontend.hpp"
#include "Char_Pool.hpp"
#include "port_internal.hpp"

namespace port
{
// Debug probe: PORT_PROBE=1 logs the player's rotation and position
} // namespace port

// ---------------------------------------------------------------------------
// Direct (absolute) control. GTA2 angles: 1440 units per turn, 0 = facing down the screen,
// 720 = up; the "left" key increases the angle, "right" decreases it.
// ---------------------------------------------------------------------------
namespace
{
bool gStickActive = false;
float gStickX = 0, gStickY = 0; // unit vector in screen space (y down), magnitude 0..1
float gStickMag = 0;
bool gPadStick = false;
float gPadX = 0, gPadY = 0, gPadMag = 0;
bool gWasDriving = false;

int WrapDiff(int d)
{
    while (d >= 720)
    {
        d -= 1440;
    }
    while (d < -720)
    {
        d += 1440;
    }
    return d;
}

bool GetPlayer(Ped*& ped, Sprite*& sprite, bool& inCar)
{
    if (!gGame_0x40_67E008 || gFrontend_67DC84)
    {
        return false;
    }
    Player* pl = gGame_0x40_67E008->field_38_local_player;
    ped = pl ? pl->field_2C4_player_ped : NULL;
    if (!ped)
    {
        return false;
    }
    Car* car = ped->field_16C_car;
    inCar = car != NULL;
    if (car)
    {
        sprite = car->field_50_car_sprite;
    }
    else
    {
        sprite = ped->field_168_game_object ? ped->field_168_game_object->field_80_sprite_ptr : NULL;
    }
    return sprite != NULL;
}

void SetArrows(int src, bool up, bool down, bool left, bool right)
{
    port::SetInput(src, 0xC8, up); // DIK_UP
    port::SetInput(src, 0xD0, down); // DIK_DOWN
    port::SetInput(src, 0xCB, left); // DIK_LEFT
    port::SetInput(src, 0xCD, right); // DIK_RIGHT
}

// Aim assist: while FIRE is held on foot, finds the nearest living ped within reach, inside a
// cone around `aim` (the stick direction, or where the player already faces). Returns its
// direction in game angle units, or -1. PORT_NO_AIM_ASSIST disables it.
int FindAimTarget(Ped* self, Sprite* sprite, int aim)
{
    static const bool disabled = getenv("PORT_NO_AIM_ASSIST") != NULL;
    if (disabled || !gPedPool_6787B8)
    {
        return -1;
    }
    const double kRange = 6.0; // tiles
    const int kCone = 280; // +- 70 degrees
    const double px = sprite->field_14_xy.x.AsDouble(), py = sprite->field_14_xy.y.AsDouble(), pz = sprite->field_1C_zpos.AsDouble();
    double best = kRange * kRange;
    int result = -1;
    for (Ped* p = gPedPool_6787B8->GetFirstPed_435530(); p; p = p->mpNext)
    {
        if (p == self || p->field_278_ped_state_1 == ped_state_1::dead_9 || !p->field_168_game_object)
        {
            continue;
        }
        Sprite* s = p->field_168_game_object->field_80_sprite_ptr;
        if (!s || fabs(s->field_1C_zpos.AsDouble() - pz) > 0.9)
        {
            continue;
        }
        const double dx = s->field_14_xy.x.AsDouble() - px, dy = s->field_14_xy.y.AsDouble() - py;
        const double d2 = dx * dx + dy * dy;
        if (d2 >= best || d2 < 0.01)
        {
            continue;
        }
        const int angle = (static_cast<int>(atan2(dx, dy) * 57.29578 * 4.0) + 1440) % 1440;
        if (abs(WrapDiff(angle - aim)) <= kCone)
        {
            best = d2;
            result = angle;
        }
    }
    return result;
}

// Steers towards the aim target while FIRE is held; true if it set the turn keys
bool AimAssistTurn(int current, int target, bool& left, bool& right)
{
    if (target < 0)
    {
        return false;
    }
    const int diff = WrapDiff(target - current);
    const int adiff = diff < 0 ? -diff : diff;
    static Uint64 lastLog = 0;
    if (SDL_GetTicks() - lastLog > 500)
    {
        lastLog = SDL_GetTicks();
        port::Log("aim target=%d current=%d\n", target, current);
    }
    left = adiff > 16 && diff > 0;
    right = adiff > 16 && diff < 0;
    return true;
}

void Drive(int src, bool active, float x, float y, float mag)
{
    // CAM button held: the stick pans the camera, so it stays a plain 4-way stick
    if (src == port::kInputTouch && port::TouchButtonDown(0x38))
    {
        const bool on = active && mag >= 0.25f;
        SetArrows(src, on && y < -0.38f, on && y > 0.38f, on && x < -0.38f, on && x > 0.38f);
        return;
    }
    if (!active || mag < 0.25f)
    {
        // REV alone keeps backing up straight
        bool left = false, right = false;
        Ped* idlePed;
        Sprite* idleSprite;
        bool idleInCar;
        if (src == port::kInputTouch && port::TouchButtonDown(0x1D) && GetPlayer(idlePed, idleSprite, idleInCar) && !idleInCar)
        {
            const int current = idleSprite->field_0_rotation.rValue;
            AimAssistTurn(current, FindAimTarget(idlePed, idleSprite, current), left, right);
        }
        SetArrows(src, false, src == port::kInputTouch && port::TouchButtonDown(0xD0), left, right);
        return;
    }
    Ped* ped;
    Sprite* sprite;
    bool inCar;
    if (!GetPlayer(ped, sprite, inCar))
    {
        // menus: plain 4-way arrows
        static Uint64 lastMenuLog = 0;
        if (SDL_GetTicks() - lastMenuLog > 2000)
        {
            lastMenuLog = SDL_GetTicks();
            port::Log("direct src=%d menu mode (game=%p frontend=%p)\n", src, static_cast<void*>(gGame_0x40_67E008),
                      static_cast<void*>(gFrontend_67DC84));
        }
        SetArrows(src, y < -0.38f, y > 0.38f, x < -0.38f, x > 0.38f);
        return;
    }
    const float deg = atan2f(x, y) * 57.29578f; // 0 = down, 360 units = right, 720 = up
    const int target = (static_cast<int>(deg * 4.0f) + 1440) % 1440;
    const int current = sprite->field_0_rotation.rValue;
    const int diff = WrapDiff(target - current);
    const int adiff = diff < 0 ? -diff : diff;

    bool up = false, down = false, left = false, right = false;
    // The REV touch button holds DIK_DOWN through the touch source; reverse while it's down
    const bool reverse = inCar && port::TouchButtonDown(0xD0);
    if (reverse)
    {
        // backing up: point the stick where the car's rear should go (steering is inverted)
        down = true;
        const int back = WrapDiff(target + 720 - current);
        const int aback = back < 0 ? -back : back;
        if (aback > 40)
        {
            right = back > 0;
            left = back < 0;
        }
    }
    else if (inCar)
    {
        // Always drive forwards and steer towards the stick: pulling the stick the other way
        // makes a U-turn (the handbrake/JUMP button tightens it). A small dead zone stops the
        // binary steering from zig-zagging around the target direction.
        up = true;
        if (adiff > 40)
        {
            left = diff > 0;
            right = diff < 0;
        }
    }
    else
    {
        if (adiff > 24)
        {
            left = diff > 0;
            right = diff < 0;
        }
        up = adiff < 400; // start walking while still turning
        if (src == port::kInputTouch && port::TouchButtonDown(0x1D))
        {
            AimAssistTurn(current, FindAimTarget(ped, sprite, target), left, right); // keep walking, aim at the target
        }
    }
    gWasDriving = inCar;
    static Uint64 lastLog = 0;
    if (SDL_GetTicks() - lastLog > 1000)
    {
        lastLog = SDL_GetTicks();
        port::Log("direct src=%d car=%d stick=(%.2f,%.2f) target=%d current=%d keys=%d%d%d%d\n", src, inCar ? 1 : 0, x, y, target, current, up,
                  down, left, right);
    }
    SetArrows(src, up, down, left, right);
}
} // namespace

namespace port
{
// Debug probe: PORT_PROBE=1 logs the player's (or car's) rotation and position
void ProbePlayer(int frame)
{
    Ped* ped;
    Sprite* sprite;
    bool inCar;
    if (!getenv("PORT_PROBE") || frame % 20)
    {
        return;
    }
    if (!GetPlayer(ped, sprite, inCar))
    {
        Log("probe f=%d no player (game=%p frontend=%p)\n", frame, static_cast<void*>(gGame_0x40_67E008), static_cast<void*>(gFrontend_67DC84));
        return;
    }
    Log("probe f=%d rot=%d x=%.2f y=%.2f car=%d\n", frame, sprite->field_0_rotation.rValue, sprite->field_14_xy.x.AsDouble(),
        sprite->field_14_xy.y.AsDouble(), inCar ? 1 : 0);
}

bool DirectControlEnabled()
{
    return !getenv("PORT_TANK_CONTROLS");
}

void SetTouchStick(bool active, float x, float y, float mag)
{
    gStickActive = active;
    gStickX = x;
    gStickY = y;
    gStickMag = mag;
}

void SetPadStick(bool active, float x, float y, float mag)
{
    gPadStick = active;
    gPadX = x;
    gPadY = y;
    gPadMag = mag;
}

// Called once per frame
void UpdateDirectControl()
{
    if (!DirectControlEnabled())
    {
        return;
    }
    Drive(kInputTouch, gStickActive, gStickX, gStickY, gStickMag);
    if (gPadStick || gPadMag > 0)
    {
        Drive(kInputGamepad, gPadStick, gPadX, gPadY, gPadMag);
    }
}
} // namespace port

// ---------------------------------------------------------------------------
// Debug: PORT_SPAWN_CAR="frame[:model]" spawns a car next to the player at that frame (for
// testing the driving controls; get in with the CAR button / Enter).
// ---------------------------------------------------------------------------
namespace port
{
void DebugSpawnCar(int frame)
{
    const char* env = getenv("PORT_SPAWN_CAR");
    if (!env || atoi(env) != frame)
    {
        return;
    }
    const char* colon = strchr(env, ':');
    int model = colon ? atoi(colon + 1) : 10;
    Ped* ped;
    Sprite* sprite;
    bool inCar;
    if (!GetPlayer(ped, sprite, inCar) || inCar)
    {
        return;
    }
    Fix16 one;
    one.FromInt(1);
    Fix16 scale;
    scale.FromInt(1);
    Car* car = gCarManager_6C_677930->SpawnCarAt_446230(sprite->field_14_xy.x + one, sprite->field_14_xy.y, sprite->field_1C_zpos, Ang16(0), model,
                                                        scale);
    if (car)
    {
        car->IncrementCarStats_443D70(car_kind::recycled_1);
        Log("spawned car model %d next to the player\n", model);
    }
}
} // namespace port
