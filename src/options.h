#ifndef ALOHA_OPTIONS_H
#define ALOHA_OPTIONS_H

#include "score.h"

enum Part {
    PART_GUITAR, PART_BASS, PART_STEEL, PART_PIANO,
    PART_PERCUSSION, PART_VOCAL, PART_COUNT
};

struct Options {
    const char *wav;
    const char *csv;
    const char *device;
    const char *stems;
    const char *ipa;
    double bpm;
    double volume;
    double seconds;
    double offset;
    double part_gain[PART_COUNT];
    unsigned rate;
    int transpose;
    int play;
    int wet;
    int lead_only;
    int check;
    int lyrics;                 /* -1: automatic, 0: off, 1: on */
    enum PlayMode mode;
    enum VoiceStyle voice_style;
    enum Section section;
};

enum OptionsResult {
    OPTIONS_OK,
    OPTIONS_HELP,
    OPTIONS_ERROR
};

/* Parse once at program startup. Strings refer to argv storage. */
enum OptionsResult options_parse(int, char **, struct Options *);

#endif
