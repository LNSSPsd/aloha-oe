#include "pcm.h"

#if defined(__NetBSD__) && !defined(ALOHA_USE_OSS)

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { BUFFER_FRAMES = 4096, FRAME_BYTES = 4 };

static int
audio_ioctl(int fd, unsigned long request, void *argument)
{
    int result;

    do {
        result = ioctl(fd, request, argument);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int
audio_configure(int fd, unsigned rate, size_t *blocksize)
{
    audio_info_t info;

    AUDIO_INITINFO(&info);
    info.play.encoding = AUDIO_ENCODING_SLINEAR_LE;
    info.play.precision = 16;
    info.play.channels = 2;
    info.play.sample_rate = rate;
    info.play.pause = 0;
    if (audio_ioctl(fd, AUDIO_SETINFO, &info) < 0) {
        perror("audio: AUDIO_SETINFO");
        return -1;
    }
    if (audio_ioctl(fd, AUDIO_GETBUFINFO, &info) < 0) {
        perror("audio: AUDIO_GETBUFINFO");
        return -1;
    }
    if (info.play.encoding != AUDIO_ENCODING_SLINEAR_LE ||
        info.play.precision != 16 || info.play.channels != 2 ||
        info.play.sample_rate != rate || info.play.pause != 0) {
        fprintf(stderr, "audio: NetBSD device did not accept stereo "
            "signed 16-bit little-endian PCM at %u Hz\n", rate);
        return -1;
    }
    *blocksize = info.blocksize ? info.blocksize : FRAME_BYTES;
    return 0;
}

/* NetBSD 9+ counts play.seek in pending bytes. This is the track/mixer
 * position; downstream hardware buffering can add latency. Only AUDIO_DRAIN
 * establishes completion, so the final frame is withheld until it succeeds. */
static int
audio_position(int fd, size_t submitted, size_t frames, size_t *pending,
    AudioProgress progress, void *context, size_t *last)
{
    audio_info_t info;
    size_t played;

    if (audio_ioctl(fd, AUDIO_GETBUFINFO, &info) < 0) {
        perror("audio: AUDIO_GETBUFINFO");
        return -1;
    }
    *pending = info.play.seek;
    played = submitted > *pending ? (submitted - *pending) / FRAME_BYTES : 0;
    if (frames != 0 && played >= frames)
        played = frames - 1;
    pcm_progress(progress, context, played, last);
    return 0;
}

static int
audio_deadline(double deadline)
{
    double now = pcm_now();

    if (now >= 0 && now < deadline)
        return 0;
    fputs("audio: NetBSD playback timed out or the clock failed\n", stderr);
    return -1;
}

int
pcm_play(const float *samples, size_t frames, unsigned rate, const char *device,
    AudioProgress progress, void *context)
{
    unsigned char buffer[BUFFER_FRAMES * FRAME_BYTES];
    const char *path = device;
    size_t first, submitted = 0, last = SIZE_MAX, pending = 0, blocksize;
    double start, deadline;
    int fd, result = -1;

    if (frames == 0) {
        pcm_progress(progress, context, 0, &last);
        return 0;
    }
    if (path == NULL || *path == '\0' || strcmp(path, "default") == 0)
        path = "/dev/audio";
    fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "audio: opening %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (audio_configure(fd, rate, &blocksize) < 0)
        goto out;
    start = pcm_now();
    if (start < 0) {
        fputs("audio: cannot read the monotonic clock\n", stderr);
        goto out;
    }
    deadline = start + (double)frames / rate + 15.0;
    pcm_progress(progress, context, 0, &last);
    for (first = 0; first < frames;) {
        size_t count = frames - first, bytes, offset = 0;

        if (count > BUFFER_FRAMES)
            count = BUFFER_FRAMES;
        pcm_s16le(buffer, samples + first * 2, count * 2);
        bytes = count * FRAME_BYTES;
        while (offset < bytes) {
            ssize_t written;

            if (audio_deadline(deadline) < 0)
                goto out;
            written = write(fd, buffer + offset, bytes - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (progress != NULL && audio_position(fd, submitted, frames,
                    &pending, progress, context, &last) < 0)
                    goto out;
                pcm_pause();
                continue;
            }
            if (written <= 0) {
                fprintf(stderr, "audio: writing %s: %s\n", path,
                    written == 0 ? "zero-length write" : strerror(errno));
                goto out;
            }
            offset += (size_t)written;
            submitted += (size_t)written;
            if (progress != NULL && audio_position(fd, submitted, frames,
                &pending, progress, context, &last) < 0)
                goto out;
        }
        first += count;
    }
    /* Report the tail while it drains. Let the native drain operation handle
     * the last partial block and the hardware buffers behind the track mixer. */
    if (progress != NULL) {
        for (;;) {
            if (audio_position(fd, submitted, frames, &pending,
                progress, context, &last) < 0)
                goto out;
            if (pending <= blocksize)
                break;
            if (audio_deadline(deadline) < 0)
                goto out;
            pcm_pause();
        }
    }
    if (audio_ioctl(fd, AUDIO_DRAIN, NULL) < 0) {
        perror("audio: AUDIO_DRAIN");
        goto out;
    }
    pcm_progress(progress, context, frames, &last);
    result = 0;

out:
    if (result != 0 && audio_ioctl(fd, AUDIO_FLUSH, NULL) < 0)
        perror("audio: AUDIO_FLUSH");
    if (close(fd) < 0) {
        fprintf(stderr, "audio: closing %s: %s\n", path, strerror(errno));
        result = -1;
    }
    return result;
}

#endif /* __NetBSD__ && !ALOHA_USE_OSS */
