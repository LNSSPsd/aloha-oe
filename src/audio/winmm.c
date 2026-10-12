#include "pcm.h"

#if defined(_WIN32) && !defined(ALOHA_USE_OSS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NBUFFERS = 3, BUFFER_FRAMES = 4096 };

struct wave_buffer {
    WAVEHDR header;
    int16_t samples[BUFFER_FRAMES * 2];
    size_t end_frame;
    int prepared;
    int queued;
};

struct wave_output {
    HWAVEOUT handle;
    HANDLE event;
    struct wave_buffer buffers[NBUFFERS];
};

struct wave_position {
    DWORD previous[3];
    uint64_t ticks[3];
    unsigned seen;
};

static void
wave_error(const char *what, MMRESULT error)
{
    char text[MAXERRORLENGTH];

    if (waveOutGetErrorTextA(error, text, sizeof(text)) != MMSYSERR_NOERROR)
        snprintf(text, sizeof(text), "error %u", (unsigned)error);
    fprintf(stderr, "%s: %s\n", what, text);
}

static int
wave_device(const char *name, UINT *device)
{
    const char *p;
    char *end;
    unsigned long number;

    *device = WAVE_MAPPER;
    if (name == NULL || *name == '\0' || strcmp(name, "default") == 0)
        return 0;
    for (p = name; *p != '\0'; ++p)
        if (*p < '0' || *p > '9')
            goto invalid;
    errno = 0;
    number = strtoul(name, &end, 10);
    if (errno == ERANGE || *end != '\0' || number > UINT_MAX ||
        number >= waveOutGetNumDevs())
        goto invalid;
    *device = (UINT)number;
    return 0;

invalid:
    fprintf(stderr, "waveOut device must be default or a decimal device ID "
        "from 0 to the number of output devices minus one: %s\n", name);
    return -1;
}

/* Some drivers substitute bytes or milliseconds for TIME_SAMPLES. Extend
 * each DWORD counter independently; small backwards driver jitter is ignored.
 * An unsuccessful query leaves the caller's completed-buffer position intact. */
static size_t
wave_position(HWAVEOUT handle, struct wave_position *position, unsigned rate,
    size_t completed, size_t submitted)
{
    MMTIME time;
    DWORD value, delta;
    uint64_t frames;
    unsigned unit;

    memset(&time, 0, sizeof(time));
    time.wType = TIME_SAMPLES;
    if (waveOutGetPosition(handle, &time, sizeof(time)) != MMSYSERR_NOERROR)
        return completed;
    switch (time.wType) {
    case TIME_SAMPLES:
        unit = 0;
        value = time.u.sample;
        break;
    case TIME_BYTES:
        unit = 1;
        value = time.u.cb;
        break;
    case TIME_MS:
        unit = 2;
        value = time.u.ms;
        break;
    default:
        return completed;
    }
    if (!(position->seen & (1U << unit))) {
        position->ticks[unit] = value;
        position->previous[unit] = value;
        position->seen |= 1U << unit;
    } else {
        delta = value - position->previous[unit];
        if (delta < UINT32_C(0x80000000)) {
            position->ticks[unit] += delta;
            position->previous[unit] = value;
        }
    }
    frames = position->ticks[unit];
    if (unit == 1)
        frames /= 2 * sizeof(int16_t);
    else if (unit == 2)
        frames = frames * rate / 1000;
    if (frames < completed)
        frames = completed;
    if (frames > submitted)
        frames = submitted;
    return (size_t)frames;
}

static MMRESULT
wave_submit(struct wave_output *output, struct wave_buffer *buffer,
    const float *samples, size_t *next, size_t frames)
{
    size_t count, i;
    MMRESULT error;

    count = frames - *next;
    if (count > BUFFER_FRAMES)
        count = BUFFER_FRAMES;
    for (i = 0; i < count * 2; ++i)
        buffer->samples[i] = pcm_s16(samples[*next * 2 + i]);
    buffer->header.dwBufferLength = (DWORD)(count * 2 * sizeof(int16_t));
    buffer->header.dwFlags &= ~WHDR_DONE;
    error = waveOutWrite(output->handle, &buffer->header,
        sizeof(buffer->header));
    if (error == MMSYSERR_NOERROR) {
        *next += count;
        buffer->end_frame = *next;
        buffer->queued = 1;
    }
    return error;
}

/* The driver owns prepared headers until unprepare succeeds. Keep this small
 * heap allocation (and its event) alive if a broken driver refuses cleanup;
 * no retained object points into the caller's sample buffer or stack. */
static int
wave_dispose(struct wave_output *output, int abort_playback)
{
    MMRESULT error;
    double deadline, now;
    int failed = 0, retained = 0;
    unsigned i;

    if (output->handle != NULL) {
        if (abort_playback) {
            error = waveOutReset(output->handle);
            if (error != MMSYSERR_NOERROR) {
                wave_error("waveOutReset", error);
                failed = 1;
            }
        }
        deadline = pcm_now() + 2.0;
        for (i = 0; i < NBUFFERS; ++i) {
            struct wave_buffer *buffer = &output->buffers[i];

            if (!buffer->prepared)
                continue;
            for (;;) {
                error = waveOutUnprepareHeader(output->handle,
                    &buffer->header, sizeof(buffer->header));
                now = pcm_now();
                if (error != WAVERR_STILLPLAYING || now < 0 || now >= deadline)
                    break;
                WaitForSingleObject(output->event, 10);
            }
            if (error != MMSYSERR_NOERROR) {
                wave_error("waveOutUnprepareHeader", error);
                retained = 1;
            }
        }
        error = waveOutClose(output->handle);
        if (error != MMSYSERR_NOERROR) {
            wave_error("waveOutClose", error);
            retained = 1;
        }
    }
    if (retained) {
        fputs("waveOut: retaining driver-owned buffers after cleanup failure\n",
            stderr);
        return -1;
    }
    if (output->event != NULL && !CloseHandle(output->event)) {
        fprintf(stderr, "CloseHandle: Windows error %lu\n",
            (unsigned long)GetLastError());
        failed = 1;
    }
    free(output);
    return failed ? -1 : 0;
}

int
pcm_play(const float *samples, size_t frames, unsigned rate, const char *device,
    AudioProgress progress, void *context)
{
    struct wave_output *output;
    struct wave_position position = {{0}, {0}, 0};
    WAVEFORMATEX format;
    MMRESULT error;
    UINT id;
    size_t next = 0, completed = 0, last = SIZE_MAX;
    double deadline, now;
    unsigned pending = 0, i;
    int result = -1;

    if (wave_device(device, &id) != 0)
        return -1;
    if (rate == 0 || rate > UINT32_MAX / 4) {
        fputs("waveOut: sample rate is out of range\n", stderr);
        return -1;
    }
    if (frames == 0) {
        pcm_progress(progress, context, 0, &last);
        return 0;
    }
    output = calloc(1, sizeof(*output));
    if (output == NULL) {
        perror("waveOut buffers");
        return -1;
    }
    output->event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (output->event == NULL) {
        fprintf(stderr, "CreateEvent: Windows error %lu\n",
            (unsigned long)GetLastError());
        goto out;
    }
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = rate * format.nBlockAlign;
    error = waveOutOpen(&output->handle, id, &format,
        (DWORD_PTR)output->event, 0, CALLBACK_EVENT);
    if (error != MMSYSERR_NOERROR) {
        output->handle = NULL;
        wave_error("waveOutOpen", error);
        goto out;
    }
    error = waveOutPause(output->handle);
    if (error != MMSYSERR_NOERROR) {
        wave_error("waveOutPause", error);
        goto out;
    }
    for (i = 0; i < NBUFFERS && next < frames; ++i) {
        struct wave_buffer *buffer = &output->buffers[i];

        buffer->header.lpData = (LPSTR)buffer->samples;
        buffer->header.dwBufferLength = sizeof(buffer->samples);
        error = waveOutPrepareHeader(output->handle, &buffer->header,
            sizeof(buffer->header));
        if (error != MMSYSERR_NOERROR) {
            wave_error("waveOutPrepareHeader", error);
            goto out;
        }
        buffer->prepared = 1;
        error = wave_submit(output, buffer, samples, &next, frames);
        if (error != MMSYSERR_NOERROR) {
            wave_error("waveOutWrite", error);
            goto out;
        }
        ++pending;
    }
    error = waveOutRestart(output->handle);
    if (error != MMSYSERR_NOERROR) {
        wave_error("waveOutRestart", error);
        goto out;
    }
    pcm_progress(progress, context, 0, &last);
    now = pcm_now();
    if (now < 0) {
        fputs("waveOut: cannot read the monotonic clock\n", stderr);
        goto out;
    }
    deadline = now + (double)frames / rate + 15.0;
    while (pending != 0) {
        DWORD wait;

        wait = WaitForSingleObject(output->event, 10);
        if (wait == WAIT_FAILED) {
            fprintf(stderr, "WaitForSingleObject: Windows error %lu\n",
                (unsigned long)GetLastError());
            goto out;
        }
        for (i = 0; i < NBUFFERS; ++i) {
            struct wave_buffer *buffer = &output->buffers[i];

            if (buffer->queued && (buffer->header.dwFlags & WHDR_DONE)) {
                buffer->queued = 0;
                --pending;
                if (completed < buffer->end_frame)
                    completed = buffer->end_frame;
            }
        }
        pcm_progress(progress, context,
            wave_position(output->handle, &position, rate, completed, next),
            &last);
        for (i = 0; i < NBUFFERS && next < frames; ++i) {
            struct wave_buffer *buffer = &output->buffers[i];

            if (buffer->queued)
                continue;
            error = wave_submit(output, buffer, samples, &next, frames);
            if (error != MMSYSERR_NOERROR) {
                wave_error("waveOutWrite", error);
                goto out;
            }
            ++pending;
        }
        now = pcm_now();
        if ((now < 0 || now >= deadline) && pending != 0) {
            fputs("waveOut: playback timed out or the clock failed\n", stderr);
            goto out;
        }
    }
    /* Normal WHDR_DONE completion means playback finished. Reset is reserved
     * for the error path: it also sets DONE, but discards pending audio. */
    pcm_progress(progress, context, frames, &last);
    result = 0;

out:
    if (wave_dispose(output, result != 0) != 0)
        result = -1;
    return result;
}

#endif /* _WIN32 */
