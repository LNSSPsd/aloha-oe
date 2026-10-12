#ifndef ALOHA_AUDIO_H
#define ALOHA_AUDIO_H

#include <stddef.h>

/* Interleaved left/right PCM, finite samples in [-1, 1].
 * device: NULL/empty/"default" selects the backend default; otherwise a
 * CoreAudio UID, a native Unix device path, or a Windows waveOut device ID.
 * The caller retains the samples until this blocking function returns.
 * Returns zero on success, nonzero after printing a diagnostic. */
int play_audio(const float *stereo, size_t frames, unsigned rate,
    const char *device);

/* Optional notification on the calling thread, never the realtime audio
 * callback. Position follows device consumption, never simple submission;
 * precision/latency depends on the backend (NetBSD reports track consumption).
 * Calls are monotonic and include the final frame count after successful drain.
 * The callback should return promptly. */
typedef void (*AudioProgress)(void *context, size_t played_frames);
int play_audio_with_progress(const float *stereo, size_t frames, unsigned rate,
    const char *device, AudioProgress progress, void *context);

#endif
