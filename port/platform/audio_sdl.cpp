// Miles Sound System (the subset GTA2 uses) on top of an SDL3 audio stream.
//
// - "samples" (HSAMPLE): PCM blocks from the .raw sound banks, 8 or 16-bit mono, with volume
//   (0..127), pan (0..127, 64 = centre), playback rate, loop count and loop block.
// - "streams" (HSTREAM): WAV files (IMA ADPCM in the shipped data: music and vocals), decoded
//   up front with SDL_LoadWAV.
// - No 3D providers are reported, so the game uses its 2D panning path.
// Everything is mixed in the audio callback with linear resampling.
#include "port_internal.hpp"
#include "types.hpp"
#include "3rdParty/mss.h"
#include <vector>

namespace port
{
std::string ResolveRead(const char* path);
}

namespace
{
const int kOutRate = 44100;

struct Voice
{
    // source
    const void* data = NULL;
    u32 frames = 0;
    int channels = 1;
    bool is16 = true;
    bool isUnsigned8 = true;
    std::vector<s16> owned; // decoded stream data
    // state
    int rate = 22050;
    int volume = 127;
    int pan = 64;
    int loopCount = 1; // 0 = forever
    s32 loopStart = 0; // bytes
    s32 loopEnd = -1; // bytes, -1 = end
    double pos = 0;
    bool playing = false;
    bool done = true;
    bool isStream = false;
};

SDL_AudioStream* gStream = NULL;
SDL_Mutex* gLock = NULL;
std::vector<Voice*>& Voices()
{
    static std::vector<Voice*> v;
    return v;
}
S32 gMasterVolume = 127;

inline int FrameBytes(const Voice* v)
{
    return v->channels * (v->is16 ? 2 : 1);
}

inline void SampleAt(const Voice* v, u32 frame, float& l, float& r)
{
    if (v->is16)
    {
        const s16* p = static_cast<const s16*>(v->data) + static_cast<size_t>(frame) * v->channels;
        l = p[0] * (1.0f / 32768.0f);
        r = v->channels > 1 ? p[1] * (1.0f / 32768.0f) : l;
    }
    else
    {
        const u8* p = static_cast<const u8*>(v->data) + static_cast<size_t>(frame) * v->channels;
        l = (p[0] - 128) * (1.0f / 128.0f);
        r = v->channels > 1 ? (p[1] - 128) * (1.0f / 128.0f) : l;
    }
}

void MixVoice(Voice* v, float* out, int frames)
{
    if (!v->playing || !v->data || !v->frames)
    {
        return;
    }
    const double step = static_cast<double>(v->rate) / kOutRate;
    const float vol = 0.8f * (v->volume / 127.0f) * (gMasterVolume / 127.0f); // 0.8: headroom for the mix
    // equal-power-ish pan
    const float p = v->pan / 127.0f;
    const float gl = vol * (v->channels > 1 ? 1.0f : fminf(1.0f, 2.0f * (1.0f - p)));
    const float gr = vol * (v->channels > 1 ? 1.0f : fminf(1.0f, 2.0f * p));

    const int fb = FrameBytes(v);
    u32 loopStart = static_cast<u32>(v->loopStart > 0 ? v->loopStart / fb : 0);
    u32 loopEnd = v->loopEnd < 0 ? v->frames : static_cast<u32>(v->loopEnd / fb);
    if (loopEnd > v->frames || loopEnd <= loopStart)
    {
        loopEnd = v->frames;
    }

    for (int i = 0; i < frames; i++)
    {
        u32 idx = static_cast<u32>(v->pos);
        const u32 end = (v->loopCount != 1) ? loopEnd : v->frames;
        if (idx >= end)
        {
            if (v->loopCount == 1)
            {
                v->playing = false;
                v->done = true;
                return;
            }
            if (v->loopCount > 1)
            {
                v->loopCount--;
            }
            v->pos = loopStart + (v->pos - end);
            idx = static_cast<u32>(v->pos);
            if (idx >= v->frames)
            {
                v->playing = false;
                v->done = true;
                return;
            }
        }
        float l0, r0, l1, r1;
        SampleAt(v, idx, l0, r0);
        if (idx + 1 < v->frames)
        {
            SampleAt(v, idx + 1, l1, r1);
        }
        else
        {
            l1 = l0;
            r1 = r0;
        }
        const float t = static_cast<float>(v->pos - idx);
        out[i * 2] += (l0 + (l1 - l0) * t) * gl;
        out[i * 2 + 1] += (r0 + (r1 - r0) * t) * gr;
        v->pos += step;
    }
}

void SDLCALL AudioCallback(void*, SDL_AudioStream* stream, int additional, int)
{
    const int frames = additional / static_cast<int>(sizeof(float) * 2);
    if (frames <= 0)
    {
        return;
    }
    static std::vector<float> buf;
    buf.assign(static_cast<size_t>(frames) * 2, 0.0f);
    SDL_LockMutex(gLock);
    for (Voice* v : Voices())
    {
        MixVoice(v, buf.data(), frames);
    }
    SDL_UnlockMutex(gLock);
    for (float& s : buf)
    {
        s = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
    }
    // Debug: PORT_AUDIO_DUMP=<file> records the mix as raw float stereo 44.1 kHz
    static FILE* dump = getenv("PORT_AUDIO_DUMP") ? ::fopen(getenv("PORT_AUDIO_DUMP"), "wb") : NULL;
    if (dump)
    {
        fwrite(buf.data(), sizeof(float), buf.size(), dump);
        fflush(dump);
    }
    SDL_PutAudioStreamData(stream, buf.data(), frames * static_cast<int>(sizeof(float) * 2));
}

struct Lock
{
    Lock()
    {
        SDL_LockMutex(gLock);
    }
    ~Lock()
    {
        SDL_UnlockMutex(gLock);
    }
};

Voice* NewVoice()
{
    Voice* v = new Voice();
    Lock l;
    Voices().push_back(v);
    return v;
}

void FreeVoice(Voice* v)
{
    if (!v)
    {
        return;
    }
    Lock l;
    auto& vs = Voices();
    for (size_t i = 0; i < vs.size(); i++)
    {
        if (vs[i] == v)
        {
            vs.erase(vs.begin() + i);
            break;
        }
    }
    delete v;
}

inline Voice* V(void* h)
{
    return static_cast<Voice*>(h);
}

struct Driver
{
    int dummy;
};
Driver gDriver;
} // namespace

namespace port
{
// Plays interleaved s16 PCM (copied) through the mixer; returns a handle for StopPcm
void* PlayPcm(const short* data, int frames, int channels, int rate)
{
    if (!gLock || !data || frames <= 0)
    {
        return NULL;
    }
    Voice* v = new Voice();
    v->owned.assign(data, data + static_cast<size_t>(frames) * channels);
    v->data = v->owned.data();
    v->channels = channels;
    v->is16 = true;
    v->frames = static_cast<u32>(frames);
    v->rate = rate;
    v->isStream = true;
    v->loopCount = 1;
    v->playing = true;
    v->done = false;
    Lock l;
    Voices().push_back(v);
    return v;
}

void StopPcm(void* h)
{
    FreeVoice(static_cast<Voice*>(h));
}
} // namespace port

extern "C" {

S32 AILCALL AIL_startup(void)
{
    if (!gLock)
    {
        gLock = SDL_CreateMutex();
    }
    return 1;
}

void AILCALL AIL_shutdown(void)
{
    if (gStream)
    {
        SDL_DestroyAudioStream(gStream);
        gStream = NULL;
    }
}

S32 AILCALL AIL_set_preference(U32, S32)
{
    return 0;
}

S32 AILCALL AIL_waveOutOpen(HDIGDRIVER* drvr, LPHWAVEOUT*, S32, LPWAVEFORMAT)
{
    if (!gLock)
    {
        gLock = SDL_CreateMutex();
    }
    if (!gStream)
    {
        if (getenv("PORT_NO_AUDIO"))
        {
            *drvr = NULL;
            return -1;
        }
        SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, kOutRate};
        gStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, NULL);
        if (!gStream)
        {
            port::Log("Audio device failed: %s\n", SDL_GetError());
            *drvr = NULL;
            return -1;
        }
        SDL_ResumeAudioStreamDevice(gStream);
    }
    *drvr = reinterpret_cast<HDIGDRIVER>(&gDriver);
    return 0;
}

void AILCALL AIL_waveOutClose(HDIGDRIVER)
{
}

S32 AILCALL AIL_digital_handle_release(HDIGDRIVER)
{
    if (gStream)
    {
        SDL_PauseAudioStreamDevice(gStream);
    }
    return 1;
}

S32 AILCALL AIL_digital_handle_reacquire(HDIGDRIVER)
{
    if (gStream)
    {
        SDL_ResumeAudioStreamDevice(gStream);
    }
    return 1;
}

void AILCALL AIL_set_digital_master_volume(HDIGDRIVER, S32 v)
{
    gMasterVolume = v;
}

// ---- samples ----
HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER)
{
    return reinterpret_cast<HSAMPLE>(NewVoice());
}

void AILCALL AIL_release_sample_handle(HSAMPLE s)
{
    FreeVoice(V(s));
}

void AILCALL AIL_init_sample(HSAMPLE s)
{
    if (!s)
    {
        return;
    }
    Lock l;
    Voice* v = V(s);
    v->data = NULL;
    v->frames = 0;
    v->volume = 127;
    v->pan = 64;
    v->loopCount = 1;
    v->loopStart = 0;
    v->loopEnd = -1;
    v->pos = 0;
    v->playing = false;
    v->done = true;
}

void AILCALL AIL_set_sample_type(HSAMPLE s, S32 format, U32)
{
    if (!s)
    {
        return;
    }
    Lock l;
    V(s)->is16 = (format & DIG_F_16BITS_MASK) != 0;
    V(s)->channels = (format & DIG_F_STEREO_MASK) ? 2 : 1;
}

void AILCALL AIL_set_sample_address(HSAMPLE s, void* start, U32 len)
{
    if (!s)
    {
        return;
    }
    Lock l;
    Voice* v = V(s);
    v->data = start;
    v->frames = len / static_cast<u32>(FrameBytes(v));
    v->pos = 0;
    v->loopStart = 0;
    v->loopEnd = -1;
}

void AILCALL AIL_set_sample_playback_rate(HSAMPLE s, S32 rate)
{
    if (s && rate > 0)
    {
        V(s)->rate = rate;
    }
}

void AILCALL AIL_set_sample_volume(HSAMPLE s, S32 vol)
{
    if (s)
    {
        V(s)->volume = vol < 0 ? 0 : (vol > 127 ? 127 : vol);
    }
}

void AILCALL AIL_set_sample_pan(HSAMPLE s, S32 pan)
{
    if (s)
    {
        V(s)->pan = pan < 0 ? 0 : (pan > 127 ? 127 : pan);
    }
}

void AILCALL AIL_set_sample_loop_count(HSAMPLE s, S32 count)
{
    if (s)
    {
        V(s)->loopCount = count;
    }
}

void AILCALL AIL_set_sample_loop_block(HSAMPLE s, S32 start, S32 end)
{
    if (s)
    {
        Lock l;
        V(s)->loopStart = start;
        V(s)->loopEnd = end;
    }
}

void AILCALL AIL_start_sample(HSAMPLE s)
{
    if (!s)
    {
        return;
    }
    Lock l;
    V(s)->pos = 0;
    V(s)->playing = V(s)->data != NULL;
    V(s)->done = !V(s)->playing;
}

void AILCALL AIL_end_sample(HSAMPLE s)
{
    if (!s)
    {
        return;
    }
    Lock l;
    V(s)->playing = false;
    V(s)->done = true;
}

U32 AILCALL AIL_sample_status(HSAMPLE s)
{
    if (!s)
    {
        return SMP_DONE;
    }
    Lock l;
    return V(s)->playing ? SMP_PLAYING : SMP_DONE;
}

// ---- streams ----
HSTREAM AILCALL AIL_open_stream(HDIGDRIVER, char* filename, S32)
{
    std::string path = port::ResolveRead(filename);
    SDL_AudioSpec spec;
    Uint8* buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len))
    {
        return NULL;
    }
    // Normalise to signed 16-bit, keeping the source rate and channel count
    SDL_AudioSpec dst = {SDL_AUDIO_S16, spec.channels, spec.freq};
    Uint8* conv = NULL;
    int convLen = 0;
    bool ok = SDL_ConvertAudioSamples(&spec, buf, static_cast<int>(len), &dst, &conv, &convLen);
    SDL_free(buf);
    if (!ok)
    {
        return NULL;
    }
    Voice* v = new Voice();
    v->owned.assign(reinterpret_cast<s16*>(conv), reinterpret_cast<s16*>(conv) + convLen / 2);
    SDL_free(conv);
    v->data = v->owned.data();
    v->channels = dst.channels;
    v->is16 = true;
    v->frames = static_cast<u32>(v->owned.size() / dst.channels);
    v->rate = dst.freq;
    v->isStream = true;
    v->loopCount = 1;
    {
        Lock l;
        Voices().push_back(v);
    }
    return reinterpret_cast<HSTREAM>(v);
}

void AILCALL AIL_close_stream(HSTREAM s)
{
    FreeVoice(V(s));
}

void AILCALL AIL_start_stream(HSTREAM s)
{
    if (!s)
    {
        return;
    }
    Lock l;
    V(s)->pos = 0;
    V(s)->playing = true;
    V(s)->done = false;
}

S32 AILCALL AIL_stream_status(HSTREAM s)
{
    if (!s)
    {
        return SMP_DONE;
    }
    Lock l;
    return V(s)->playing ? SMP_PLAYING : SMP_DONE;
}

void AILCALL AIL_set_stream_volume(HSTREAM s, S32 vol)
{
    if (s)
    {
        V(s)->volume = vol < 0 ? 0 : (vol > 127 ? 127 : vol);
    }
}

S32 AILCALL AIL_stream_volume(HSTREAM s)
{
    return s ? V(s)->volume : 0;
}

void AILCALL AIL_set_stream_loop_count(HSTREAM s, S32 count)
{
    if (s)
    {
        V(s)->loopCount = count;
    }
}

void AILCALL AIL_set_stream_playback_rate(HSTREAM s, S32 rate)
{
    if (s && rate > 0)
    {
        V(s)->rate = rate;
    }
}

S32 AILCALL AIL_stream_playback_rate(HSTREAM s)
{
    return s ? V(s)->rate : 22050;
}

void AILCALL AIL_stream_ms_position(HSTREAM s, S32* total, S32* current)
{
    Voice* v = V(s);
    if (!v)
    {
        return;
    }
    Lock l;
    if (total)
    {
        *total = static_cast<S32>(static_cast<double>(v->frames) * 1000.0 / v->rate);
    }
    if (current)
    {
        *current = static_cast<S32>(v->pos * 1000.0 / v->rate);
    }
}

void AILCALL AIL_set_stream_ms_position(HSTREAM s, S32 ms)
{
    if (!s)
    {
        return;
    }
    Lock l;
    V(s)->pos = static_cast<double>(ms) * V(s)->rate / 1000.0;
}

void AILCALL AIL_delay(S32 intervals)
{
    SDL_Delay(static_cast<Uint32>(intervals) * 1000 / 60);
}

void* AILCALL AIL_mem_alloc_lock(U32 size)
{
    return malloc(size);
}

void AILCALL AIL_mem_free_lock(void* p)
{
    free(p);
}

// ---- 3D: no providers, the game falls back to 2D samples ----
S32 AILCALL AIL_enumerate_3D_providers(HPROENUM*, HPROVIDER*, char**)
{
    return 0;
}
M3DRESULT AILCALL AIL_open_3D_provider(HPROVIDER)
{
    return M3D_NOT_INIT;
}
void AILCALL AIL_close_3D_provider(HPROVIDER)
{
}
void AILCALL AIL_set_3D_provider_preference(HPROVIDER, C8*, void*)
{
}
void AILCALL AIL_3D_provider_attribute(HPROVIDER, C8*, void*)
{
}
H3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER)
{
    return NULL;
}
void AILCALL AIL_release_3D_sample_handle(H3DSAMPLE)
{
}
S32 AILCALL AIL_set_3D_sample_info(H3DSAMPLE, AILSOUNDINFO*)
{
    return 0;
}
void AILCALL AIL_set_3D_sample_volume(H3DSAMPLE, S32)
{
}
void AILCALL AIL_set_3D_sample_playback_rate(H3DSAMPLE, S32)
{
}
void AILCALL AIL_set_3D_sample_loop_count(H3DSAMPLE, U32)
{
}
void AILCALL AIL_set_3D_sample_loop_block(H3DSAMPLE, S32, S32)
{
}
void AILCALL AIL_set_3D_sample_float_distances(H3DSAMPLE, F32, F32, F32, F32)
{
}
void AILCALL AIL_set_3D_position(H3DPOBJECT, F32, F32, F32)
{
}
void AILCALL AIL_start_3D_sample(H3DSAMPLE)
{
}
void AILCALL AIL_end_3D_sample(H3DSAMPLE)
{
}
U32 AILCALL AIL_3D_sample_status(H3DSAMPLE)
{
    return SMP_DONE;
}

} // extern "C"
