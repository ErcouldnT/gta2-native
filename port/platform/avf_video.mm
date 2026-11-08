// Movie decoding with AVFoundation (macOS and iOS). Kept free of the game's Win32/MSVC
// compatibility headers: it only exposes the small C interface in avf_video.h.
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include "avf_video.h"
#include <stdlib.h>
#include <string.h>

struct AvfMovie
{
    AVAssetReader* videoReader;
    AVAssetReaderTrackOutput* videoOutput;
    int width;
    int height;
    double fps;
    double duration;
};

extern "C" AvfMovie* avf_open(const char* path, int* width, int* height, double* fps, double* duration)
{
    @autoreleasepool
    {
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
        NSArray<AVAssetTrack*>* tracks = [asset tracksWithMediaType:AVMediaTypeVideo];
        if (tracks.count == 0)
        {
            return NULL;
        }
        AVAssetTrack* track = tracks[0];
        NSError* err = nil;
        AVAssetReader* reader = [AVAssetReader assetReaderWithAsset:asset error:&err];
        if (!reader)
        {
            return NULL;
        }
        NSDictionary* settings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
        AVAssetReaderTrackOutput* out = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track outputSettings:settings];
        out.alwaysCopiesSampleData = NO;
        [reader addOutput:out];
        if (![reader startReading])
        {
            return NULL;
        }
        AvfMovie* m = new AvfMovie();
        m->videoReader = reader;
        m->videoOutput = out;
        CGSize size = track.naturalSize;
        m->width = (int)size.width;
        m->height = (int)size.height;
        m->fps = track.nominalFrameRate > 0 ? track.nominalFrameRate : 15.0;
        m->duration = CMTimeGetSeconds(asset.duration);
        *width = m->width;
        *height = m->height;
        *fps = m->fps;
        *duration = m->duration;
        return m;
    }
}

extern "C" int avf_next_frame(AvfMovie* m, unsigned char* dst, int pitch)
{
    if (!m)
    {
        return 0;
    }
    @autoreleasepool
    {
        CMSampleBufferRef sample = [m->videoOutput copyNextSampleBuffer];
        if (!sample)
        {
            return 0;
        }
        CVImageBufferRef img = CMSampleBufferGetImageBuffer(sample);
        if (img)
        {
            CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
            const unsigned char* src = (const unsigned char*)CVPixelBufferGetBaseAddress(img);
            size_t srcPitch = CVPixelBufferGetBytesPerRow(img);
            int rows = (int)CVPixelBufferGetHeight(img);
            int cols = (int)CVPixelBufferGetWidth(img);
            if (rows > m->height)
            {
                rows = m->height;
            }
            if (cols > m->width)
            {
                cols = m->width;
            }
            for (int y = 0; y < rows; y++)
            {
                memcpy(dst + (size_t)y * pitch, src + (size_t)y * srcPitch, (size_t)cols * 4);
            }
            CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        }
        CFRelease(sample);
        return 1;
    }
}

extern "C" short* avf_read_audio(const char* path, int* frames, int* rate, int* channels)
{
    @autoreleasepool
    {
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
        NSArray<AVAssetTrack*>* tracks = [asset tracksWithMediaType:AVMediaTypeAudio];
        if (tracks.count == 0)
        {
            return NULL;
        }
        NSError* err = nil;
        AVAssetReader* reader = [AVAssetReader assetReaderWithAsset:asset error:&err];
        if (!reader)
        {
            return NULL;
        }
        const int outRate = 44100, outCh = 2;
        NSDictionary* settings = @{
            AVFormatIDKey : @(kAudioFormatLinearPCM),
            AVSampleRateKey : @(outRate),
            AVNumberOfChannelsKey : @(outCh),
            AVLinearPCMBitDepthKey : @16,
            AVLinearPCMIsFloatKey : @NO,
            AVLinearPCMIsBigEndianKey : @NO,
            AVLinearPCMIsNonInterleaved : @NO,
        };
        AVAssetReaderTrackOutput* out = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:tracks[0] outputSettings:settings];
        [reader addOutput:out];
        if (![reader startReading])
        {
            return NULL;
        }
        size_t cap = 1 << 20, len = 0;
        short* buf = (short*)malloc(cap * sizeof(short));
        while (CMSampleBufferRef sample = [out copyNextSampleBuffer])
        {
            CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
            size_t bytes = block ? CMBlockBufferGetDataLength(block) : 0;
            size_t n = bytes / sizeof(short);
            if (len + n > cap)
            {
                while (len + n > cap)
                {
                    cap *= 2;
                }
                buf = (short*)realloc(buf, cap * sizeof(short));
            }
            if (bytes)
            {
                CMBlockBufferCopyDataBytes(block, 0, bytes, buf + len);
                len += n;
            }
            CFRelease(sample);
        }
        *frames = (int)(len / outCh);
        *rate = outRate;
        *channels = outCh;
        return buf;
    }
}

extern "C" void avf_close(AvfMovie* m)
{
    if (!m)
    {
        return;
    }
    @autoreleasepool
    {
        [m->videoReader cancelReading];
        m->videoReader = nil;
        m->videoOutput = nil;
    }
    delete m;
}
