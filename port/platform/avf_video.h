// C interface of the AVFoundation movie decoder (avf_video.mm)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AvfMovie;
struct AvfMovie* avf_open(const char* path, int* width, int* height, double* fps, double* duration);
// Decodes the next frame as BGRA into dst; returns 0 at the end of the movie
int avf_next_frame(struct AvfMovie* m, unsigned char* dst, int pitch);
// Whole audio track as interleaved signed 16-bit (malloc'd), or NULL
short* avf_read_audio(const char* path, int* frames, int* rate, int* channels);
void avf_close(struct AvfMovie* m);

#ifdef __cplusplus
}
#endif
