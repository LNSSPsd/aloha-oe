#include "options.h"

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    OPT_SECONDS = 256,
    OPT_OFFSET,
    OPT_DRY,
    OPT_LEAD_ONLY,
    OPT_NO_LYRICS,
    OPT_VOICE_STYLE,
    OPT_IPA,
    OPT_GUITAR_GAIN,
    OPT_BASS_GAIN,
    OPT_STEEL_GAIN,
    OPT_PIANO_GAIN,
    OPT_PERCUSSION_GAIN,
    OPT_VOCAL_GAIN,
    OPT_STEMS,
    OPT_SCORE,
    OPT_CHECK
};

static const struct option long_options[] = {
    { "help",            no_argument,       NULL, 'h' },
    { "wav",             required_argument, NULL, 'o' },
    { "no-play",         no_argument,       NULL, 'n' },
    { "section",         required_argument, NULL, 's' },
    { "bpm",             required_argument, NULL, 'b' },
    { "transpose",       required_argument, NULL, 't' },
    { "volume",          required_argument, NULL, 'v' },
    { "rate",            required_argument, NULL, 'r' },
    { "device",          required_argument, NULL, 'd' },
    { "mode",            required_argument, NULL, 'm' },
    { "voice",           no_argument,       NULL, 'V' },
    { "ensemble",        no_argument,       NULL, 'e' },
    { "lyrics",          no_argument,       NULL, 'l' },
    { "seconds",         required_argument, NULL, OPT_SECONDS },
    { "offset",          required_argument, NULL, OPT_OFFSET },
    { "dry",             no_argument,       NULL, OPT_DRY },
    { "lead-only",       no_argument,       NULL, OPT_LEAD_ONLY },
    { "no-lyrics",       no_argument,       NULL, OPT_NO_LYRICS },
    { "voice-style",     required_argument, NULL, OPT_VOICE_STYLE },
    { "ipa",             required_argument, NULL, OPT_IPA },
    { "guitar-gain",      required_argument, NULL, OPT_GUITAR_GAIN },
    { "bass-gain",        required_argument, NULL, OPT_BASS_GAIN },
    { "steel-gain",       required_argument, NULL, OPT_STEEL_GAIN },
    { "piano-gain",       required_argument, NULL, OPT_PIANO_GAIN },
    { "percussion-gain",  required_argument, NULL, OPT_PERCUSSION_GAIN },
    { "vocal-gain",       required_argument, NULL, OPT_VOCAL_GAIN },
    { "stems",           required_argument, NULL, OPT_STEMS },
    { "score",           required_argument, NULL, OPT_SCORE },
    { "check",           no_argument,       NULL, OPT_CHECK },
    { NULL,              0,                 NULL, 0 }
};

static const struct Options defaults = {
    .bpm = 49,
    .volume = 0.85,
    .part_gain = { 1, 1, 1, 0, 0, 1 },
    .rate = 48000,
    .play = 1,
    .wet = 1,
    .lyrics = -1,
    .mode = MODE_INSTRUMENTAL,
    .voice_style = VOICE_BARITONE,
    .section = SECTION_FULL
};

static void
usage(FILE *out)
{
    fputs("aloha-oe: an entirely synthesized Aloha 'Oe\n"
          "Usage: aloha-oe [options]\n"
          "  -o, --wav FILE          export stereo 16-bit WAV\n"
          "  -n, --no-play           render/export without opening a sound device\n"
          "  -s, --section NAME      full (default), verse, or chorus\n"
          "  -b, --bpm N             quarter-note tempo, 25..150 (default 49; eighth 98)\n"
          "  -t, --transpose N       semitones relative to E major (-12..12)\n"
          "  -v, --volume N          master peak amplitude, 0..0.9 (default 0.85)\n"
          "  -r, --rate N            22050, 44100, or 48000 (default) samples/second\n"
          "      --seconds N         preview N seconds, with a gentle ending\n"
          "      --offset N          start preview N seconds into the selected section\n"
          "      --dry               disable the generated room reverb\n"
          "      --lead-only         play/export only the score melody\n"
          "  -m, --mode NAME         instrumental (default), vocal, or ensemble\n"
          "  -V, --voice             alias for --mode vocal\n"
          "  -e, --ensemble          alias for --mode ensemble\n"
          "  -l, --lyrics            stream words during playback (auto with vocals)\n"
          "      --no-lyrics         disable terminal lyric output\n"
          "      --voice-style NAME  baritone (default) or retro (11kHz/7-bit)\n"
          "      --ipa TEXT          audition a UTF-8 IPA phrase on E3 (default 6 s)\n"
          "      --guitar-gain N     guitar level, 0..2 (default 1)\n"
          "      --bass-gain N       bass level, 0..2 (default 1)\n"
          "      --steel-gain N      steel melody level, 0..2 (default 1)\n"
          "      --piano-gain N      optional piano level, 0..2 (default 0)\n"
          "      --percussion-gain N optional percussion level, 0..2 (default 0)\n"
          "      --vocal-gain N      synthetic vocal level, 0..2 (default 1)\n"
          "      --stems DIRECTORY   export six tracks with one shared master gain\n"
          "      --score FILE        export scheduled note events as CSV\n"
          "  -d, --device NAME       select the native PCM output device\n"
          "      --check             validate score, tuning, timing, and signal bounds\n"
          "  -h, --help              show this help\n", out);
}

static int
number(const char *text, double low, double high, double *out)
{
    char *end;
    double value;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value) ||
        value < low || value > high)
        return -1;
    *out = value;
    return 0;
}

static const char *
option_name(int option)
{
    const struct option *entry;

    for (entry = long_options; entry->name != NULL; ++entry) {
        if (entry->val == option)
            return entry->name;
    }
    return "?";
}

enum OptionsResult
options_parse(int argc, char **argv, struct Options *opts)
{
    int option;
    double value;

    *opts = defaults;
    opterr = 0;
    while ((option = getopt_long(argc, argv, ":ho:ns:b:t:v:r:d:m:Vel",
        long_options, NULL)) != -1) {
        switch (option) {
        case 'h':
            usage(stdout);
            return OPTIONS_HELP;
        case 'o':
            opts->wav = optarg;
            break;
        case 'n':
            opts->play = 0;
            break;
        case 's':
            if (strcmp(optarg, "full") == 0)
                opts->section = SECTION_FULL;
            else if (strcmp(optarg, "verse") == 0)
                opts->section = SECTION_VERSE;
            else if (strcmp(optarg, "chorus") == 0)
                opts->section = SECTION_CHORUS;
            else
                goto invalid_value;
            break;
        case 'b':
            if (number(optarg, 25, 150, &opts->bpm) != 0)
                goto invalid_value;
            break;
        case 't':
            if (number(optarg, -12, 12, &value) != 0 || value != floor(value))
                goto invalid_value;
            opts->transpose = (int)value;
            break;
        case 'v':
            if (number(optarg, 0, 0.9, &opts->volume) != 0)
                goto invalid_value;
            break;
        case 'r':
            if (number(optarg, 22050, 48000, &value) != 0 ||
                (value != 22050 && value != 44100 && value != 48000))
                goto invalid_value;
            opts->rate = (unsigned)value;
            break;
        case 'd':
            opts->device = optarg;
            break;
        case 'm':
            if (strcmp(optarg, "instrumental") == 0)
                opts->mode = MODE_INSTRUMENTAL;
            else if (strcmp(optarg, "vocal") == 0)
                opts->mode = MODE_VOCAL;
            else if (strcmp(optarg, "ensemble") == 0)
                opts->mode = MODE_ENSEMBLE;
            else
                goto invalid_value;
            break;
        case 'V':
            opts->mode = MODE_VOCAL;
            break;
        case 'e':
            opts->mode = MODE_ENSEMBLE;
            break;
        case 'l':
            opts->lyrics = 1;
            break;
        case OPT_SECONDS:
            if (number(optarg, 0.05, 1200, &opts->seconds) != 0)
                goto invalid_value;
            break;
        case OPT_OFFSET:
            if (number(optarg, 0, 1200, &opts->offset) != 0)
                goto invalid_value;
            break;
        case OPT_DRY:
            opts->wet = 0;
            break;
        case OPT_LEAD_ONLY:
            opts->lead_only = 1;
            break;
        case OPT_NO_LYRICS:
            opts->lyrics = 0;
            break;
        case OPT_VOICE_STYLE:
            if (strcmp(optarg, "baritone") == 0)
                opts->voice_style = VOICE_BARITONE;
            else if (strcmp(optarg, "retro") == 0)
                opts->voice_style = VOICE_RETRO;
            else
                goto invalid_value;
            break;
        case OPT_IPA:
            if (*optarg == '\0')
                goto invalid_value;
            opts->ipa = optarg;
            break;
        case OPT_GUITAR_GAIN:
        case OPT_BASS_GAIN:
        case OPT_STEEL_GAIN:
        case OPT_PIANO_GAIN:
        case OPT_PERCUSSION_GAIN:
        case OPT_VOCAL_GAIN:
            if (number(optarg, 0, 2,
                &opts->part_gain[option - OPT_GUITAR_GAIN]) != 0)
                goto invalid_value;
            break;
        case OPT_STEMS:
            if (*optarg == '\0')
                goto invalid_value;
            opts->stems = optarg;
            break;
        case OPT_SCORE:
            opts->csv = optarg;
            break;
        case OPT_CHECK:
            opts->check = 1;
            break;
        case ':':
            fprintf(stderr, "aloha-oe: option requires an argument: %s\n",
                argv[optind - 1]);
            goto error;
        case '?':
        default:
            fprintf(stderr, "aloha-oe: unknown or ambiguous option: %s\n",
                argv[optind - 1]);
            goto error;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "aloha-oe: unexpected argument: %s\n", argv[optind]);
        goto error;
    }
    return OPTIONS_OK;

invalid_value:
    fprintf(stderr, "aloha-oe: invalid value for --%s: %s\n",
        option_name(option), optarg);
error:
    fputs("Try 'aloha-oe --help' for usage.\n", stderr);
    return OPTIONS_ERROR;
}
