/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <dav1d/dav1d.h>
#include <tests/libfuzzer/dav1d_fuzzer.h>

static unsigned r32le(const uint8_t *const p) {
    return ((uint32_t)p[3] << 24U) | (p[2] << 16U) | (p[1] << 8U) | p[0];
}

// expects ivf input

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Dav1dSettings settings = { 0 };
    Dav1dContext * ctx = NULL;
    Dav1dPicture pic;
    const uint8_t *ptr = data;
    int err;

    dav1d_version();

    if (size < 32) goto end;
    ptr += 32; // skip ivf header

    dav1d_default_settings(&settings);

#ifdef DAV1D_MT_FUZZING
    settings.n_frame_threads = settings.n_tile_threads = 2;
#else
    settings.n_frame_threads = settings.n_tile_threads = 1;
#endif

    err = dav1d_open(&ctx, &settings);
    if (err < 0) goto end;

    while (ptr <= data + size - 12) {
        Dav1dData buf;

        size_t frame_size = r32le(ptr);
        ptr += 12;

        if (frame_size > size || ptr > data + size - frame_size)
            break;

        // copy frame data to a new buffer to catch reads past the end of input
        err = dav1d_data_create(&buf, frame_size);
        if (err) goto cleanup;
        memcpy(buf.data, ptr, frame_size);
        ptr += frame_size;

        do {
            memset(&pic, 0, sizeof(pic));
            err = dav1d_decode(ctx, &buf, &pic);
            if (err == 0) {
                dav1d_picture_unref(&pic);
            } else if (err != -EAGAIN) {
                break;
            }
        } while (buf.sz > 0);

        if (buf.sz > 0 || frame_size == 0)
            dav1d_data_unref(&buf);
    }

    do {
        memset(&pic, 0, sizeof(pic));
        err = dav1d_decode(ctx, NULL, &pic);
        if (err == 0)
            dav1d_picture_unref(&pic);
    } while (err == 0);

cleanup:
    dav1d_flush(ctx);
    dav1d_close(&ctx);
end:
    return 0;
}
