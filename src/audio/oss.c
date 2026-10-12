#include "pcm.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(ALOHA_USE_OSS)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

static int
oss_configure(int fd, unsigned rate)
{
    int format = AFMT_S16_LE, channels = 2, speed = (int)rate;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != AFMT_S16_LE) {
        fprintf(stderr, "audio: OSS output cannot supply signed 16-bit little-endian PCM\n");
        return 1;
    }
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) < 0 || channels != 2) {
        fprintf(stderr, "audio: OSS output cannot supply stereo PCM\n");
        return 1;
    }
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) < 0) {
        fprintf(stderr, "audio: configuring OSS sample rate failed: %s\n",
            strerror(errno));
        return 1;
    }
    if (speed != (int)rate) {
        fprintf(stderr, "audio: OSS negotiated %d Hz instead of %u Hz; refusing a pitch change\n",
            speed, rate);
        return 1;
    }
    return 0;
}

static int
oss_position(int fd, size_t submitted_bytes, AudioProgress progress,
    void *context, size_t *last_position, int *position_started, int *pending)
{
    int status;
    do { status = ioctl(fd, SNDCTL_DSP_GETODELAY, pending); }
    while (status < 0 && errno == EINTR);
    if (status < 0 || *pending < 0) {
        fprintf(stderr, "audio: reading OSS playback position failed: %s\n",
            status < 0 ? strerror(errno) : "negative output delay");
        return 1;
    }
    size_t queued = (size_t)*pending;
    size_t played = submitted_bytes > queued ? (submitted_bytes - queued) / 4 : 0;
    if (!*position_started || played > *last_position) {
        progress(context, played);
        *last_position = played;
        *position_started = 1;
    }
    return 0;
}

int
pcm_play(const float *stereo, size_t frames, unsigned rate,
    const char *device, AudioProgress progress, void *context)
{
    unsigned char pcm[4096 * 2 * 2];
    int fd, result = 1;
    size_t submitted_bytes = 0, last_position = 0;
    int position_started = 0, pending = 0;
    double deadline = 0;
    const char *path = device;
    if (path == NULL || path[0] == '\0' || strcmp(path, "default") == 0)
        path = "/dev/dsp";
    fd = open(path, O_WRONLY | (progress != NULL ? O_NONBLOCK : 0));
    if (fd < 0) {
        fprintf(stderr, "audio: opening %s failed: %s\n", path, strerror(errno));
        return 1;
    }
    if (oss_configure(fd, rate))
        goto cleanup;
    if (progress != NULL) {
        double start = pcm_now();
        if (start < 0) {
            fprintf(stderr, "audio: cannot read monotonic clock\n");
            goto cleanup;
        }
        deadline = start + (double)frames / rate + 10.0;
    }
    for (size_t first = 0; first < frames;) {
        size_t count = frames - first, bytes, written = 0;
        if (count > 4096)
            count = 4096;
        pcm_s16le(pcm, stereo + first * 2, count * 2);
        bytes = count * 2 * 2;
        while (written < bytes) {
            ssize_t n = write(fd, pcm + written, bytes - written);
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 && progress != NULL && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (oss_position(fd, submitted_bytes, progress, context,
                    &last_position, &position_started, &pending))
                    goto cleanup;
                double now = pcm_now();
                if (now < 0 || now > deadline) {
                    fprintf(stderr, "audio: OSS playback did not finish before its deadline\n");
                    goto cleanup;
                }
                pcm_pause();
                continue;
            }
            if (n <= 0) {
                fprintf(stderr, "audio: writing %s failed: %s\n", path,
                    n == 0 ? "zero-length write" : strerror(errno));
                goto cleanup;
            }
            written += (size_t)n;
            submitted_bytes += (size_t)n;
            if (progress != NULL && oss_position(fd, submitted_bytes, progress,
                context, &last_position, &position_started, &pending))
                goto cleanup;
        }
        first += count;
    }
    /* Keep delivering lyrics while the last queued buffers are sounding;
     * a blocking SYNC alone would freeze progress during this final tail. */
    if (progress != NULL) {
        do {
            if (oss_position(fd, submitted_bytes, progress, context,
                &last_position, &position_started, &pending))
                goto cleanup;
            if (pending == 0)
                break;
            double now = pcm_now();
            if (now < 0 || now > deadline) {
                fprintf(stderr, "audio: OSS output drain timed out\n");
                goto cleanup;
            }
            pcm_pause();
        } while (pending > 0);
    }
    {
        int status;
        do {
            status = ioctl(fd, SNDCTL_DSP_SYNC, NULL);
        } while (status < 0 && errno == EINTR);
        if (status < 0) {
            fprintf(stderr, "audio: draining %s failed: %s\n", path, strerror(errno));
            goto cleanup;
        }
    }
    if (progress != NULL && last_position < frames)
        progress(context, frames);
    result = 0;
cleanup:
    if (close(fd) < 0) {
        fprintf(stderr, "audio: closing %s failed: %s\n", path, strerror(errno));
        result = 1;
    }
    return result;
}

#endif
