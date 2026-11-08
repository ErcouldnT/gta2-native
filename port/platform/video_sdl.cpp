// Replacement for GTA2's dmavideo.dll (Vid_*) and d3ddll.dll (gbh_*) on top of the SDL3
// renderer (Metal on macOS/iOS). Behaviour follows GTA2Hax's D3D9 reimplementation of d3ddll:
// pre-transformed vertices in game screen space, 8-bit paletted textures, per-vertex diffuse
// colour (CPU lighting), additive and alpha blending selected by the draw flags.
#include "port_internal.hpp"
#include <objbase.h>
#include <dinput.h>
#include "3rdParty/GTA2Hax/d3ddll/d3ddll.hpp"
#include "3rdParty/GTA2Hax/DmaVideo/DmaVideo.hpp"
#include <map>
#include <vector>

typedef void(__stdcall* TBeginSceneCB)();

namespace
{
SDL_Window* gWindow = NULL;
SDL_Renderer* gRenderer = NULL;
int gLogicalW = 640;
int gLogicalH = 480;
SDL_BlendMode gAdditiveBlend = SDL_BLENDMODE_ADD;

// ---- video modes offered to the game ----
struct Mode
{
    int w, h;
};
const Mode kModes[] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 960}};
const int kModeCount = sizeof(kModes) / sizeof(kModes[0]);
std::vector<SDisplayMode> gDisplayModes;
int gEnumIdx = 0;

SVideo gVideo;
SDevice gDevice;

// ---- gbh state ----
struct PalData
{
    DWORD* original; // 256 entries, stride 64 DWORDs (palette pages are stored in columns)
    WORD* pData; // 2 x 256 RGB565 entries, what the engine sees in STexture::field_18
    bool loaded;
};
PalData gPals[16384];
std::map<const WORD*, const DWORD*> gPal565ToOriginal;

struct SLightInternal
{
    DWORD flags;
    float brightness;
    float radius;
    float radiusSq;
    float radiusInv;
    float x, y, z;
    float r, g, b;
};
SLightInternal gLights[256];
DWORD gNumLights = 0;
float gAmbient = 255.0f;

float gDrawOffsetX = 0;
float gWinLeft = 0, gWinTop = 0, gWinRight = 639, gWinBottom = 479;

struct ImageEntry
{
    bool loaded;
    DWORD w, h;
    STexture* tex;
};
std::vector<ImageEntry> gImages;
u32 gTextureId = 0;
TBeginSceneCB gBeginSceneCB = NULL;
u32 gGlobals[256];
int gScreenTable[1700];

const u32 FLAG_FIT_TO_TEXTURE = 0x10000;
const u32 FLAG_TEXTURE_FILTERING = 0x20000;
const u32 FLAG_LIGHTING_SHADOW = 0x8000;
const u32 FLAG_USE_ALPHA_DIFFUSE = 0x2000;

inline u32 Rgb565To888(u16 c)
{
    u32 r = ((c >> 11) & 0x1F) * 255 / 31;
    u32 g = ((c >> 5) & 0x3F) * 255 / 63;
    u32 b = (c & 0x1F) * 255 / 31;
    return (r << 16) | (g << 8) | b;
}

inline u32 Rgb555To888(u16 c)
{
    u32 r = (c >> 10) & 0x1F;
    u32 g = (c >> 5) & 0x1F;
    u32 b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b;
}

u16 Rgb888To565(u32 c)
{
    u32 r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    return static_cast<u16>(((r * 31 / 255) << 11) | ((g * 63 / 255) << 5) | (b * 31 / 255));
}

// Build (or rebuild) the SDL texture behind an STexture
SDL_Texture* UploadTexture(SDL_Texture* existing, u32 w, u32 h, const u8* pixels, const u16* pal, const s32* pitch)
{
    if (!gRenderer || !w || !h)
    {
        return existing;
    }
    SDL_Texture* tex = existing;
    if (tex)
    {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        if (static_cast<u32>(tw) != w || static_cast<u32>(th) != h)
        {
            SDL_DestroyTexture(tex);
            tex = NULL;
        }
    }
    if (!tex)
    {
        tex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, static_cast<int>(w), static_cast<int>(h));
        if (!tex)
        {
            port::Log("SDL_CreateTexture %ux%u failed: %s\n", w, h, SDL_GetError());
            return NULL;
        }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }

    std::vector<u32> out(static_cast<size_t>(w) * h, 0);
    if (pixels && pal)
    {
        // Prefer the original 24-bit palette to the engine's 16-bit copy
        auto it = gPal565ToOriginal.find(pal);
        const DWORD* orig = it != gPal565ToOriginal.end() ? it->second : NULL;
        u32 colours[256];
        for (int i = 0; i < 256; i++)
        {
            u32 rgb = orig ? (orig[i * 64] & 0xFFFFFF) : Rgb565To888(pal[i]);
            colours[i] = (i == 0 ? 0u : 0xFF000000u) | rgb;
        }
        // Tile and sprite pages are 256 pixels wide
        for (u32 y = 0; y < h; y++)
        {
            const u8* row = pixels + static_cast<size_t>(y) * 256;
            for (u32 x = 0; x < w; x++)
            {
                out[y * w + x] = colours[row[x]];
            }
        }
    }
    else if (pixels && pitch)
    {
        // 16-bit RGB555 TGA, bottom-up; *pitch is the TGA image id length
        const u16* src = reinterpret_cast<const u16*>(pixels + *pitch);
        for (u32 y = 0; y < h; y++)
        {
            for (u32 x = 0; x < w; x++)
            {
                out[(h - 1 - y) * w + x] = 0xFF000000u | Rgb555To888(src[y * w + x]);
            }
        }
    }
    SDL_UpdateTexture(tex, NULL, out.data(), static_cast<int>(w * 4));
    return tex;
}

inline SDL_Texture* TexOf(STexture* t)
{
    return t ? reinterpret_cast<SDL_Texture*>(t->field_1C_ptr) : NULL;
}

// Textures are converted on first use, not when registered: the game registers 1024 tile
// textures for styles that only have 992 tiles (an original bug, see docs/known_crashes.md),
// and the pixels behind the missing ones must never be read. field_2 is unused by the game;
// the port keeps a "needs upload" flag there.
const u16 kTexDirty = 0x8000;

SDL_Texture* TexForDraw(STexture* t)
{
    if (!t)
    {
        return NULL;
    }
    if (t->field_2 & kTexDirty)
    {
        t->field_2 &= ~kTexDirty;
        t->field_1C_ptr = reinterpret_cast<IDirect3DTexture9*>(
            UploadTexture(TexOf(t), t->field_E_width, t->field_10_height, t->field_14_original_pixel_data_ptr, t->field_18_pPaltData, NULL));
    }
    return TexOf(t);
}

void ComputeLighting(Vert* pVerts, int count, u8 colourMul)
{
    for (int i = 0; i < count; i++)
    {
        const Vert& world = pVerts[i + 4];
        float lr = 0, lg = 0, lb = 0;
        for (DWORD j = 0; j < gNumLights; j++)
        {
            const SLightInternal& l = gLights[j];
            if ((l.flags & 0x30000) != 0x10000)
            {
                continue;
            }
            float dx = world.x - l.x, dy = world.y - l.y, dz = world.z - l.z;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > l.radiusSq)
            {
                continue;
            }
            float att = (l.radius - sqrtf(d2)) * l.radiusInv;
            if (att > 0.0f)
            {
                float b = att * l.brightness;
                lr += l.r * b;
                lg += l.g * b;
                lb += l.b * b;
            }
        }
        const s32 c = static_cast<s32>(pVerts[i].diff);
        const float m = colourMul / 255.0f;
        s32 r = static_cast<s32>(fminf(m * ((c >> 16) & 0xFF) * lr + gAmbient, 255.0f));
        s32 g = static_cast<s32>(fminf(m * ((c >> 8) & 0xFF) * lg + gAmbient, 255.0f));
        s32 b = static_cast<s32>(fminf(m * (c & 0xFF) * lb + gAmbient, 255.0f));
        pVerts[i].diff = static_cast<DWORD>((c & 0xFF000000) | (r << 16) | (g << 8) | b);
    }
}

void RenderVerts(u32 flags, STexture* pTexture, Vert* pVerts, int count, int baseColour)
{
    if (count < 3 || count > 4 || pVerts[0].z <= 0.0f || !gRenderer)
    {
        return;
    }
    if (flags & FLAG_FIT_TO_TEXTURE)
    {
        flags |= FLAG_TEXTURE_FILTERING;
    }
    const float tw = pTexture ? static_cast<float>(pTexture->field_E_width) : 1.0f;
    const float th = pTexture ? static_cast<float>(pTexture->field_10_height) : 1.0f;

    if (flags & FLAG_FIT_TO_TEXTURE)
    {
        static const float off[4][4] = {{0, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 1, 1}, {0, 1, 0, 1}};
        for (int i = 0; i < count; i++)
        {
            pVerts[i].x = pVerts[0].x + off[i][0] * tw;
            pVerts[i].y = pVerts[0].y + off[i][1] * th;
            pVerts[i].z = pVerts[0].z;
            pVerts[i].u = off[i][2] * tw;
            pVerts[i].v = off[i][3] * th;
        }
    }
    for (int i = 0; i < count; i++)
    {
        pVerts[i].w = pVerts[i].z;
    }
    if (!(flags & FLAG_USE_ALPHA_DIFFUSE))
    {
        const u32 c = static_cast<u8>(baseColour);
        const u32 diff = c | (c << 8) | (c << 16) | 0xFF000000u;
        for (int i = 0; i < count; i++)
        {
            pVerts[i].diff = diff;
            pVerts[i].spec = 255;
        }
    }
    if ((flags & FLAG_LIGHTING_SHADOW) && gAmbient != 255.0f)
    {
        ComputeLighting(pVerts, count, static_cast<u8>(baseColour));
    }

    const bool additive = (flags & 0x200) != 0;
    const bool alphaBlend = !additive && (flags & 0x180) != 0;

    SDL_Vertex v[4];
    for (int i = 0; i < count; i++)
    {
        const u32 d = pVerts[i].diff;
        v[i].position.x = pVerts[i].x + gDrawOffsetX;
        v[i].position.y = pVerts[i].y;
        v[i].color.r = ((d >> 16) & 0xFF) / 255.0f;
        v[i].color.g = ((d >> 8) & 0xFF) / 255.0f;
        v[i].color.b = (d & 0xFF) / 255.0f;
        // Without alpha blending D3D ignores the diffuse alpha; the texture alpha (palette
        // index 0) still cuts out transparent pixels here, which matches what you see in-game.
        v[i].color.a = alphaBlend ? ((d >> 24) & 0xFF) / 255.0f : 1.0f;
        v[i].tex_coord.x = pVerts[i].u / tw;
        v[i].tex_coord.y = pVerts[i].v / th;
    }

    SDL_Texture* tex = TexForDraw(pTexture);
    if (tex)
    {
        SDL_SetTextureBlendMode(tex, additive ? gAdditiveBlend : SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, (flags & FLAG_TEXTURE_FILTERING) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    }
    else
    {
        SDL_SetRenderDrawBlendMode(gRenderer, additive ? gAdditiveBlend : SDL_BLENDMODE_BLEND);
    }
    static const int kQuadIdx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(gRenderer, tex, v, count, count == 4 ? kQuadIdx : NULL, count == 4 ? 6 : 0);
    gGlobals[0]++;
}

STexture* InternalRegisterTexture(s32 width, s32 height, BYTE* pData, const s32* palIdx, const s32* pitch)
{
    STexture* t = static_cast<STexture*>(calloc(1, sizeof(STexture)));
    if (!t)
    {
        return NULL;
    }
    t->field_0_id = static_cast<u16>(gTextureId++);
    t->field_E_width = static_cast<u16>(width);
    t->field_10_height = static_cast<u16>(height);
    t->field_14_original_pixel_data_ptr = pData;
    if (palIdx)
    {
        t->field_12_bPalIsValid = gPals[*palIdx].loaded;
        t->field_18_pPaltData = gPals[*palIdx].pData;
    }
    if (pitch)
    {
        // images (TGA) are complete: convert now, the source buffer may be freed
        t->field_1C_ptr = reinterpret_cast<IDirect3DTexture9*>(UploadTexture(NULL, width, height, pData, NULL, pitch));
    }
    else
    {
        t->field_2 |= kTexDirty;
    }
    return t;
}
} // namespace

extern "C" void port_set_draw_offset(float x)
{
    gDrawOffsetX = x;
}

// ===========================================================================
// Window / renderer
// ===========================================================================
namespace port
{
void WidescreenSize(int& w, int& h)
{
    w = h = 0;
    // Wider than 4:3: the camera keeps the 4:3 scale and sees more to the sides, the HUD is
    // centred (Camera::PortWideFactor, Game::Draw_4B92D0). Default on iOS, PORT_43=1 turns it off.
    bool want = getenv("PORT_WIDESCREEN") != NULL;
#if defined(SDL_PLATFORM_IOS)
    want = !getenv("PORT_43");
#endif
    if (!want)
    {
        return;
    }
    float aspect = 16.0f / 9.0f;
    if (const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay()))
    {
        float a = static_cast<float>(dm->w) / dm->h;
        aspect = a >= 1.0f ? a : 1.0f / a;
    }
    h = 480;
    w = (static_cast<int>(480 * aspect) + 7) & ~7;
}

SDL_Window* Window()
{
    return gWindow;
}

SDL_Renderer* Renderer()
{
    return gRenderer;
}

int LogicalW()
{
    return gLogicalW;
}

int LogicalH()
{
    return gLogicalH;
}

void SetLogicalSize(int w, int h)
{
    gLogicalW = w;
    gLogicalH = h;
    if (gRenderer)
    {
        SDL_SetRenderLogicalPresentation(gRenderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
}

// Debug: PORT_SHOTS="60,300,900" saves those frames as BMPs into PORT_SHOT_DIR (default /tmp,
// "~/..." is relative to $HOME, i.e. the app container on iOS). Movie frames use PORT_MOVIE_SHOTS.
void MaybeScreenshot(int frame, const char* tag)
{
    SDL_Renderer* r = gRenderer;
    const char* shots = getenv(strcmp(tag, "movie") == 0 ? "PORT_MOVIE_SHOTS" : "PORT_SHOTS");
    if (!r || !shots)
    {
        return;
    }
    char key[16];
    snprintf(key, sizeof(key), "%d", frame);
    std::string list = std::string(",") + shots + ",";
    if (list.find(std::string(",") + key + ",") == std::string::npos)
    {
        return;
    }
    if (SDL_Surface* surf = SDL_RenderReadPixels(r, NULL))
    {
        const char* dir = getenv("PORT_SHOT_DIR");
        std::string d = dir ? dir : "/tmp";
        if (d.compare(0, 2, "~/") == 0 && getenv("HOME"))
        {
            d = std::string(getenv("HOME")) + d.substr(1);
        }
        char path[512];
        snprintf(path, sizeof(path), "%s/%s_%05d.bmp", d.c_str(), tag, frame);
        SDL_SaveBMP(surf, path);
        SDL_DestroySurface(surf);
        Log("saved %s\n", path);
    }
}

bool CreateWindowAndRenderer(const char* title, int w, int h)
{
    if (gWindow)
    {
        return true;
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(SDL_PLATFORM_IOS)
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    int ww = w * 2, wh = h * 2;
#if defined(SDL_PLATFORM_IOS)
    if (const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay()))
    {
        ww = dm->w;
        wh = dm->h;
    }
#endif
    if (!SDL_CreateWindowAndRenderer(title, ww, wh, flags, &gWindow, &gRenderer))
    {
        Log("SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    int pw = 0, ph = 0, sw = 0, sh = 0;
    SDL_GetWindowSize(gWindow, &sw, &sh);
    SDL_GetCurrentRenderOutputSize(gRenderer, &pw, &ph);
    Log("Renderer: %s, window %dx%d, output %dx%d\n", SDL_GetRendererName(gRenderer), sw, sh, pw, ph);
    SDL_SetRenderVSync(gRenderer, 1);
    gAdditiveBlend = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                                                SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
    SetLogicalSize(gLogicalW, gLogicalH);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);
    SDL_RenderPresent(gRenderer);
    return true;
}
} // namespace port

namespace impl
{
// ===========================================================================
// DMA Video (Vid_*)
// ===========================================================================
static SVidVersion gVidVersion = {0x3FE7AE14, "GTA2 native port video (SDL3)"};

static SVidVersion* __stdcall Vid_GetVersion()
{
    return &gVidVersion;
}

static s32 __stdcall Vid_FindNextMode(SVideo* pVideo);

static s32 __stdcall Vid_FindFirstMode(SVideo* pVideo, s32)
{
    if (!pVideo)
    {
        return 0;
    }
    gEnumIdx = 0;
    return impl::Vid_FindNextMode(pVideo);
}

static s32 __stdcall Vid_FindNextMode(SVideo* pVideo)
{
    if (!pVideo || gEnumIdx >= kModeCount)
    {
        return 0;
    }
    return ++gEnumIdx;
}

static SVideo* __stdcall Vid_Init_SYS(HINSTANCE, u16 flags)
{
    memset(&gVideo, 0, sizeof(gVideo));
    gVideo.field_0 = 1;
    gVideo.field_4_flags = (flags & 0x40) | 0x200;
    gVideo.field_14_display_mode_count_2_q = 1;
    gVideo.field_18_num_guids = 2;
    gVideo.field_48_rect_right = 640;
    gVideo.field_4C_rect_bottom = 480;

    gDisplayModes.clear();
    for (int i = 0; i < kModeCount; i++)
    {
        SDisplayMode m;
        memset(&m, 0, sizeof(m));
        m.field_0_display_mode_idx = i + 1;
        m.field_8_width = kModes[i].w;
        m.field_C_height = kModes[i].h;
        m.field_10_pitch = kModes[i].w * 2;
        m.field_14_rgb_bit_count = 16;
        gDisplayModes.push_back(m);
    }
    // Widescreen: a mode with the display's aspect ratio at 480 lines (iOS default, PORT_WIDESCREEN=1 on macOS)
    {
        int ww = 0, wh = 0;
        port::WidescreenSize(ww, wh);
        if (ww)
        {
            SDisplayMode m;
            memset(&m, 0, sizeof(m));
            m.field_0_display_mode_idx = static_cast<DWORD>(gDisplayModes.size() + 1);
            m.field_8_width = ww;
            m.field_C_height = wh;
            m.field_10_pitch = ww * 2;
            m.field_14_rgb_bit_count = 16;
            gDisplayModes.push_back(m);
        }
    }
    port::ApplyWidescreenDefault();
    // RGB565 masks, Bink reads these
    gVideo.field_5C = 5;
    gVideo.field_60_g = 6;
    gVideo.field_64_r = 5;
    gVideo.field_68_b = 5;
    gVideo.field_6C = 5;
    return &gVideo;
}

static s32 __stdcall Vid_CheckMode(SVideo* pVideo, s32 w, s32 h, s32)
{
    if (!pVideo)
    {
        return 0;
    }
    for (const SDisplayMode& m : gDisplayModes)
    {
        if (static_cast<s32>(m.field_8_width) == w && static_cast<s32>(m.field_C_height) == h)
        {
            return static_cast<s32>(m.field_0_display_mode_idx);
        }
    }
    return 0;
}

static SDevice* __stdcall Vid_FindDevice(SVideo*, s32 id)
{
    memset(&gDevice, 0, sizeof(gDevice));
    gDevice.field_0_id = id;
    gDevice.field_4_flags = 1;
    return &gDevice;
}

static SDisplayMode* __stdcall Vid_FindMode(SVideo*, s32 modeId)
{
    if (modeId >= 1 && modeId <= static_cast<s32>(gDisplayModes.size()))
    {
        return &gDisplayModes[modeId - 1];
    }
    return NULL;
}

static s32 __stdcall Vid_SetDevice(SVideo*, s32)
{
    return 0;
}

static void __stdcall Vid_CloseScreen(SVideo*)
{
}

static s32 __stdcall Vid_SetMode(SVideo* pVideo, HWND, s32 modeId)
{
    int w = 640, h = 480;
    if (modeId >= 1 && modeId <= static_cast<s32>(gDisplayModes.size()))
    {
        w = static_cast<int>(gDisplayModes[modeId - 1].field_8_width);
        h = static_cast<int>(gDisplayModes[modeId - 1].field_C_height);
    }
    else if (modeId == -2)
    {
        // windowed: the game's window_width/height (the client rect)
        RECT r;
        GetWindowRect(NULL, &r);
        w = r.right - r.left > 0 ? r.right - r.left : 640;
        h = r.bottom - r.top > 0 ? r.bottom - r.top : 480;
    }
    port::Log("Vid_SetMode %d -> %dx%d\n", modeId, w, h);
    pVideo->field_40_full_screen = modeId;
    pVideo->field_48_rect_right = w;
    pVideo->field_4C_rect_bottom = h;
    pVideo->field_4C0_hwnd = port::GameHwnd();
    port::SetLogicalSize(w, h);
    return 0;
}

static void __stdcall Vid_FlipBuffers(SVideo*)
{
    static int frame = 0;
    frame++;
    port::AutoKeys(frame);
    port::AutoTouch(frame);
    port::ProbePlayer(frame);
    port::DebugSpawnCar(frame);
    port::UpdateDirectControl();
    port::DrawTouchOverlay();
    if (gRenderer)
    {
        port::MaybeScreenshot(frame, "shot");
        SDL_RenderPresent(gRenderer);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
        SDL_RenderClear(gRenderer);
    }
}

static void __stdcall Vid_ReleaseSurface(SVideo*)
{
}

static void __stdcall Vid_GrabSurface(SVideo*)
{
}

static void __stdcall Vid_ShutDown_SYS(SVideo*)
{
}

static s32 __stdcall Vid_EnableWrites(SVideo*)
{
    return 0;
}

static s32 __stdcall Vid_DisableWrites(SVideo*)
{
    return 0;
}

static s32 __stdcall Vid_GetSurface(SVideo*)
{
    return 0;
}

static s32 __stdcall Vid_FreeSurface(SVideo*)
{
    return 0;
}

static s32 __stdcall Vid_ClearScreen(SVideo*, u8 r, u8 g, u8 b, s32 left, s32 top, s32 right, s32 bottom)
{
    if (!gRenderer)
    {
        return 0;
    }
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    if (left == 0 && top == 0 && right == 0 && bottom == 0)
    {
        SDL_RenderClear(gRenderer);
    }
    else
    {
        SDL_FRect rc = {static_cast<float>(left), static_cast<float>(top), static_cast<float>(right - left), static_cast<float>(bottom - top)};
        SDL_RenderFillRect(gRenderer, &rc);
    }
    return 0;
}

static s32 __stdcall Vid_SetGamma(SVideo*, f32, f32, f32)
{
    return 0;
}

static s32 __stdcall Vid_WindowProc(SVideo*, HWND, u32, WPARAM, LPARAM)
{
    return 0;
}

static s32 __stdcall Vid_InitDLL(HINSTANCE, SVideoFunctions*)
{
    return 0;
}

// ===========================================================================
// d3ddll (gbh_*), signatures as the game's gbh_graphics.hpp typedefs
// ===========================================================================
static void __stdcall ConvertColourBank(BYTE*)
{
}

static s32 __stdcall DrawLine(s32, s32 a2, s32, s32 a4, s32)
{
    return abs(a4 - a2);
}

static void __stdcall SetShadeTableA(s32, s32, s32, s32, s32)
{
}

static s32* __stdcall MakeScreenTable(s32 value, s32 elementSize, u32 size)
{
    if (size > 1700)
    {
        size = 1700;
    }
    for (u32 i = 0; i < size; i++)
    {
        gScreenTable[i] = value;
        value += elementSize;
    }
    return gScreenTable;
}

static s32 __stdcall gbh_AddLight(SLight* p)
{
    DWORD idx = gNumLights;
    if (idx >= 256)
    {
        return 0;
    }
    SLightInternal& l = gLights[idx];
    l.flags = p->field_0;
    l.brightness = (p->field_0 & 0xFF) * (1.0f / 255.0f);
    l.x = p->field_4_x;
    l.y = p->field_8_y;
    l.z = p->field_C_z;
    l.r = ((p->field_10_colour >> 16) & 0xFF) * (1.0f / 255.0f);
    l.g = ((p->field_10_colour >> 8) & 0xFF) * (1.0f / 255.0f);
    l.b = (p->field_10_colour & 0xFF) * (1.0f / 255.0f);
    l.radius = ((p->field_0 >> 8) & 0xFF) * (1.0f / 255.0f) * 8.0f;
    l.radiusSq = l.radius * l.radius;
    l.radiusInv = l.radius > 0 ? 1.0f / l.radius : 0.0f;
    gNumLights++;
    return static_cast<s32>(idx * 0x2C);
}

static STexture* __stdcall gbh_LockTexture(STexture* t);
static STexture* __stdcall gbh_UnlockTexture(STexture* t);

static void __stdcall gbh_AssignPalette(STexture* t, s32 palId)
{
    bool relock = !(t->field_13_flags & 1);
    if (relock)
    {
        impl::gbh_LockTexture(t);
    }
    t->field_18_pPaltData = gPals[palId].pData;
    t->field_12_bPalIsValid = gPals[palId].loaded;
    if (relock)
    {
        impl::gbh_UnlockTexture(t);
    }
}

static void __stdcall gbh_BeginLevel()
{
}

static void __stdcall gbh_EndLevel()
{
}

static void __stdcall gbh_SetBeginSceneCB(TBeginSceneCB cb)
{
    gBeginSceneCB = cb;
}

static void __stdcall gbh_BeginScene()
{
    if (gBeginSceneCB)
    {
        gBeginSceneCB();
    }
}

static void __stdcall gbh_EndScene()
{
}

static s32 __stdcall gbh_BlitBuffer(s32, s32, s32, s32, s32, s32)
{
    return 0;
}

static s32 __stdcall gbh_BlitImage(s32 idx, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y)
{
    if (idx < 0 || idx >= static_cast<s32>(gImages.size()) || !gImages[idx].loaded)
    {
        return -1;
    }
    Vert v[8];
    memset(v, 0, sizeof(v));
    const float w = static_cast<float>(r - l), h = static_cast<float>(b - t);
    const float xs[4] = {0, w, w, 0}, ys[4] = {0, 0, h, h};
    const float us[4] = {(float)l, (float)r, (float)r, (float)l}, vs[4] = {(float)t, (float)t, (float)b, (float)b};
    for (int i = 0; i < 4; i++)
    {
        v[i].x = x + xs[i];
        v[i].y = y + ys[i];
        v[i].z = 1.0f;
        v[i].w = 1.0f;
        v[i].u = us[i];
        v[i].v = vs[i];
    }
    RenderVerts(0, gImages[idx].tex, v, 4, 0xFF);
    return 0;
}

static void __stdcall gbh_CloseDLL()
{
}

static void __stdcall gbh_CloseScreen(SVideo*)
{
}

static u32 __stdcall gbh_Convert16BitGraphic(s32, u32, WORD*, s32)
{
    return 0;
}

static u32 __stdcall gbh_ConvertColour(u8 r, u8 g, u8 b)
{
    return (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | b;
}

static s32 __stdcall gbh_DrawFlatRect(Vert* pVerts, s32 colour)
{
    if (!gRenderer)
    {
        return 0;
    }
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(gRenderer, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF, 255);
    SDL_FRect rc = {pVerts[0].x + gDrawOffsetX, pVerts[0].y, pVerts[2].x - pVerts[0].x, pVerts[2].y - pVerts[0].y};
    SDL_RenderFillRect(gRenderer, &rc);
    return 0;
}

static void __stdcall gbh_DrawQuad(s32 flags, STexture* t, Vert* v, s32 colour)
{
    RenderVerts(static_cast<u32>(flags), t, v, 4, colour);
}

static void __stdcall gbh_DrawQuadClipped(s32, s32, s32, s32, s32)
{
}

static s32 __stdcall gbh_DrawTilePart(u32 flags, STexture* t, Vert* p, s32 colour)
{
    u32 effFlags = flags;
    if (!(flags & 0x4000))
    {
        // Inset the UVs by half a texel (avoids bleeding at tile edges)
        const float lo = 0.5f, hi = 63.499901f;
        p[0].u = lo;
        p[0].v = lo;
        p[1].u = hi;
        p[1].v = lo;
        p[2].u = hi;
        p[2].v = hi;
        p[3].u = lo;
        p[3].v = hi;
    }
    float uv[4][2];
    for (int i = 0; i < 4; i++)
    {
        uv[i][0] = p[i].u;
        uv[i][1] = p[i].v;
    }
    auto setUV = [&](int dst, int src) {
        p[dst].u = uv[src][0];
        p[dst].v = uv[src][1];
    };
    auto snap = [&]() {
        for (int i = 0; i < 4; i++)
        {
            uv[i][0] = p[i].u;
            uv[i][1] = p[i].v;
        }
    };
    switch (flags & 0x60)
    {
        case 0x20: // rotate 90
            setUV(0, 3);
            setUV(1, 0);
            setUV(2, 1);
            setUV(3, 2);
            snap();
            break;
        case 0x40: // rotate 180 == flip both
            effFlags = flags ^ 0x18;
            break;
        case 0x60: // rotate 270
            setUV(0, 1);
            setUV(1, 2);
            setUV(2, 3);
            setUV(3, 0);
            snap();
            break;
    }
    if (effFlags & 8)
    {
        setUV(0, 1);
        setUV(1, 0);
        setUV(2, 3);
        setUV(3, 2);
        if (effFlags & 0x10)
        {
            snap();
        }
    }
    if (effFlags & 0x10)
    {
        setUV(0, 3);
        setUV(1, 2);
        setUV(2, 1);
        setUV(3, 0);
    }
    impl::gbh_DrawQuad(static_cast<s32>(effFlags), t, p, colour);
    return 0;
}

static s32 __stdcall gbh_DrawTile(u32 flags, STexture* t, Vert* p, u8 colour)
{
    return impl::gbh_DrawTilePart(flags, t, p, colour);
}

static void __stdcall gbh_DrawTriangle(s32 flags, STexture* t, Vert* v, u8 colour)
{
    RenderVerts(static_cast<u32>(flags), t, v, 3, colour);
}

static s32 __stdcall gbh_FreeImageTable()
{
    for (ImageEntry& e : gImages)
    {
        if (e.loaded && e.tex)
        {
            if (TexOf(e.tex))
            {
                SDL_DestroyTexture(TexOf(e.tex));
            }
            free(e.tex);
        }
    }
    gImages.clear();
    return 0;
}

static void __stdcall gbh_FreePalette(s32 palId)
{
    gPal565ToOriginal.erase(gPals[palId].pData);
    free(gPals[palId].pData);
    gPals[palId].pData = NULL;
    gPals[palId].loaded = false;
}

static void __stdcall gbh_FreeTexture(STexture* t)
{
    if (!t)
    {
        return;
    }
    if (TexOf(t))
    {
        SDL_DestroyTexture(TexOf(t));
    }
    free(t);
}

static u32* __stdcall gbh_GetGlobals()
{
    return gGlobals;
}

static s32 __stdcall gbh_GetUsedCache(s32)
{
    return 0;
}

static s32 __stdcall gbh_Init(s32)
{
    return 0;
}

static u32 __stdcall gbh_InitDLL(SVideo*)
{
    return 1;
}

static s32 __stdcall gbh_InitImageTable(s32 size)
{
    gImages.assign(static_cast<size_t>(size), ImageEntry{false, 0, 0, NULL});
    return 0;
}

#pragma pack(push, 1)
struct TgaHeader
{
    u8 idLength;
    u8 colourMapType;
    u8 imageType;
    u8 colourMap[5];
    u16 xOrigin;
    u16 yOrigin;
    u16 width;
    u16 height;
    u8 bpp;
    u8 descriptor;
};
#pragma pack(pop)

static s32 __stdcall gbh_LoadImage(SImage* pImage)
{
    const TgaHeader* h = reinterpret_cast<const TgaHeader*>(pImage);
    size_t slot = 0;
    for (; slot < gImages.size() && gImages[slot].loaded; slot++)
    {
    }
    if (slot == gImages.size())
    {
        return -1;
    }
    if (!h || h->colourMapType || h->imageType != 2 || h->bpp != 16)
    {
        port::Log("gbh_LoadImage: unsupported TGA (type %d, bpp %d)\n", h ? h->imageType : -1, h ? h->bpp : -1);
        return -2;
    }
    ImageEntry& e = gImages[slot];
    e.loaded = true;
    e.w = h->width;
    e.h = h->height;
    s32 idLen = h->idLength;
    e.tex = InternalRegisterTexture(h->width, h->height, reinterpret_cast<BYTE*>(pImage) + sizeof(TgaHeader), NULL, &idLen);
    return e.tex ? static_cast<s32>(slot) : -3;
}

static STexture* __stdcall gbh_LockTexture(STexture* t)
{
    t->field_8_locked_pixels_ptr = t->field_14_original_pixel_data_ptr;
    t->field_13_flags |= 1;
    t->field_6_pal_size = 256;
    return t;
}

static STexture* __stdcall gbh_UnlockTexture(STexture* t)
{
    t->field_6_pal_size = 0;
    t->field_8_locked_pixels_ptr = NULL;
    t->field_13_flags &= 0xFE;
    t->field_2 |= kTexDirty;
    return t;
}

static void __stdcall gbh_Plot(f32 x, f32 y, s32, s32 colour)
{
    if (!gRenderer)
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(gRenderer, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF, 255);
    SDL_RenderPoint(gRenderer, x + gDrawOffsetX, y);
}

static s32 __stdcall gbh_PrintBitmap(s32, s32)
{
    return 0;
}

static u32 __stdcall gbh_RegisterPalette(s32 palId, DWORD* pData)
{
    DWORD* p = pData;
    for (int i = 0; i < 256; i++, p += 64)
    {
        if (i == 0)
        {
            *p = 0; // colour 0 is transparent
        }
        else if (*p == 0)
        {
            *p = 0x10000;
        }
    }
    WORD* pal = static_cast<WORD*>(malloc(256 * 2 * sizeof(WORD)));
    p = pData;
    for (int i = 0; i < 256; i++, p += 64)
    {
        pal[i] = Rgb888To565(*p);
        pal[256 + i] = pal[i];
    }
    if (gPals[palId].pData)
    {
        gPal565ToOriginal.erase(gPals[palId].pData);
    }
    gPals[palId].original = pData;
    gPals[palId].pData = pal;
    gPals[palId].loaded = true;
    gPal565ToOriginal[pal] = pData;
    return static_cast<u32>(palId);
}

static STexture* __stdcall gbh_RegisterTexture(s32 w, s32 h, BYTE* pData, s32 palIdx, char_type)
{
    return InternalRegisterTexture(w, h, pData, &palIdx, NULL);
}

static void __stdcall gbh_ResetLights()
{
    gNumLights = 0;
}

static void __stdcall gbh_SetAmbient(f32 a)
{
    gAmbient = a * 255.0f;
}

static s32 __stdcall gbh_SetCamera(f32, f32, f32, f32)
{
    return 0;
}

static s32 __stdcall gbh_SetColourDepth()
{
    return 1;
}

static void __stdcall gbh_SetWindow(f32 l, f32 t, f32 r, f32 b)
{
    gWinLeft = l;
    gWinTop = t;
    gWinRight = r;
    gWinBottom = b;
    // The game window always covers the whole logical screen; no clip rect needed (and SDL's
    // clip rect interacts badly with logical presentation on iOS).
}

} // namespace impl

// ===========================================================================
// Export table used by GetProcAddress
// ===========================================================================
struct PortExport
{
    const char* name;
    void* fn;
};



#define EXP(f) {#f, reinterpret_cast<void*>(impl::f)}
static const PortExport kExports[] = {
    EXP(Vid_GetVersion),
    EXP(Vid_Init_SYS),
    EXP(Vid_CheckMode),
    EXP(Vid_FindMode),
    EXP(Vid_FindFirstMode),
    EXP(Vid_FindNextMode),
    EXP(Vid_FindDevice),
    EXP(Vid_SetDevice),
    EXP(Vid_CloseScreen),
    EXP(Vid_SetMode),
    EXP(Vid_FlipBuffers),
    EXP(Vid_ReleaseSurface),
    EXP(Vid_GrabSurface),
    EXP(Vid_ShutDown_SYS),
    EXP(Vid_EnableWrites),
    EXP(Vid_DisableWrites),
    EXP(Vid_GetSurface),
    EXP(Vid_FreeSurface),
    EXP(Vid_ClearScreen),
    EXP(Vid_WindowProc),
    EXP(Vid_InitDLL),
    EXP(Vid_SetGamma),
    EXP(ConvertColourBank),
    EXP(DrawLine),
    EXP(SetShadeTableA),
    EXP(MakeScreenTable),
    EXP(gbh_AddLight),
    EXP(gbh_AssignPalette),
    EXP(gbh_BeginLevel),
    EXP(gbh_SetBeginSceneCB),
    EXP(gbh_BeginScene),
    EXP(gbh_BlitBuffer),
    EXP(gbh_BlitImage),
    EXP(gbh_CloseDLL),
    EXP(gbh_CloseScreen),
    EXP(gbh_Convert16BitGraphic),
    EXP(gbh_ConvertColour),
    EXP(gbh_DrawFlatRect),
    EXP(gbh_DrawQuad),
    EXP(gbh_DrawQuadClipped),
    EXP(gbh_DrawTile),
    EXP(gbh_DrawTilePart),
    EXP(gbh_DrawTriangle),
    EXP(gbh_EndLevel),
    EXP(gbh_EndScene),
    EXP(gbh_FreeImageTable),
    EXP(gbh_FreePalette),
    EXP(gbh_FreeTexture),
    EXP(gbh_GetGlobals),
    EXP(gbh_GetUsedCache),
    EXP(gbh_Init),
    EXP(gbh_InitDLL),
    EXP(gbh_InitImageTable),
    EXP(gbh_LoadImage),
    EXP(gbh_LockTexture),
    EXP(gbh_Plot),
    EXP(gbh_PrintBitmap),
    EXP(gbh_RegisterPalette),
    EXP(gbh_RegisterTexture),
    EXP(gbh_ResetLights),
    EXP(gbh_SetAmbient),
    EXP(gbh_SetCamera),
    EXP(gbh_SetColourDepth),
    EXP(gbh_SetWindow),
    EXP(gbh_UnlockTexture),
    {"DirectInputCreateA", reinterpret_cast<void*>(DirectInputCreateA)},
    {NULL, NULL},
};

const PortExport* PortGetVideoExports()
{
    return kExports;
}
