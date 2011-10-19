/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "blockd.h"

typedef enum
{
    PRED = 0,
    DEST = 1
} BLOCKSET;

static void setup_block
(
    BLOCKD *b,
    int mv_stride,
    unsigned char **base,
    int Stride,
    int offset,
    BLOCKSET bs
)
{

    if (bs == DEST)
    {
        b->dst_stride = Stride;
        b->dst = offset;
        b->base_dst = base;
    }
    else
    {
        b->pre_stride = Stride;
        b->pre = offset;
        b->base_pre = base;
    }

}


static void setup_macroblock(MACROBLOCKD *x, BLOCKSET bs)
{
    int block;

    unsigned char **y, **u, **v;

    if (bs == DEST)
    {
        y = &x->dst.y_buffer;
        u = &x->dst.u_buffer;
        v = &x->dst.v_buffer;
    }
    else
    {
        y = &x->pre.y_buffer;
        u = &x->pre.u_buffer;
        v = &x->pre.v_buffer;
    }

    for (block = 0; block < 16; block++) /* y blocks */
    {
        setup_block(&x->block[block], x->dst.y_stride, y, x->dst.y_stride,
                        (block >> 2) * 4 * x->dst.y_stride + (block & 3) * 4, bs);
    }

    for (block = 16; block < 20; block++) /* U and V blocks */
    {
        setup_block(&x->block[block], x->dst.uv_stride, u, x->dst.uv_stride,
                        ((block - 16) >> 1) * 4 * x->dst.uv_stride + (block & 1) * 4, bs);

        setup_block(&x->block[block+4], x->dst.uv_stride, v, x->dst.uv_stride,
                        ((block - 16) >> 1) * 4 * x->dst.uv_stride + (block & 1) * 4, bs);
    }
}

void vp8_setup_block_dptrs(MACROBLOCKD *x)
{
    int r, c;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 4; c++)
        {
            x->block[r*4+c].diff      = &x->diff[r * 4 * 16 + c * 4];
            x->block[r*4+c].predictor = x->predictor + r * 4 * 16 + c * 4;
        }
    }

    for (r = 0; r < 2; r++)
    {
        for (c = 0; c < 2; c++)
        {
            x->block[16+r*2+c].diff      = &x->diff[256 + r * 4 * 8 + c * 4];
            x->block[16+r*2+c].predictor = x->predictor + 256 + r * 4 * 8 + c * 4;

        }
    }

    for (r = 0; r < 2; r++)
    {
        for (c = 0; c < 2; c++)
        {
            x->block[20+r*2+c].diff      = &x->diff[320+ r * 4 * 8 + c * 4];
            x->block[20+r*2+c].predictor = x->predictor + 320 + r * 4 * 8 + c * 4;

        }
    }

    x->block[24].diff = &x->diff[384];

    for (r = 0; r < 25; r++)
    {
        x->block[r].qcoeff  = x->qcoeff  + r * 16;
        x->block[r].dqcoeff = x->dqcoeff + r * 16;
    }
}

void vp8_build_block_doffsets(MACROBLOCKD *x)
{

    /* handle the destination pitch features */
    setup_macroblock(x, DEST);
    setup_macroblock(x, PRED);
}
