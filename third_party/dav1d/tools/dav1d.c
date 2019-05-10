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
#include "vcs_version.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include "dav1d/dav1d.h"

#include "input/input.h"

#include "output/output.h"

#include "dav1d_cli_parse.h"

static void print_stats(const int istty, const unsigned n,
                        const unsigned num)
{
    const char *pre_string = istty ? "\r" : "";
    const char *post_string = istty ? "" : "\n";

    if (num == 0xFFFFFFFFU) {
        fprintf(stderr, "%sDecoded %u frames%s", pre_string, n, post_string);
    } else {
        fprintf(stderr, "%sDecoded %u/%u frames (%.1lf%%)%s",
                pre_string, n, num, 100.0 * n / num, post_string);
    }
}

int main(const int argc, char *const *const argv) {
    const int istty = isatty(fileno(stderr));
    int res = 0;
    CLISettings cli_settings;
    Dav1dSettings lib_settings;
    DemuxerContext *in;
    MuxerContext *out = NULL;
    Dav1dPicture p;
    Dav1dContext *c;
    Dav1dData data;
    unsigned n_out = 0, total, fps[2];
    const char *version = dav1d_version();

    if (strcmp(version, DAV1D_VERSION)) {
        fprintf(stderr, "Version mismatch (library: %s, executable: %s)\n",
                version, DAV1D_VERSION);
        return -1;
    }

    init_demuxers();
    init_muxers();
    parse(argc, argv, &cli_settings, &lib_settings);

    if ((res = input_open(&in, cli_settings.demuxer,
                          cli_settings.inputfile,
                          fps, &total)) < 0)
    {
        return res;
    }
    for (unsigned i = 0; i <= cli_settings.skip; i++) {
        if ((res = input_read(in, &data)) < 0) {
            input_close(in);
            return res;
        }
        if (i < cli_settings.skip) dav1d_data_unref(&data);
    }

    if (!cli_settings.quiet)
        fprintf(stderr, "dav1d %s - by VideoLAN\n", dav1d_version());

    // skip frames until a sequence header is found
    if (cli_settings.skip) {
        Dav1dSequenceHeader seq;
        unsigned seq_skip = 0;
        while (dav1d_parse_sequence_header(&seq, data.data, data.sz)) {
            if ((res = input_read(in, &data)) < 0) {
                input_close(in);
                return res;
            }
            seq_skip++;
        }
        if (seq_skip && !cli_settings.quiet)
            fprintf(stderr,
                    "skipped %u packets due to missing sequence header\n",
                    seq_skip);
    }

    //getc(stdin);
    if (cli_settings.limit != 0 && cli_settings.limit < total)
        total = cli_settings.limit;

    if ((res = dav1d_open(&c, &lib_settings)))
        return res;

    do {
        memset(&p, 0, sizeof(p));
        if ((res = dav1d_send_data(c, &data)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
        }

        if ((res = dav1d_get_picture(c, &p)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
            res = 0;
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    return res;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total);
        }

        if (cli_settings.limit && n_out == cli_settings.limit)
            break;
    } while (data.sz > 0 || !input_read(in, &data));

    if (data.sz > 0) dav1d_data_unref(&data);

    // flush
    if (res == 0) while (!cli_settings.limit || n_out < cli_settings.limit) {
        if ((res = dav1d_get_picture(c, &p)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
            } else {
                res = 0;
                break;
            }
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    return res;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total);
        }
    }

    input_close(in);
    if (out) {
        if (!cli_settings.quiet && istty)
            fprintf(stderr, "\n");
        if (cli_settings.verify)
            res |= output_verify(out, cli_settings.verify);
        else
            output_close(out);
    } else {
        fprintf(stderr, "No data decoded\n");
        res = 1;
    }
    dav1d_close(&c);

    return res;
}
