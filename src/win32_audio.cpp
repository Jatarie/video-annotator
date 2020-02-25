#include "windows.h"
#include <Dsound.h>
#include "inttypes.h"

static LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer;

typedef HRESULT WINAPI direct_sound_create(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);

struct win32_sound_output {
    int SamplesPerSecond;
    int Channels;
    uint32_t RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize;
    int LatencySampleCount;
};

win32_sound_output InitSoundOutput() {
    win32_sound_output SoundOutput = {};
    SoundOutput.SamplesPerSecond = 44100;
    SoundOutput.Channels = 2;
    SoundOutput.BytesPerSample = sizeof(float) * SoundOutput.Channels;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 5;
    return SoundOutput;
}

static void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, float* SourceBuffer) {
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;
    float* SourceSample = SourceBuffer;
    if (SUCCEEDED(GlobalSecondarySoundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
        float* DestSample = (float*)Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (int SampleCount = 0; SampleCount < Region1SampleCount; SampleCount++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }

        DestSample = (float*)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (int SampleCount = 0; SampleCount < Region2SampleCount; SampleCount++) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }
        if (SUCCEEDED(GlobalSecondarySoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size))) {
        }
    }
}

static void Win32InitDSound(HWND Window, uint32_t SamplesPerSecond, uint32_t BufferSize) {
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    direct_sound_create* DirectSoundCreate;
    if (DSoundLibrary) {
        DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
    }

    LPDIRECTSOUND DirectSound;
    if (SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
        DSBUFFERDESC BufferDescription = {};
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        BufferDescription.dwSize = sizeof(DSBUFFERDESC);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
        BufferDescription.dwBufferBytes = 0;
        if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
                WAVEFORMATEX WaveFormat = {};
                WaveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
                WaveFormat.nChannels = 2;
                WaveFormat.nSamplesPerSec = SamplesPerSecond;
                WaveFormat.wBitsPerSample = 32;
                WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
                WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
                if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                    BufferDescription.dwFlags = 0;
                    BufferDescription.dwBufferBytes = BufferSize;
                    BufferDescription.lpwfxFormat = &WaveFormat;
                    if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondarySoundBuffer, 0))) {
                    }
                    else {
                        OutputDebugStringA("Failed to create secondary buffer");
                    }
                }
                else {
                    OutputDebugStringA("Failed to set format on primary buffer");
                }
            }
            else {
                OutputDebugStringA("Failed to create primary buffer");
            }
        }
        else {
            OutputDebugStringA("Failed to set cooperative level");
        }
    }
    else {
        OutputDebugStringA("Failed to create DirectSound object");
    }
}