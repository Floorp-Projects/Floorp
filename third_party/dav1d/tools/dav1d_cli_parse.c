/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "dav1d_cli_parse.h"
#include "src/cpu.h"

static const char short_opts[] = "i:o:vql:s:";

enum {
    ARG_DEMUXER = 256,
    ARG_MUXER,
    ARG_FRAME_TIMES,
    ARG_REALTIME,
    ARG_REALTIME_CACHE,
    ARG_FRAME_THREADS,
    ARG_TILE_THREADS,
    ARG_VERIFY,
    ARG_FILM_GRAIN,
    ARG_OPPOINT,
    ARG_ALL_LAYERS,
    ARG_SIZE_LIMIT,
    ARG_CPU_MASK,
};

static const struct option long_opts[] = {
    { "input",          1, NULL, 'i' },
    { "output",         1, NULL, 'o' },
    { "quiet",          0, NULL, 'q' },
    { "demuxer",        1, NULL, ARG_DEMUXER },
    { "muxer",          1, NULL, ARG_MUXER },
    { "version",        0, NULL, 'v' },
    { "frametimes",     1, NULL, ARG_FRAME_TIMES },
    { "limit",          1, NULL, 'l' },
    { "skip",           1, NULL, 's' },
    { "realtime",       2, NULL, ARG_REALTIME },
    { "realtimecache",  1, NULL, ARG_REALTIME_CACHE },
    { "framethreads",   1, NULL, ARG_FRAME_THREADS },
    { "tilethreads",    1, NULL, ARG_TILE_THREADS },
    { "verify",         1, NULL, ARG_VERIFY },
    { "filmgrain",      1, NULL, ARG_FILM_GRAIN },
    { "oppoint",        1, NULL, ARG_OPPOINT },
    { "alllayers",      1, NULL, ARG_ALL_LAYERS },
    { "sizelimit",      1, NULL, ARG_SIZE_LIMIT },
    { "cpumask",        1, NULL, ARG_CPU_MASK },
    { NULL,             0, NULL, 0 },
};

#if ARCH_AARCH64 || ARCH_ARM
#define ALLOWED_CPU_MASKS " or 'neon'"
#elif ARCH_X86
#define ALLOWED_CPU_MASKS \
    ", 'sse2', 'ssse3', 'sse41', 'avx2', 'avx512' or 'avx512icl'"
#else
#define ALLOWED_CPU_MASKS "not yet implemented for this architecture"
#endif

static void usage(const char *const app, const char *const reason, ...) {
    if (reason) {
        va_list args;

        va_start(args, reason);
        vfprintf(stderr, reason, args);
        va_end(args);
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "Usage: %s [options]\n\n", app);
    fprintf(stderr, "Supported options:\n"
            " --input/-i  $file:    input file\n"
            " --output/-o $file:    output file\n"
            " --demuxer $name:      force demuxer type ('ivf', 'section5' or 'annexb'; default: detect from extension)\n"
            " --muxer $name:        force muxer type ('md5', 'yuv', 'yuv4mpeg2' or 'null'; default: detect from extension)\n"
            " --quiet/-q:           disable status messages\n"
            " --frametimes $file:   dump frame times to file\n"
            " --limit/-l $num:      stop decoding after $num frames\n"
            " --skip/-s $num:       skip decoding of the first $num frames\n"
            " --realtime [$fract]:  limit framerate, optional argument to override input framerate\n"
            " --realtimecache $num: set the size of the cache in realtime mode (default: 0)\n"
            " --version/-v:         print version and exit\n"
            " --framethreads $num:  number of frame threads (default: 1)\n"
            " --tilethreads $num:   number of tile threads (default: 1)\n"
            " --filmgrain $num:     enable film grain application (default: 1, except if muxer is md5)\n"
            " --oppoint $num:       select an operating point of a scalable AV1 bitstream (0 - 32)\n"
            " --alllayers $num:     output all spatial layers of a scalable AV1 bitstream (default: 1)\n"
            " --sizelimit $num:     stop decoding if the frame size exceeds the specified limit\n"
            " --verify $md5:        verify decoded md5. implies --muxer md5, no output\n"
            " --cpumask $mask:      restrict permitted CPU instruction sets (0" ALLOWED_CPU_MASKS "; default: -1)\n");
    exit(1);
}

static void error(const char *const app, const char *const optarg,
                  const int option, const char *const shouldbe)
{
    char optname[256];
    int n;

    for (n = 0; long_opts[n].name; n++)
        if (long_opts[n].val == option)
            break;
    assert(long_opts[n].name);
    if (long_opts[n].val < 256) {
        sprintf(optname, "-%c/--%s", long_opts[n].val, long_opts[n].name);
    } else {
        sprintf(optname, "--%s", long_opts[n].name);
    }

    usage(app, "Invalid argument \"%s\" for option %s; should be %s",
          optarg, optname, shouldbe);
}

static unsigned parse_unsigned(const char *const optarg, const int option,
                               const char *const app)
{
    char *end;
    const unsigned res = (unsigned) strtoul(optarg, &end, 0);
    if (*end || end == optarg) error(app, optarg, option, "an integer");
    return res;
}

static int parse_optional_fraction(const char *const optarg, const int option,
                                   const char *const app, double *value)
{
    if (optarg == NULL) return 0;
    char *end;
    *value = strtod(optarg, &end);
    if (*end == '/' && end != optarg) {
        const char *optarg2 = end + 1;
        *value /= strtod(optarg2, &end);
        if (*end || end == optarg2) error(app, optarg, option, "a fraction");
    } else if (*end || end == optarg) {
        error(app, optarg, option, "a fraction");
    }
    return 1;
}

typedef struct EnumParseTable {
    const char *str;
    const int val;
} EnumParseTable;

#if ARCH_X86
enum CpuMask {
    X86_CPU_MASK_SSE2      = DAV1D_X86_CPU_FLAG_SSE2,
    X86_CPU_MASK_SSSE3     = DAV1D_X86_CPU_FLAG_SSSE3     | X86_CPU_MASK_SSE2,
    X86_CPU_MASK_SSE41     = DAV1D_X86_CPU_FLAG_SSE41     | X86_CPU_MASK_SSSE3,
    X86_CPU_MASK_AVX2      = DAV1D_X86_CPU_FLAG_AVX2      | X86_CPU_MASK_SSE41,
    X86_CPU_MASK_AVX512ICL = DAV1D_X86_CPU_FLAG_AVX512ICL | X86_CPU_MASK_AVX2,
};
#endif

static const EnumParseTable cpu_mask_tbl[] = {
#if ARCH_AARCH64 || ARCH_ARM
    { "neon", DAV1D_ARM_CPU_FLAG_NEON },
#elif ARCH_X86
    { "sse2",      X86_CPU_MASK_SSE2 },
    { "ssse3",     X86_CPU_MASK_SSSE3 },
    { "sse41",     X86_CPU_MASK_SSE41 },
    { "avx2",      X86_CPU_MASK_AVX2 },
    { "avx512icl", X86_CPU_MASK_AVX512ICL },
#endif
    { 0 },
};

static unsigned parse_enum(char *optarg, const EnumParseTable *const tbl,
                           const int option, const char *app)
{
    char str[1024];

    strcpy(str, "any of ");
    for (int n = 0; tbl[n].str; n++) {
        if (!strcmp(tbl[n].str, optarg))
            return tbl[n].val;

        if (n) {
            if (!tbl[n + 1].str)
                strcat(str, " or ");
            else
                strcat(str, ", ");
        }
        strcat(str, tbl[n].str);
    }

    char *end;
    unsigned res;
    if (!strncmp(optarg, "0x", 2)) {
        res = (unsigned) strtoul(&optarg[2], &end, 16);
    } else {
        res = (unsigned) strtoul(optarg, &end, 0);
    }

    if (*end || end == optarg) {
        strcat(str, ", a hexadecimal (starting with 0x), or an integer");
        error(app, optarg, option, str);
    }

    return res;
}

void parse(const int argc, char *const *const argv,
           CLISettings *const cli_settings, Dav1dSettings *const lib_settings)
{
    int o;

    memset(cli_settings, 0, sizeof(*cli_settings));
    dav1d_default_settings(lib_settings);
    int grain_specified = 0;

    while ((o = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (o) {
        case 'o':
            cli_settings->outputfile = optarg;
            break;
        case 'i':
            cli_settings->inputfile = optarg;
            break;
        case 'q':
            cli_settings->quiet = 1;
            break;
        case 'l':
            cli_settings->limit = parse_unsigned(optarg, 'l', argv[0]);
            break;
        case 's':
            cli_settings->skip = parse_unsigned(optarg, 's', argv[0]);
            break;
        case ARG_DEMUXER:
            cli_settings->demuxer = optarg;
            break;
        case ARG_MUXER:
            cli_settings->muxer = optarg;
            break;
        case ARG_FRAME_TIMES:
            cli_settings->frametimes = optarg;
            break;
        case ARG_REALTIME:
            // workaround to parse an optional argument of the form `--a b`
            // (getopt only allows `--a=b`)
            if (optarg == NULL && optind < argc && argv[optind] != NULL &&
                argv[optind][0] != '-')
            {
                optarg = argv[optind];
                optind++;
            }
            cli_settings->realtime = 1 + parse_optional_fraction(optarg,
                ARG_REALTIME, argv[0], &cli_settings->realtime_fps);
            break;
        case ARG_REALTIME_CACHE:
            cli_settings->realtime_cache =
                parse_unsigned(optarg, ARG_REALTIME_CACHE, argv[0]);
            break;
        case ARG_FRAME_THREADS:
            lib_settings->n_frame_threads =
                parse_unsigned(optarg, ARG_FRAME_THREADS, argv[0]);
            break;
        case ARG_TILE_THREADS:
            lib_settings->n_tile_threads =
                parse_unsigned(optarg, ARG_TILE_THREADS, argv[0]);
            break;
        case ARG_VERIFY:
            cli_settings->verify = optarg;
            break;
        case ARG_FILM_GRAIN:
            lib_settings->apply_grain =
                !!parse_unsigned(optarg, ARG_FILM_GRAIN, argv[0]);
            grain_specified = 1;
            break;
        case ARG_OPPOINT:
            lib_settings->operating_point =
                parse_unsigned(optarg, ARG_OPPOINT, argv[0]);
            break;
        case ARG_ALL_LAYERS:
            lib_settings->all_layers =
                !!parse_unsigned(optarg, ARG_ALL_LAYERS, argv[0]);
            break;
        case ARG_SIZE_LIMIT: {
            char *arg = optarg, *end;
            uint64_t res = strtoul(arg, &end, 0);
            if (*end == 'x') // NxM
                res *= strtoul((arg = end + 1), &end, 0);
            if (*end || end == arg || res >= UINT_MAX)
                error(argv[0], optarg, ARG_SIZE_LIMIT, "an integer or dimension");
            lib_settings->frame_size_limit = (unsigned) res;
            break;
        }
        case 'v':
            fprintf(stderr, "%s\n", dav1d_version());
            exit(0);
        case ARG_CPU_MASK:
            dav1d_set_cpu_flags_mask(parse_enum(optarg, cpu_mask_tbl,
                                                ARG_CPU_MASK, argv[0]));
            break;
        default:
            usage(argv[0], NULL);
        }
    }

    if (optind < argc)
        usage(argv[0], "Extra/unused arguments found, e.g. '%s'\n", argv[optind]);
    if (cli_settings->verify) {
        if (cli_settings->outputfile)
            usage(argv[0], "Verification (--verify) requires output file (-o/--output) to not set");
        if (cli_settings->muxer && !strcmp(cli_settings->muxer, "md5"))
            usage(argv[0], "Verification (--verify) requires the md5 muxer (--muxer md5)");

        cli_settings->outputfile = "-";
        if (!cli_settings->muxer)
            cli_settings->muxer = "md5";
    }

    if (!grain_specified && cli_settings->muxer &&
        !strcmp(cli_settings->muxer, "md5"))
    {
        lib_settings->apply_grain = 0;
    }

    if (!cli_settings->inputfile)
        usage(argv[0], "Input file (-i/--input) is required");
    if ((!cli_settings->muxer || strcmp(cli_settings->muxer, "null")) &&
        !cli_settings->outputfile)
    {
        usage(argv[0], "Output file (-o/--output) is required");
    }
}
