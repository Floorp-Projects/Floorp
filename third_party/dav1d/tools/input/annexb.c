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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "input/demuxer.h"

typedef struct DemuxerPriv {
    FILE *f;
    size_t temporal_unit_size;
    size_t frame_unit_size;
} AnnexbInputContext;

static int leb128(AnnexbInputContext *const c, size_t *const len) {
    unsigned more, i = 0;
    uint8_t byte;
    *len = 0;
    do {
        if (fread(&byte, 1, 1, c->f) < 1)
            return -1;
        more = byte & 0x80;
        unsigned bits = byte & 0x7f;
        if (i <= 3 || (i == 4 && bits < (1 << 4)))
            *len |= bits << (i * 7);
        else if (bits) return -1;
        if (++i == 8 && more) return -1;
    } while (more);
    return i;
}

static int annexb_open(AnnexbInputContext *const c, const char *const file,
                       unsigned fps[2], unsigned *const num_frames)
{
    int res;
    size_t len;

    if (!(c->f = fopen(file, "rb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    // TODO: Parse sequence header and read timing info if any.
    fps[0] = 25;
    fps[1] = 1;
    for (*num_frames = 0;; (*num_frames)++) {
        res = leb128(c, &len);
        if (res < 0)
            break;
        fseeko(c->f, len, SEEK_CUR);
    }
    fseeko(c->f, 0, SEEK_SET);

    return 0;
}

static int annexb_read(AnnexbInputContext *const c, Dav1dData *const data) {
    size_t len;
    int res;

    if (!c->temporal_unit_size) {
        res = leb128(c, &c->temporal_unit_size);
        if (res < 0) return -1;
    }
    if (!c->frame_unit_size) {
        res = leb128(c, &c->frame_unit_size);
        if (res < 0 || (c->frame_unit_size + res) > c->temporal_unit_size) return -1;
        c->temporal_unit_size -= res;
    }
    res = leb128(c, &len);
    if (res < 0 || (len + res) > c->frame_unit_size) return -1;
    uint8_t *ptr = dav1d_data_create(data, len);
    if (!ptr) return -1;
    c->temporal_unit_size -= len + res;
    c->frame_unit_size -= len + res;
    if (fread(ptr, len, 1, c->f) != 1) {
        fprintf(stderr, "Failed to read frame data: %s\n", strerror(errno));
        dav1d_data_unref(data);
        return -1;
    }

    return 0;
}

static void annexb_close(AnnexbInputContext *const c) {
    fclose(c->f);
}

const Demuxer annexb_demuxer = {
    .priv_data_size = sizeof(AnnexbInputContext),
    .name = "annexb",
    .extension = "obu",
    .open = annexb_open,
    .read = annexb_read,
    .close = annexb_close,
};
