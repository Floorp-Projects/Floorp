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

#ifndef __DAV1D_PICTURE_H__
#define __DAV1D_PICTURE_H__

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "headers.h"

typedef struct Dav1dPictureParameters {
    int w; ///< width (in pixels)
    int h; ///< height (in pixels)
    enum Dav1dPixelLayout layout; ///< format of the picture
    int bpc; ///< bits per pixel component (8 or 10)
} Dav1dPictureParameters;

typedef struct Dav1dPicture {
    Dav1dSequenceHeader *seq_hdr;
    Dav1dFrameHeader *frame_hdr;

    /**
     * Pointers to planar image data (Y is [0], U is [1], V is [2]). The data
     * should be bytes (for 8 bpc) or words (for 10 bpc). In case of words
     * containing 10 bpc image data, the pixels should be located in the LSB
     * bits, so that values range between [0, 1023]; the upper bits should be
     * zero'ed out.
     */
    void *data[3];

    /**
     * Number of bytes between 2 lines in data[] for luma [0] or chroma [1].
     */
    ptrdiff_t stride[2];

    Dav1dPictureParameters p;
    Dav1dDataProps m;
    struct Dav1dRef *frame_hdr_ref, *seq_hdr_ref, *ref; ///< allocation origins

    void *allocator_data; ///< pointer managed by the allocator
} Dav1dPicture;

typedef struct Dav1dPicAllocator {
    void *cookie; ///< custom data to pass to the allocator callbacks.
    /**
     * Allocate the picture buffer based on the Dav1dPictureParameters.
     *
     * The data[0], data[1] and data[2] must be 32 byte aligned and with a
     * pixel width/height multiple of 128 pixels.
     * data[1] and data[2] must share the same stride[1].
     *
     * @param  pic The picture to allocate the buffer for. The callback needs to
     *             fill the picture data[0], data[1], data[2], stride[0] and
     *             stride[1].
     *             The allocator can fill the pic allocator_data pointer with
     *             a custom pointer that will be passed to
     *             release_picture_callback().
     * @param cookie Custom pointer passed to all calls.
    *
    * @return 0 on success. A negative errno value on error.
     */
    int (*alloc_picture_callback)(Dav1dPicture *pic, void *cookie);
    /**
     * Release the picture buffer.
     *
     * @param pic    The picture that was filled by alloc_picture_callback().
     * @param cookie Custom pointer passed to all calls.
     */
    void (*release_picture_callback)(Dav1dPicture *pic, void *cookie);
} Dav1dPicAllocator;

/**
 * Release reference to a picture.
 */
DAV1D_API void dav1d_picture_unref(Dav1dPicture *p);

#endif /* __DAV1D_PICTURE_H__ */
