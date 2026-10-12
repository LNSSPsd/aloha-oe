#define _POSIX_C_SOURCE 200809L
#include "pcm.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

double
pcm_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER ticks, frequency;

    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&ticks))
        return -1.0;
    return (double)ticks.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return -1.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

void
pcm_pause(void)
{
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec delay = { 0, 10000000 };

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR)
        ;
#endif
}

int16_t
pcm_s16(float sample)
{
    if (sample < -1.0f)
        sample = -1.0f;
    else if (sample > 1.0f)
        sample = 1.0f;
    return (int16_t)lrintf(sample * 32767.0f);
}

void
pcm_s16le(unsigned char *dst, const float *src, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        uint16_t value = (uint16_t)pcm_s16(src[i]);
        dst[2 * i] = (unsigned char)value;
        dst[2 * i + 1] = (unsigned char)(value >> 8);
    }
}

void
pcm_progress(AudioProgress callback, void *context, size_t position, size_t *last)
{
    if (callback != NULL && (*last == SIZE_MAX || position > *last)) {
        callback(context, position);
        *last = position;
    }
}

int
play_audio_with_progress(const float *samples, size_t frames, unsigned rate,
    const char *device, AudioProgress progress, void *context)
{
    size_t i;

    if (rate == 0 || rate > INT_MAX || frames > SIZE_MAX / (2 * sizeof(float)) ||
        (frames != 0 && samples == NULL)) {
        fputs("audio: invalid PCM buffer or sample rate\n", stderr);
        return 1;
    }
    if (frames == 0)
        return 0;
    for (i = 0; i < frames * 2; ++i) {
        if (!isfinite(samples[i])) {
            fprintf(stderr, "audio: nonfinite sample at %zu\n", i);
            return 1;
        }
    }
    return pcm_play(samples, frames, rate, device, progress, context);
}

int
play_audio(const float *samples, size_t frames, unsigned rate, const char *device)
{
    return play_audio_with_progress(samples, frames, rate, device, NULL, NULL);
}
