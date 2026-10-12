#ifndef ALOHA_PCM_H
#define ALOHA_PCM_H

#include "../audio.h"
#include <stdint.h>

/* Private interface. Exactly one native backend supplies pcm_play(). */
int pcm_play(const float *, size_t, unsigned, const char *, AudioProgress, void *);

double pcm_now(void);
void pcm_pause(void);
int16_t pcm_s16(float);
void pcm_s16le(unsigned char *, const float *, size_t);

/* Initialize *last to SIZE_MAX. Repeated/backward positions are suppressed. */
void pcm_progress(AudioProgress, void *, size_t, size_t *);

#endif
