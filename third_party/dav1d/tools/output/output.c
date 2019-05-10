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
#include <stdlib.h>
#include <string.h>

#include "output/output.h"
#include "output/muxer.h"

struct MuxerContext {
    MuxerPriv *data;
    const Muxer *impl;
};

#define MAX_NUM_MUXERS 4
static const Muxer *muxers[MAX_NUM_MUXERS];
static unsigned num_muxers = 0;

#define register_muxer(impl) { \
    extern const Muxer impl; \
    assert(num_muxers < MAX_NUM_MUXERS); \
    muxers[num_muxers++] = &impl; \
}

void init_muxers(void) {
    register_muxer(null_muxer);
    register_muxer(md5_muxer);
    register_muxer(yuv_muxer);
    register_muxer(y4m2_muxer);
}

static const char *find_extension(const char *const f) {
    const size_t l = strlen(f);

    if (l == 0) return NULL;

    const char *const end = &f[l - 1], *step = end;
    while ((*step >= 'a' && *step <= 'z') ||
           (*step >= 'A' && *step <= 'Z') ||
           (*step >= '0' && *step <= '9'))
    {
        step--;
    }

    return (step < end && step > f && *step == '.' && step[-1] != '/') ?
           &step[1] : NULL;
}

int output_open(MuxerContext **const c_out,
                const char *const name, const char *const filename,
                const Dav1dPictureParameters *const p, const unsigned fps[2])
{
    const Muxer *impl;
    MuxerContext *c;
    unsigned i;
    int res;

    if (name) {
        for (i = 0; i < num_muxers; i++) {
            if (!strcmp(muxers[i]->name, name)) {
                impl = muxers[i];
                break;
            }
        }
        if (i == num_muxers) {
            fprintf(stderr, "Failed to find muxer named \"%s\"\n", name);
            return DAV1D_ERR(ENOPROTOOPT);
        }
    } else {
        const char *ext = find_extension(filename);
        if (!ext) {
            fprintf(stderr, "No extension found for file %s\n", filename);
            return -1;
        }
        for (i = 0; i < num_muxers; i++) {
            if (!strcmp(muxers[i]->extension, ext)) {
                impl = muxers[i];
                break;
            }
        }
        if (i == num_muxers) {
            fprintf(stderr, "Failed to find muxer for extension \"%s\"\n", ext);
            return DAV1D_ERR(ENOPROTOOPT);
        }
    }

    if (!(c = malloc(sizeof(MuxerContext) + impl->priv_data_size))) {
        fprintf(stderr, "Failed to allocate memory\n");
        return DAV1D_ERR(ENOMEM);
    }
    c->impl = impl;
    c->data = (MuxerPriv *) &c[1];
    if (impl->write_header && (res = impl->write_header(c->data, filename, p, fps)) < 0) {
        free(c);
        return res;
    }
    *c_out = c;

    return 0;
}

int output_write(MuxerContext *const ctx, Dav1dPicture *const p) {
    int res;

    if ((res = ctx->impl->write_picture(ctx->data, p)) < 0)
        return res;

    return 0;
}

void output_close(MuxerContext *const ctx) {
    if (ctx->impl->write_trailer)
        ctx->impl->write_trailer(ctx->data);
    free(ctx);
}

int output_verify(MuxerContext *const ctx, const char *const md5_Str) {
    int res = 0;
    if (ctx->impl->verify)
        res = ctx->impl->verify(ctx->data, md5_Str);
    free(ctx);
    return res;
}
