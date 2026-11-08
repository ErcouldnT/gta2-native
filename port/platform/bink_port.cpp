// Bink video API used by GTA2's intro, on top of the AVFoundation decoder.
//
// The original intro.bik has to be converted once to intro.mp4 next to it (H.264 + AAC):
//   ffmpeg -i intro.bik -c:v h264_videotoolbox -b:v 2500k -c:a aac intro.mp4
// (port/install-ios.sh and port/run-mac.sh do this). Without the .mp4, BinkOpen fails and the
// game skips the movie.
#include "port_internal.hpp"
#include "3rdParty/bink.h"
#include "avf_video.h"
#include <vector>

namespace
{
struct PortBink
{
    BINK bink; // must stay first: the game reads width/height/frames from it
    AvfMovie* movie;
    double fps;
    std::vector<unsigned char> frame; // BGRA
    void* audio;
    Uint64 startNs;
    bool started;
    bool ended;
};

struct PortBinkBuffer
{
    BINKBUFFER buf;
    std::vector<unsigned char> pixels;
    SDL_Texture* tex;
    int width;
};

std::string MoviePath(const char* name)
{
    std::string p = port::ResolveRead(name);
    size_t dot = p.find_last_of('.');
    if (dot != std::string::npos)
    {
        p = p.substr(0, dot);
    }
    std::string mp4 = p + ".mp4";
    // the case of the extension may differ too
    FILE* f = ::fopen(mp4.c_str(), "rb");
    if (!f)
    {
        mp4 = port::ResolveRead((std::string(name).substr(0, std::string(name).find_last_of('.')) + ".mp4").c_str());
        f = ::fopen(mp4.c_str(), "rb");
    }
    if (f)
    {
        fclose(f);
        return mp4;
    }
    return "";
}
} // namespace

BINK* __stdcall BinkOpen(const char* name, s32)
{
    std::string path = MoviePath(name);
    if (path.empty() || getenv("PORT_NO_MOVIES"))
    {
        port::Log("BinkOpen(%s): no converted .mp4, skipping the movie\n", name);
        return NULL;
    }
    int w = 0, h = 0;
    double fps = 15, duration = 0;
    AvfMovie* m = avf_open(path.c_str(), &w, &h, &fps, &duration);
    if (!m)
    {
        port::Log("BinkOpen(%s): can't decode %s\n", name, path.c_str());
        return NULL;
    }
    PortBink* b = new PortBink();
    memset(&b->bink, 0, sizeof(b->bink));
    b->bink.width = w;
    b->bink.height = h;
    b->bink.total_frames = static_cast<s32>(duration * fps + 0.5);
    b->bink.current_frame = 1;
    b->movie = m;
    b->fps = fps;
    b->frame.assign(static_cast<size_t>(w) * h * 4, 0);
    b->audio = NULL;
    b->started = false;
    b->ended = false;

    int frames = 0, rate = 0, ch = 0;
    if (short* pcm = avf_read_audio(path.c_str(), &frames, &rate, &ch))
    {
        b->audio = port::PlayPcm(pcm, frames, ch, rate);
        free(pcm);
    }
    b->startNs = SDL_GetTicksNS();
    b->started = true;
    port::Log("BinkOpen(%s): %dx%d, %d frames at %.2f fps\n", name, w, h, b->bink.total_frames, fps);
    return &b->bink;
}

void __stdcall BinkClose(BINK* bink)
{
    if (!bink)
    {
        return;
    }
    PortBink* b = reinterpret_cast<PortBink*>(bink);
    if (b->audio)
    {
        port::StopPcm(b->audio);
    }
    avf_close(b->movie);
    delete b;
}

void __stdcall BinkGetSummary(BINK*, void* summary)
{
    if (summary)
    {
        memset(summary, 0, 0x78);
    }
}

// Non-zero while it isn't time for the next frame yet
s32 __stdcall BinkWait(BINK* bink)
{
    if (!bink)
    {
        return 0;
    }
    PortBink* b = reinterpret_cast<PortBink*>(bink);
    const double elapsed = (SDL_GetTicksNS() - b->startNs) / 1e9;
    const double due = (b->bink.current_frame - 1) / b->fps;
    if (elapsed < due)
    {
        SDL_Delay(1);
        return 1;
    }
    return 0;
}

s32 __stdcall BinkDoFrame(BINK* bink)
{
    if (!bink)
    {
        return 0;
    }
    PortBink* b = reinterpret_cast<PortBink*>(bink);
    if (!b->ended && !avf_next_frame(b->movie, b->frame.data(), b->bink.width * 4))
    {
        // the stream may hold a frame or two less than duration * fps
        b->ended = true;
        b->bink.current_frame = b->bink.total_frames;
    }
    return 0;
}

void __stdcall BinkNextFrame(BINK* bink)
{
    if (bink && bink->current_frame < bink->total_frames)
    {
        bink->current_frame++;
    }
}

s32 __stdcall BinkCopyToBuffer(BINK* bink, void* dest, s32 destpitch, s32 destheight, s32, s32, s32)
{
    if (!bink || !dest)
    {
        return 0;
    }
    PortBink* b = reinterpret_cast<PortBink*>(bink);
    const int rows = destheight < bink->height ? destheight : bink->height;
    for (int y = 0; y < rows; y++)
    {
        memcpy(static_cast<unsigned char*>(dest) + static_cast<size_t>(y) * destpitch, b->frame.data() + static_cast<size_t>(y) * bink->width * 4,
               static_cast<size_t>(bink->width) * 4);
    }
    return 0;
}

void __stdcall BinkSetSoundSystem(void*, s32)
{
}

void __stdcall BinkSetIOSize(s32)
{
}

void* __stdcall BinkOpenMiles(s32)
{
    return NULL;
}

BINKBUFFER* __stdcall BinkBufferOpen(HWND, s32 width, s32 height, s32)
{
    PortBinkBuffer* bb = new PortBinkBuffer();
    memset(&bb->buf, 0, sizeof(bb->buf));
    bb->width = width;
    bb->pixels.assign(static_cast<size_t>(width) * height * 4, 0);
    bb->buf.height = height;
    bb->buf.pitch = width * 4;
    bb->buf.pixels = bb->pixels.data();
    bb->tex = port::Renderer() ? SDL_CreateTexture(port::Renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height)
                               : NULL;
    if (bb->tex)
    {
        SDL_SetTextureScaleMode(bb->tex, SDL_SCALEMODE_LINEAR);
    }
    return &bb->buf;
}

void __stdcall BinkBufferClose(BINKBUFFER* buf)
{
    if (!buf)
    {
        return;
    }
    PortBinkBuffer* bb = reinterpret_cast<PortBinkBuffer*>(buf);
    if (bb->tex)
    {
        SDL_DestroyTexture(bb->tex);
    }
    delete bb;
}

void* __stdcall BinkBufferLock(BINKBUFFER* buf)
{
    return buf ? buf->pixels : NULL;
}

void __stdcall BinkBufferUnlock(BINKBUFFER*)
{
}

// Shows the frame: the game doesn't flip while a movie plays, so present here
void __stdcall BinkBufferBlit(BINKBUFFER* buf, void*, s32)
{
    SDL_Renderer* r = port::Renderer();
    if (!buf || !r)
    {
        return;
    }
    PortBinkBuffer* bb = reinterpret_cast<PortBinkBuffer*>(buf);
    if (bb->tex)
    {
        SDL_UpdateTexture(bb->tex, NULL, bb->pixels.data(), buf->pitch);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        SDL_RenderTexture(r, bb->tex, NULL, NULL);
    }
    port::DrawTouchOverlay();
    static int movieFrame = 0;
    port::MaybeScreenshot(++movieFrame, "movie");
    SDL_RenderPresent(r);
}

void __stdcall BinkBufferSetDDPrimary(void*)
{
}

void __stdcall BinkBufferCheckWinPos(BINKBUFFER*, s32*, s32*)
{
}

void __stdcall BinkBufferSetOffset(BINKBUFFER*, s32, s32)
{
}
