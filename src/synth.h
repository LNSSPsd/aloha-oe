#ifndef ALOHA_SYNTH_H
#define ALOHA_SYNTH_H

#include <stddef.h>

enum Instrument {
    INST_GUITAR,
    INST_BASS,
    INST_STEEL,
    INST_PIANO,
    INST_SHAKER,
    /* Source vowel family; a/e/i/o/u order. A Note's IPA sequence can override
     * the vocal-tract targets with consonants, diphthongs and word endings. */
    INST_VOICE_A,
    INST_VOICE_E,
    INST_VOICE_I,
    INST_VOICE_O,
    INST_VOICE_U
};

enum VoiceStyle { VOICE_BARITONE, VOICE_RETRO };

/* Times are seconds; pitch is equal-tempered MIDI with A4 = 440 Hz.
 * Velocity is clamped to [0, 1], and pan to [-1 (left), 1 (right)]. */
struct Note {
    double start;
    double duration;
    int midi;
    double velocity;
    double pan;
    enum Instrument instrument;
    /* An optional initial bend in semitones, resolving to the scored pitch. */
    double bend_semitones;
    /* A valid voice instrument makes a diphthong; zero holds the first vowel. */
    enum Instrument vowel_end;
    /* ASCII consonant onset: h/k/p/t/b/d/g/s/f/v/l/m/n/w/y/r/q; q is a stop. */
    char consonant;
    /* Optional UTF-8 IPA syllable with timed onset, nucleus and coda. */
    const char *ipa;
    enum VoiceStyle voice_style;
    /* Explicit instrumental slide duration; zero retains the short legacy
     * exponential bend. The target remains the score's nominal pitch. */
    double bend_seconds;
    /* Optional instrumental swell/legato attack; zero uses the instrument's
     * normal attack. Does not move the scheduled note start. */
    double attack_seconds;
};

/* Interleaved stereo floats. Ownership passes to the caller on success. */
struct Audio {
    float *samples;
    size_t frames;
    unsigned rate;
};

double synth_frequency(int midi);
const char *synth_instrument_name(enum Instrument instrument);

/* Return 0 on success, -1 with errno set on invalid input/allocation failure.
 * Output peak is normalized to volume (clamped to [0, 0.9]); wet enables room.
 * The caller must release an earlier Audio before rendering into it again. */
int synth_render(const struct Note *notes, size_t n, struct Audio *out,
                 double seconds, unsigned rate, double volume, int wet);

/* Identical renderer without peak normalization, for a mix and its stems to
 * share one externally chosen master gain. Samples can exceed [-1, 1]. */
int synth_render_raw(const struct Note *notes, size_t n, struct Audio *out,
                     double seconds, unsigned rate, int wet);

/* A single dry note, starting at zero, for audition and pitch diagnostics. */
int synth_note_render(const struct Note *note, struct Audio *out,
                      double seconds, unsigned rate, double volume);

void synth_free(struct Audio *audio);

#endif
