#pragma once
#include "windows.h"
typedef UINT MMRESULT;
typedef struct HWAVEOUT__* HWAVEOUT;
typedef struct HWAVEIN__* HWAVEIN;
typedef struct HMIDIOUT__* HMIDIOUT;
typedef HWAVEOUT* LPHWAVEOUT;
typedef HMIDIOUT* LPHMIDIOUT;
typedef struct waveformat_tag
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
} WAVEFORMAT, *LPWAVEFORMAT, *PWAVEFORMAT;
typedef struct pcmwaveformat_tag
{
    WAVEFORMAT wf;
    WORD wBitsPerSample;
} PCMWAVEFORMAT, *LPPCMWAVEFORMAT;
typedef struct tWAVEFORMATEX
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct wavehdr_tag
{
    LPSTR lpData;
    DWORD dwBufferLength;
    DWORD dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags;
    DWORD dwLoops;
    struct wavehdr_tag* lpNext;
    DWORD_PTR reserved;
} WAVEHDR, *LPWAVEHDR;
typedef struct midihdr_tag
{
    LPSTR lpData;
    DWORD dwBufferLength;
    DWORD dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags;
    struct midihdr_tag* lpNext;
    DWORD_PTR reserved;
    DWORD dwOffset;
    DWORD_PTR dwReserved[8];
} MIDIHDR, *LPMIDIHDR;
#define WAVE_FORMAT_PCM 1
#define MMSYSERR_NOERROR 0
