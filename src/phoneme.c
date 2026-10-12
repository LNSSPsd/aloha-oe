/* Small, independently written IPA-to-source/filter planner.
 *
 * Architecture references, not copied code or SAM tables:
 * Dennis Klatt, JASA 67 (1980), 971-995, DOI 10.1121/1.383940:
 * https://www.fon.hum.uva.nl/david/ma_ssp/doc/Klatt-1980-JAS000971.pdf
 * Tiny Speech Synth demonstrates phoneme targets, noise/voicing and overlap:
 * https://heckmeck.de/demoscene/tiny-speech-synth-js/
 *
 * The rounded targets and timing heuristics below are generic approximations
 * chosen for this toy. They are not measurements of Johnny Cash's voice.
 * An IPA sequence is already aligned to a musical note; no dictionary, reciter,
 * recorded phoneme, or external speech engine is involved.
 */
#include "phoneme.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

enum PhoneClass { VOWEL, STOP, FRICATIVE, NASAL, APPROXIMANT, PAUSE };

struct Target {
    unsigned codepoint;
    enum PhoneClass kind;
    double f[3], bw[3];
    double voiced, noise, fric_freq, fric_width, nasal, length;
};

/* Frequencies/bandwidths in Hz; consonant length is a short nominal duration.
 * Formant targets are deliberately rounded, broadly adult-male-like values. */
static const struct Target targets[] = {
    { 'a',    VOWEL, { 760, 1250, 2600 }, { 90, 120, 180 }, 1, 0, 0, 0, 0, 0 },
    { 0x0251, VOWEL, { 730, 1100, 2450 }, { 90, 110, 170 }, 1, 0, 0, 0, 0, 0 },
    { 0x00e6, VOWEL, { 660, 1750, 2450 }, { 85, 120, 180 }, 1, 0, 0, 0, 0, 0 },
    { 0x025b, VOWEL, { 540, 1850, 2500 }, { 80, 115, 175 }, 1, 0, 0, 0, 0, 0 },
    { 'e',    VOWEL, { 430, 1950, 2550 }, { 75, 110, 170 }, 1, 0, 0, 0, 0, 0 },
    { 0x026a, VOWEL, { 390, 2000, 2600 }, { 75, 110, 170 }, 1, 0, 0, 0, 0, 0 },
    { 'i',    VOWEL, { 280, 2300, 3000 }, { 70, 100, 180 }, 1, 0, 0, 0, 0, 0 },
    { 0x0254, VOWEL, { 570,  850, 2450 }, { 85, 100, 170 }, 1, 0, 0, 0, 0, 0 },
    { 'o',    VOWEL, { 430,  950, 2500 }, { 75, 100, 170 }, 1, 0, 0, 0, 0, 0 },
    { 0x028a, VOWEL, { 440, 1050, 2250 }, { 75, 100, 160 }, 1, 0, 0, 0, 0, 0 },
    { 'u',    VOWEL, { 300,  850, 2250 }, { 70,  95, 160 }, 1, 0, 0, 0, 0, 0 },
    { 0x0259, VOWEL, { 500, 1500, 2500 }, { 85, 120, 180 }, 1, 0, 0, 0, 0, 0 },
    { 0x028c, VOWEL, { 640, 1200, 2400 }, { 90, 120, 180 }, 1, 0, 0, 0, 0, 0 },
    { 0x025c, VOWEL, { 490, 1350, 1750 }, { 85, 110, 180 }, 1, 0, 0, 0, 0, 0 },
    { 'p', STOP, { 180,  850, 2200 }, { 120, 200, 300 }, 0, .20, 2200, 3000, 0, .060 },
    { 'b', STOP, { 180,  850, 2200 }, { 120, 200, 300 }, .16, .15, 2200, 3000, 0, .055 },
    { 't', STOP, { 180, 1800, 2800 }, { 120, 200, 300 }, 0, .23, 5200, 2800, 0, .060 },
    { 'd', STOP, { 180, 1800, 2800 }, { 120, 200, 300 }, .16, .16, 5200, 2800, 0, .055 },
    { 'k', STOP, { 230, 1950, 2450 }, { 130, 220, 300 }, 0, .20, 3300, 2000, 0, .065 },
    { 'g', STOP, { 230, 1950, 2450 }, { 130, 220, 300 }, .16, .16, 3300, 2000, 0, .060 },
    { 0x0261, STOP, { 230, 1950, 2450 }, { 130, 220, 300 }, .16, .16, 3300, 2000, 0, .060 },
    { 0x0294, STOP, { 250, 1400, 2500 }, { 120, 180, 250 }, 0, .015, 0, 0, 0, .045 },
    { 's', FRICATIVE, { 300, 1600, 2600 }, { 160, 250, 400 }, 0, .16, 7000, 2600, 0, .085 },
    { 'z', FRICATIVE, { 300, 1600, 2600 }, { 160, 250, 400 }, .48, .11, 7000, 2600, 0, .080 },
    { 'f', FRICATIVE, { 350, 1500, 2500 }, { 180, 300, 450 }, 0, .09, 3500, 4500, 0, .075 },
    { 'v', FRICATIVE, { 350, 1500, 2500 }, { 180, 300, 450 }, .55, .07, 3500, 4500, 0, .070 },
    { 0x03b8, FRICATIVE, { 350, 1600, 2600 }, { 180, 300, 450 }, 0, .095, 4200, 3500, 0, .080 },
    { 0x00f0, FRICATIVE, { 350, 1600, 2600 }, { 180, 300, 450 }, .55, .065, 4200, 3500, 0, .075 },
    { 0x0283, FRICATIVE, { 350, 1600, 2600 }, { 180, 300, 450 }, 0, .15, 3000, 1900, 0, .085 },
    { 0x0292, FRICATIVE, { 350, 1600, 2600 }, { 180, 300, 450 }, .48, .105, 3000, 1900, 0, .080 },
    { 'h', FRICATIVE, { 500, 1500, 2500 }, { 150, 220, 300 }, 0, .095, 0, 0, 0, .050 },
    { 'm', NASAL, { 270,  900, 2050 }, { 80, 240, 350 }, .82, 0, 0, 0, 1, .060 },
    { 'n', NASAL, { 270, 1450, 2550 }, { 80, 240, 350 }, .82, 0, 0, 0, 1, .060 },
    { 0x014b, NASAL, { 270, 1750, 2400 }, { 80, 240, 350 }, .82, 0, 0, 0, 1, .065 },
    { 'l', APPROXIMANT, { 350, 1100, 2600 }, { 90, 160, 220 }, .88, 0, 0, 0, 0, .050 },
    { 'r', APPROXIMANT, { 330, 1150, 1700 }, { 90, 150, 200 }, .88, 0, 0, 0, 0, .050 },
    { 0x0279, APPROXIMANT, { 330, 1150, 1700 }, { 90, 150, 200 }, .88, 0, 0, 0, 0, .050 },
    { 'w', APPROXIMANT, { 300,  650, 2250 }, { 80, 120, 180 }, .85, 0, 0, 0, 0, .045 },
    { 'j', APPROXIMANT, { 280, 2250, 3000 }, { 80, 120, 180 }, .85, 0, 0, 0, 0, .045 },
    { ' ', PAUSE, { 500, 1500, 2500 }, { 100, 150, 220 }, 0, 0, 0, 0, 0, .008 }
};

static const struct Target *
target_for(unsigned codepoint)
{
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i)
        if (targets[i].codepoint == codepoint)
            return &targets[i];
    return NULL;
}

/* Reject overlong encodings, isolated continuations, surrogates and truncation.
 * Checking each continuation for NUL before advancing also bounds string reads. */
static int
decode_utf8(const unsigned char **cursor, unsigned *codepoint)
{
    const unsigned char *p = *cursor;
    unsigned value, minimum;
    unsigned length;
    if (*p < 0x80) {
        *codepoint = *p;
        *cursor = p + 1;
        return 0;
    }
    if (*p >= 0xc2 && *p <= 0xdf) {
        length = 2; value = *p & 0x1f; minimum = 0x80;
    } else if (*p >= 0xe0 && *p <= 0xef) {
        length = 3; value = *p & 0x0f; minimum = 0x800;
    } else if (*p >= 0xf0 && *p <= 0xf4) {
        length = 4; value = *p & 7; minimum = 0x10000;
    } else {
        errno = EILSEQ;
        return -1;
    }
    for (unsigned i = 1; i < length; ++i) {
        unsigned byte = p[i];
        if (byte == 0 || (byte & 0xc0) != 0x80) {
            errno = EILSEQ;
            return -1;
        }
        value = (value << 6) | (byte & 0x3f);
    }
    if (value < minimum || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) {
        errno = EILSEQ;
        return -1;
    }
    *codepoint = value;
    *cursor = p + length;
    return 0;
}

static struct PhoneState
state_for(const struct Target *target)
{
    struct PhoneState state;
    for (int i = 0; i < 3; ++i) {
        state.formant[i] = target->f[i];
        state.bandwidth[i] = target->bw[i];
    }
    state.voiced = target->voiced;
    state.noise = target->noise;
    state.fric_freq = target->fric_freq;
    state.fric_width = target->fric_width;
    state.nasal = target->nasal;
    return state;
}

int
phoneme_plan(const char *utf8, double seconds, struct PhoneSequence *out)
{
    struct PhoneSequence sequence = {0};
    const struct Target *phonetic[96];
    double weights[96], consonants = 0.0, vowels = 0.0;
    const unsigned char *cursor = (const unsigned char *)utf8;
    double pending_stress = 1.0;
    if (out == NULL || utf8 == NULL || !isfinite(seconds) || seconds <= 0.0) {
        errno = EINVAL;
        return -1;
    }
    memset(out, 0, sizeof(*out));
    while (*cursor != 0) {
        unsigned cp;
        const struct Target *target;
        if (decode_utf8(&cursor, &cp) != 0)
            return -1;
        if (cp == 0x02c8 || cp == 0x02cc) {
            pending_stress = cp == 0x02c8 ? 1.15 : 1.08;
            continue;
        }
        if (cp == 0x02d0 || cp == 0x02d1 || cp == ':') {
            if (sequence.count && phonetic[sequence.count - 1]->kind == VOWEL)
                weights[sequence.count - 1] *= cp == 0x02d1 ? 1.35 : 1.7;
            continue;
        }
        /* Syllable/tie markers and punctuation do not add a spoken segment. */
        if (cp == '.' || cp == ',' || cp == '-' || cp == '\'' ||
            cp == '(' || cp == ')' || cp == '[' || cp == ']' ||
            cp == '/' || cp == 0x0361 || cp == 0x035c || cp == 0x2019)
            continue;
        if (cp == '\t' || cp == '\r' || cp == '\n' || cp == 0x00a0)
            cp = ' ';
        target = target_for(cp);
        if (target == NULL) {
            errno = EINVAL;
            return -1;
        }
        if (target->kind == PAUSE && (sequence.count == 0 ||
            phonetic[sequence.count - 1]->kind == PAUSE))
            continue;
        if (sequence.count == sizeof(sequence.tokens) / sizeof(sequence.tokens[0])) {
            errno = E2BIG;
            return -1;
        }
        size_t i = sequence.count++;
        sequence.tokens[i].codepoint = cp;
        sequence.tokens[i].state = state_for(target);
        phonetic[i] = target;
        weights[i] = target->kind == VOWEL ? pending_stress : target->length;
        if (target->kind == VOWEL)
            pending_stress = 1.0;
    }
    if (sequence.count && phonetic[sequence.count - 1]->kind == PAUSE)
        --sequence.count;
    if (sequence.count == 0) {
        errno = EINVAL;
        return -1;
    }
    /* A sustained first vowel and a brief second vowel make singing diphthongs
     * recognizable while preserving the exact scored note length. */
    for (size_t i = 0; i < sequence.count; ++i) {
        if (phonetic[i]->kind == VOWEL && i + 1 < sequence.count &&
            phonetic[i + 1]->kind == VOWEL &&
            (i == 0 || phonetic[i - 1]->kind != VOWEL))
            weights[i] *= 3.0;
        if (phonetic[i]->kind == VOWEL)
            vowels += weights[i];
        else
            consonants += weights[i];
        /* Aspiration is filtered like its neighboring vowel, rather than
         * imposing one fixed vowel color on every /h/ onset. */
        if (sequence.tokens[i].codepoint == 'h') {
            const struct Target *neighbor = NULL;
            for (size_t j = i + 1; j < sequence.count; ++j) {
                if (phonetic[j]->kind == VOWEL) { neighbor = phonetic[j]; break; }
            }
            if (neighbor == NULL) {
                for (size_t j = i; j > 0; --j) {
                    if (phonetic[j - 1]->kind == VOWEL) {
                        neighbor = phonetic[j - 1]; break;
                    }
                }
            }
            if (neighbor != NULL) {
                for (int j = 0; j < 3; ++j) {
                    sequence.tokens[i].state.formant[j] = neighbor->f[j];
                    sequence.tokens[i].state.bandwidth[j] = neighbor->bw[j] * 1.6;
                }
            }
        }
    }
    double consonant_budget = fmin(consonants, seconds * 0.40);
    double consonant_scale = consonants > 0.0 ? consonant_budget / consonants : 0.0;
    double vowel_scale = vowels > 0.0 ? (seconds - consonant_budget) / vowels : 0.0;
    double time = 0.0;
    for (size_t i = 0; i < sequence.count; ++i) {
        double length = weights[i] *
            (phonetic[i]->kind == VOWEL ? vowel_scale : consonant_scale);
        sequence.tokens[i].start = time;
        time += length;
        sequence.tokens[i].end = time;
    }
    /* Coda consonants follow the held vowel and are therefore reserved before
     * the note boundary. Remove the tiny accumulated floating-point error. */
    if (vowels > 0.0)
        sequence.tokens[sequence.count - 1].end = seconds;
    sequence.duration = seconds;
    *out = sequence;
    return 0;
}

static double
smooth(double value)
{
    if (value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value * value * (3.0 - 2.0 * value);
}

static struct PhoneState
token_state(const struct PhoneToken *token, double time)
{
    struct PhoneState state = token->state;
    const struct Target *target = target_for(token->codepoint);
    double length = token->end - token->start;
    double phase = length > 0.0 ? (time - token->start) / length : 1.0;
    if (phase < 0.0) phase = 0.0;
    if (phase > 1.0) phase = 1.0;
    if (target != NULL && target->kind == STOP) {
        /* Quiet closure, brief release burst, then a little aspiration. A
         * Gaussian burst is generated from noise, never a recorded stop. */
        double burst = exp(-0.5 * pow((phase - 0.67) / 0.065, 2.0));
        double aspiration = phase > 0.72 ? 0.17 * (1.0 - phase) / 0.28 : 0.0;
        state.noise *= burst + aspiration;
        state.voiced *= 1.0 - smooth((phase - 0.50) / 0.12);
    } else if (target != NULL && target->kind == FRICATIVE && length > 0.0) {
        /* Airflow grows and dies away instead of switching on a rectangular
         * block of white noise. Retain a steady middle for place-of-articulation
         * cues; shorten these edges along with very short musical syllables. */
        double attack = fmin(0.009, length * 0.22);
        double release = fmin(0.014, length * 0.28);
        state.noise *= smooth(phase * length / attack) *
            smooth((1.0 - phase) * length / release);
    }
    return state;
}

static void
blend_state(struct PhoneState *out, const struct PhoneState *a,
    const struct PhoneState *b, double weight)
{
    for (int i = 0; i < 3; ++i) {
        out->formant[i] = a->formant[i] + (b->formant[i] - a->formant[i]) * weight;
        out->bandwidth[i] = a->bandwidth[i] +
            (b->bandwidth[i] - a->bandwidth[i]) * weight;
    }
    out->voiced = a->voiced + (b->voiced - a->voiced) * weight;
    out->noise = a->noise + (b->noise - a->noise) * weight;
    /* A vowel has no turbulent band, rather than a band centered at 0 Hz.
     * Hold a single active band's color while its level fades. Interpolating
     * its center down to zero caused an artificial falling noise sweep. */
    if (a->fric_freq > 0.0 && b->fric_freq <= 0.0) {
        out->fric_freq = a->fric_freq;
        out->fric_width = a->fric_width;
    } else if (b->fric_freq > 0.0 && a->fric_freq <= 0.0) {
        out->fric_freq = b->fric_freq;
        out->fric_width = b->fric_width;
    } else {
        out->fric_freq = a->fric_freq + (b->fric_freq - a->fric_freq) * weight;
        out->fric_width = a->fric_width + (b->fric_width - a->fric_width) * weight;
    }
    out->nasal = a->nasal + (b->nasal - a->nasal) * weight;
}

void
phoneme_at(const struct PhoneSequence *sequence, double time,
    struct PhoneState *out)
{
    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));
    /* Silence keeps damped filter targets. Zero bandwidths would put poles on
     * the unit circle if a caller updates its resonators after note-off. */
    out->formant[0] = 500;
    out->formant[1] = 1500;
    out->formant[2] = 2500;
    out->bandwidth[0] = 100;
    out->bandwidth[1] = 150;
    out->bandwidth[2] = 220;
    if (sequence != NULL && sequence->count > 0 && sequence->count <= 96 &&
        isfinite(time) && time >= sequence->duration) {
        const struct PhoneState *last = &sequence->tokens[sequence->count - 1].state;
        for (int i = 0; i < 3; ++i) {
            out->formant[i] = last->formant[i];
            out->bandwidth[i] = last->bandwidth[i];
        }
    }
    if (sequence == NULL || sequence->count == 0 || sequence->count > 96 ||
        !isfinite(time) || time < 0.0 || time >= sequence->duration)
        return;
    size_t current = 0;
    while (current < sequence->count && time >= sequence->tokens[current].end)
        ++current;
    if (current == sequence->count)
        return;
    *out = token_state(&sequence->tokens[current], time);
    /* A boundary-centered 20 ms crossfade preserves continuous formants and
     * noise gain. Very short phones use a correspondingly shorter transition. */
    for (size_t i = current ? current : 1;
         i <= current + 1 && i < sequence->count; ++i) {
        const struct PhoneToken *a = &sequence->tokens[i - 1];
        const struct PhoneToken *b = &sequence->tokens[i];
        double half = fmin(0.010, fmin(a->end - a->start,
            b->end - b->start) * 0.25);
        double boundary = b->start;
        if (half > 0.0 && time >= boundary - half && time <= boundary + half) {
            struct PhoneState left = token_state(a, fmin(time, a->end));
            struct PhoneState right = token_state(b, fmax(time, b->start));
            blend_state(out, &left, &right,
                smooth((time - boundary + half) / (2.0 * half)));
            break;
        }
    }
    const struct PhoneToken *last = &sequence->tokens[sequence->count - 1];
    double fade = fmin(0.008, (last->end - last->start) * 0.2);
    if (fade > 0.0 && time > last->end - fade) {
        double gain = smooth((last->end - time) / fade);
        out->voiced *= gain;
        out->noise *= gain;
    }
}
