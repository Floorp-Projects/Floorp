/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_ports/config.h"
#include "vp8/common/idct.h"
#include "vp8/decoder/dequantize.h"

void idct_dequant_dc_0_2x_sse2
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int dst_stride, short *dc);
void idct_dequant_dc_full_2x_sse2
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int dst_stride, short *dc);

void idct_dequant_0_2x_sse2
            (short *q, short *dq ,unsigned char *pre,
             unsigned char *dst, int dst_stride, int blk_stride);
void idct_dequant_full_2x_sse2
            (short *q, short *dq ,unsigned char *pre,
             unsigned char *dst, int dst_stride, int blk_stride);

void vp8_dequant_dc_idct_add_y_block_sse2
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int stride, char *eobs, short *dc)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (((short *)(eobs))[0] & 0xfefe)
            idct_dequant_dc_full_2x_sse2 (q, dq, pre, dst, stride, dc);
        else
            idct_dequant_dc_0_2x_sse2 (q, dq, pre, dst, stride, dc);

        if (((short *)(eobs))[1] & 0xfefe)
            idct_dequant_dc_full_2x_sse2 (q+32, dq, pre+8, dst+8, stride, dc+2);
        else
            idct_dequant_dc_0_2x_sse2 (q+32, dq, pre+8, dst+8, stride, dc+2);

        q    += 64;
        dc   += 4;
        pre  += 64;
        dst  += stride*4;
        eobs += 4;
    }
}

void vp8_dequant_idct_add_y_block_sse2
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int stride, char *eobs)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (((short *)(eobs))[0] & 0xfefe)
            idct_dequant_full_2x_sse2 (q, dq, pre, dst, stride, 16);
        else
            idct_dequant_0_2x_sse2 (q, dq, pre, dst, stride, 16);

        if (((short *)(eobs))[1] & 0xfefe)
            idct_dequant_full_2x_sse2 (q+32, dq, pre+8, dst+8, stride, 16);
        else
            idct_dequant_0_2x_sse2 (q+32, dq, pre+8, dst+8, stride, 16);

        q    += 64;
        pre  += 64;
        dst  += stride*4;
        eobs += 4;
    }
}

void vp8_dequant_idct_add_uv_block_sse2
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dstu, unsigned char *dstv, int stride, char *eobs)
{
    if (((short *)(eobs))[0] & 0xfefe)
        idct_dequant_full_2x_sse2 (q, dq, pre, dstu, stride, 8);
    else
        idct_dequant_0_2x_sse2 (q, dq, pre, dstu, stride, 8);

    q    += 32;
    pre  += 32;
    dstu += stride*4;

    if (((short *)(eobs))[1] & 0xfefe)
        idct_dequant_full_2x_sse2 (q, dq, pre, dstu, stride, 8);
    else
        idct_dequant_0_2x_sse2 (q, dq, pre, dstu, stride, 8);

    q    += 32;
    pre  += 32;

    if (((short *)(eobs))[2] & 0xfefe)
        idct_dequant_full_2x_sse2 (q, dq, pre, dstv, stride, 8);
    else
        idct_dequant_0_2x_sse2 (q, dq, pre, dstv, stride, 8);

    q    += 32;
    pre  += 32;
    dstv += stride*4;

    if (((short *)(eobs))[3] & 0xfefe)
        idct_dequant_full_2x_sse2 (q, dq, pre, dstv, stride, 8);
    else
        idct_dequant_0_2x_sse2 (q, dq, pre, dstv, stride, 8);
}
