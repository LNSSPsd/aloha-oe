/*
 * Aloha 'Oe, melody transcribed from the upper vocal line in:
 *   H. M. Queen Liliuokalani / J. Bodewalt Lampe (arranger),
 *   Jerome H. Remick & Co., 1912, score pages 2-3.
 * Public-domain scan (IMSLP 786918):
 * https://s9.imslp.org/files/imglnks/usimg/3/38/IMSLP786918-PMLP58215-Liliuokalani-Aloha_Oe.pdf
 * Cross-check: W. A. Quincke & Co. 1912, IMSLP 26121, pages 2-3:
 * https://s9.imslp.org/files/imglnks/usimg/9/9f/IMSLP26121-PMLP58215-Liliuokalani_AlohaOe.pdf
 * Independent machine-readable cross-check: OpenScore Lieder lc6650166.mscx,
 * transcribed from IMSLP 26121. All pitches and durations match after merging
 * repeated tied pitches; Lampe's last dotted-half/fermata differs from
 * Quincke's half note plus quarter rest. The latter two occupy the same time.
 * https://github.com/OpenScore/Lieder/tree/main/scores/Liliuokalani%2C_Queen_of_the_Hawaiian_Islands/_/Aloha_Oe
 *
 * Source key: A-flat major; common time (4/4); "Moderato".
 * The source does NOT specify a numerical metronome marking.
 * MIDI pitches describe the printed treble vocal register, not Cash's register.
 * One tick is a sixteenth note: 4 ticks = one notated quarter note.
 * MIDI 0 denotes silence. Same-pitch ties are merged; repeated syllabic
 * notes remain separate. The optional turn over verse bar 3 is omitted.
 * Neither original recordings nor recorded instrument samples are embedded.
 *
 * Pickups are part of each array. Verse: 1 quarter; chorus: 1 eighth.
 * The last verse bar has 3 quarters of A-flat, an eighth rest, then the
 * chorus pickup completes that bar. The final chorus note is a dotted half
 * under a fermata. A caller may add a cadence hold or silence after it.
 * Array lengths: verse 130 ticks, chorus 126 ticks, total 256 ticks.
 */
#ifndef ALOHA_MELODY_H
#define ALOHA_MELODY_H

#include <stdint.h>

typedef struct {
    uint8_t midi;
    uint8_t ticks;
} MelodyNote;

enum {
    ALOHA_PPQ = 4,
    ALOHA_SOURCE_TONIC = 68, /* A-flat 4 */
    ALOHA_VERSE_PICKUP_TICKS = 4,
    ALOHA_CHORUS_PICKUP_TICKS = 2,
    ALOHA_VERSE_TICKS = 130,
    ALOHA_CHORUS_TICKS = 126
};

static const MelodyNote aloha_verse[] = {
    /* Pickup: E-flat 4, A-flat 4; two eighths. */
    {63, 2}, {68, 2},
    /* Bar 1: C5 dotted quarter; Bb4 eighth; Ab4 dotted eighth;
       G4 sixteenth; Ab4 eighth; F4 eighth. */
    {72, 6}, {70, 2}, {68, 3}, {67, 1}, {68, 2}, {65, 2},
    /* Bar 2: E-flat half tied to quarter, C5 quarter. */
    {63, 12}, {72, 4},
    /* Bar 3: B-flat dotted quarter, A-natural eighth, then four eighths.
       A-natural is a printed chromatic note, not A-flat. */
    {70, 6}, {69, 2}, {70, 2}, {70, 2}, {73, 2}, {72, 2},
    /* Bar 4: B-flat half tied to quarter, E-flat/ A-flat eighths. */
    {70, 12}, {63, 2}, {68, 2},
    /* Bar 5: repeats the melody and dotted rhythm of bar 1. */
    {72, 6}, {70, 2}, {68, 3}, {67, 1}, {68, 2}, {65, 2},
    /* Bar 6: E-flat half tied to quarter, A-flat/ G eighths. */
    {63, 12}, {68, 2}, {67, 2},
    /* Bar 7: F quarter, then six eighths. */
    {65, 4}, {70, 2}, {68, 2}, {67, 2}, {67, 2}, {72, 2}, {70, 2},
    /* Bar 8: A-flat half tied to quarter, eighth rest.
       The following chorus pickup occupies the remaining eighth. */
    {68, 12}, {0, 2}
};

static const MelodyNote aloha_chorus[] = {
    /* Pickup: E-flat 4 eighth. */
    {63, 2},
    /* Bar 1: F quarter, A-flat quarter, D-flat dotted quarter, F eighth. */
    {65, 4}, {68, 4}, {73, 6}, {65, 2},
    /* Bar 2: E-flat quarter, A-flat quarter, C5 dotted quarter,
       A-flat sixteenth, A-flat sixteenth (separate attacks). */
    {63, 4}, {68, 4}, {72, 6}, {68, 1}, {68, 1},
    /* Bar 3: G dotted eighth, F sixteenth, then six eighths. */
    {67, 3}, {65, 1}, {67, 2}, {68, 2},
    {70, 2}, {70, 2}, {73, 2}, {73, 2},
    /* Bar 4: C5 half, A-flat dotted quarter, E-flat eighth pickup. */
    {72, 8}, {68, 6}, {63, 2},
    /* Bar 5: same melody as bar 1. */
    {65, 4}, {68, 4}, {73, 6}, {65, 2},
    /* Bar 6: four eighths, C5 dotted quarter, A-flat eighth. */
    {63, 2}, {63, 2}, {68, 2}, {68, 2}, {72, 6}, {68, 2},
    /* Bar 7: G dotted quarter, A-flat eighth, C5 eighth,
       B-flat quarter, G eighth (leading tone before the final A-flat). */
    {67, 6}, {68, 2}, {72, 2}, {70, 4}, {67, 2},
    /* Bar 8: A-flat dotted half under a fermata; no numerical hold
       duration is specified in the printed source. */
    {68, 12}
};

#define ALOHA_VERSE_NOTE_COUNT (sizeof aloha_verse / sizeof aloha_verse[0])
#define ALOHA_CHORUS_NOTE_COUNT (sizeof aloha_chorus / sizeof aloha_chorus[0])

#endif
