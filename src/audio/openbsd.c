#include "pcm.h"

#if defined(__OpenBSD__) && !defined(ALOHA_USE_OSS)
#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Native audio(4), without a sndio client or a sound server. */
static int
audio_ioctl(int fd, unsigned long request, void *arg)
{
    int result;

    do {
        result = ioctl(fd, request, arg);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int
audio_configure(int fd, unsigned rate, size_t *block, size_t *capacity)
{
    struct audio_swpar par;

    AUDIO_INITPAR(&par);
    par.sig = 1;
    par.le = 1;
    par.bits = 16;
    par.bps = 2;
    par.msb = 1;
    par.pchan = 2;
    par.rate = rate;
    par.round = 1024;
    par.nblks = 4;
    if (audio_ioctl(fd, AUDIO_SETPAR, &par) < 0 ||
        audio_ioctl(fd, AUDIO_GETPAR, &par) < 0)
        return -1;
    if (par.sig != 1 || par.le != 1 || par.bits != 16 || par.bps != 2 ||
        par.pchan != 2 || par.rate != rate) {
        fprintf(stderr, "audio: device cannot supply %u Hz stereo S16_LE; "
            "refusing a format or pitch change\n", rate);
        errno = EINVAL;
        return -1;
    }
    /* round is frames, not bytes: include both channels in the buffer size.
     * See sys/dev/audio.c, audio_setpar(), in the OpenBSD kernel. */
    if (par.round == 0 || par.nblks == 0 ||
        (uint64_t)par.round * par.nblks > 4 * 1024 * 1024) {
        errno = EINVAL;
        return -1;
    }
    *block = (size_t)par.round * 4;
    *capacity = *block * par.nblks;
    return 0;
}

static int
audio_position(int fd, size_t frames, uint32_t *previous, uint64_t *bytes,
    AudioProgress progress, void *context, size_t *last)
{
    struct audio_pos pos;
    size_t played;

    if (audio_ioctl(fd, AUDIO_GETPOS, &pos) < 0)
        return -1;
    *bytes += (uint32_t)(pos.play_pos - *previous);
    *previous = pos.play_pos;
    played = *bytes / 4 < frames ? (size_t)(*bytes / 4) : frames;
    /* An underrun makes the wall clock include silence. Fail rather than
     * letting it advance the lyrics; the caller supplies a silent guard tail
     * so the normal final data block does not trigger an underrun. */
    if (pos.play_xrun != 0) {
        errno = EPIPE;
        return -1;
    }
    pcm_progress(progress, context, played, last);
    return played == frames;
}

int
pcm_play(const float *stereo, size_t frames, unsigned rate, const char *device,
    AudioProgress progress, void *context)
{
    unsigned char samples[4096 * 4];
    const char *operation = "opening PCM device";
    size_t block, capacity, padded, sent = 0, count, real, offset, last = SIZE_MAX;
    uint32_t previous = 0;
    uint64_t played_bytes = 0;
    double deadline, now;
    ssize_t written;
    int fd, started = 0, result = 1, status, saved_errno;

    if (device == NULL || *device == '\0' || strcmp(device, "default") == 0)
        device = "/dev/audio0";
    fd = open(device, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        goto fail_open;
    operation = "configuring PCM device";
    if (audio_ioctl(fd, AUDIO_STOP, NULL) < 0 ||
        audio_configure(fd, rate, &block, &capacity) < 0)
        goto fail;
    block /= 4;
    capacity /= 4;
    /* Whole final blocks and a full initial buffer are required by audio(4).
     * Padding is silence after the music, never part of lyric progress. */
    padded = frames + (block - frames % block) % block;
    if (padded < capacity)
        padded = capacity;
    padded += capacity;
    operation = "reading the monotonic clock";
    now = pcm_now();
    if (now < 0) {
        errno = EIO;
        goto fail;
    }
    deadline = now + (double)frames / rate + 10.0;
    while (sent < padded) {
        count = padded - sent;
        if (count > sizeof(samples) / 4)
            count = sizeof(samples) / 4;
        if (!started && count > capacity - sent)
            count = capacity - sent;
        real = sent < frames ? frames - sent : 0;
        if (real > count)
            real = count;
        if (real != 0)
            pcm_s16le(samples, stereo + sent * 2, real * 2);
        memset(samples + real * 4, 0, (count - real) * 4);
        offset = 0;
        while (offset < count * 4) {
            operation = "writing PCM bytes";
            written = write(fd, samples + offset, count * 4 - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written > 0)
                offset += (size_t)written;
            else if (written == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                if (written == 0)
                    errno = EIO;
                goto fail;
            }
            if (started) {
                operation = "reading PCM playback position";
                status = audio_position(fd, frames, &previous, &played_bytes,
                    progress, context, &last);
                if (status < 0)
                    goto fail;
                if (status > 0)
                    goto completed;
            }
            now = pcm_now();
            if (now < 0 || now > deadline) {
                errno = ETIMEDOUT;
                goto fail;
            }
            if (written < 0)
                pcm_pause();
        }
        sent += count;
        if (!started && sent == capacity) {
            operation = "starting PCM device";
            /* The kernel can activate the stream before hardware start fails. */
            started = 1;
            if (audio_ioctl(fd, AUDIO_START, NULL) < 0)
                goto fail;
            pcm_progress(progress, context, 0, &last);
        }
    }
    for (;;) {
        operation = "waiting for final PCM block";
        status = audio_position(fd, frames, &previous, &played_bytes,
            progress, context, &last);
        if (status < 0)
            goto fail;
        if (status > 0)
            break;
        now = pcm_now();
        if (now < 0 || now > deadline) {
            errno = ETIMEDOUT;
            goto fail;
        }
        pcm_pause();
    }
completed:
    result = 0;
    goto done;

fail:
    saved_errno = errno;
    fprintf(stderr, "audio: %s (%s): %s\n", operation, device,
        strerror(saved_errno));
    if (saved_errno == EPIPE)
        fputs("audio: PCM underrun; stopped to preserve lyric timing\n", stderr);
done:
    /* STOP also prevents close() from performing an unobserved extra drain. */
    if (started && audio_ioctl(fd, AUDIO_STOP, NULL) < 0) {
        fprintf(stderr, "audio: stopping %s: %s\n", device, strerror(errno));
        result = 1;
    }
    if (close(fd) < 0) {
        fprintf(stderr, "audio: closing %s: %s\n", device, strerror(errno));
        result = 1;
    }
    return result;

fail_open:
    fprintf(stderr, "audio: opening %s: %s\n", device, strerror(errno));
    fputs("audio: native OpenBSD audio access needs a free device; "
        "sndiod may already own it\n", stderr);
    return 1;
}
#endif
