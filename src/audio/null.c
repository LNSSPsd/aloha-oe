#include "pcm.h"

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__DragonFly__) && \
    !defined(__linux__) && !defined(_WIN32) && !defined(__NetBSD__) && \
    !defined(__OpenBSD__) && !defined(ALOHA_USE_OSS)
#include <stdio.h>

int
pcm_play(const float *samples, size_t frames, unsigned rate, const char *device,
    AudioProgress progress, void *context)
{
    (void)samples;
    (void)frames;
    (void)rate;
    (void)device;
    (void)progress;
    (void)context;
    fputs("audio: no native backend for this system; use --no-play --wav FILE\n",
        stderr);
    return 1;
}
#endif
