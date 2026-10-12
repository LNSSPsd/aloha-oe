#ifndef ALOHA_PHONEME_H
#define ALOHA_PHONEME_H

#include <stddef.h>

/* Generic source/filter targets. Frequencies and bandwidths are Hz. Voiced
 * and noise are independent source levels; fric_freq=0 selects aspiration
 * through the vowel filters instead of a separate turbulent noise band. */
struct PhoneState {
    double formant[3], bandwidth[3];
    double voiced, noise, fric_freq, fric_width, nasal;
};

struct PhoneToken {
    unsigned codepoint;
    double start, end;
    struct PhoneState state;
};

struct PhoneSequence {
    struct PhoneToken tokens[96];
    size_t count;
    double duration;
};

/* UTF-8 IPA for one note/syllable. Returns -1 with errno set on malformed
 * input, unknown symbols, or an overlong sequence. Empty input is invalid. */
int phoneme_plan(const char *utf8, double seconds, struct PhoneSequence *out);

/* Interpolated phoneme state; outside [0, duration) it returns a silent state.
 * Vowels occupy the sustain; consonants retain short onset/coda articulation. */
void phoneme_at(const struct PhoneSequence *sequence, double t,
    struct PhoneState *out);

#endif
