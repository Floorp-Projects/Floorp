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

void vp8_dequant_dc_idct_add_y_block_v6
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int stride, char *eobs, short *dc)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (eobs[0] > 1)
            vp8_dequant_dc_idct_add_v6 (q, dq, pre, dst, 16, stride, dc[0]);
        else
            vp8_dc_only_idct_add_v6 (dc[0], pre, dst, 16, stride);

        if (eobs[1] > 1)
            vp8_dequant_dc_idct_add_v6 (q+16, dq, pre+4, dst+4, 16, stride, dc[1]);
        else
            vp8_dc_only_idct_add_v6 (dc[1], pre+4, dst+4, 16, stride);

        if (eobs[2] > 1)
            vp8_dequant_dc_idct_add_v6 (q+32, dq, pre+8, dst+8, 16, stride, dc[2]);
        else
            vp8_dc_only_idct_add_v6 (dc[2], pre+8, dst+8, 16, stride);

        if (eobs[3] > 1)
            vp8_dequant_dc_idct_add_v6 (q+48, dq, pre+12, dst+12, 16, stride, dc[3]);
        else
            vp8_dc_only_idct_add_v6 (dc[3], pre+12, dst+12, 16, stride);

        q    += 64;
        dc   += 4;
        pre  += 64;
        dst  += 4*stride;
        eobs += 4;
    }
}

void vp8_dequant_idct_add_y_block_v6
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dst, int stride, char *eobs)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (eobs[0] > 1)
            vp8_dequant_idct_add_v6 (q, dq, pre, dst, 16, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[0]*dq[0], pre, dst, 16, stride);
            ((int *)q)[0] = 0;
        }

        if (eobs[1] > 1)
            vp8_dequant_idct_add_v6 (q+16, dq, pre+4, dst+4, 16, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[16]*dq[0], pre+4, dst+4, 16, stride);
            ((int *)(q+16))[0] = 0;
        }

        if (eobs[2] > 1)
            vp8_dequant_idct_add_v6 (q+32, dq, pre+8, dst+8, 16, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[32]*dq[0], pre+8, dst+8, 16, stride);
            ((int *)(q+32))[0] = 0;
        }

        if (eobs[3] > 1)
            vp8_dequant_idct_add_v6 (q+48, dq, pre+12, dst+12, 16, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[48]*dq[0], pre+12, dst+12, 16, stride);
            ((int *)(q+48))[0] = 0;
        }

        q    += 64;
        pre  += 64;
        dst  += 4*stride;
        eobs += 4;
    }
}

void vp8_dequant_idct_add_uv_block_v6
            (short *q, short *dq, unsigned char *pre,
             unsigned char *dstu, unsigned char *dstv, int stride, char *eobs)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        if (eobs[0] > 1)
            vp8_dequant_idct_add_v6 (q, dq, pre, dstu, 8, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[0]*dq[0], pre, dstu, 8, stride);
            ((int *)q)[0] = 0;
        }

        if (eobs[1] > 1)
            vp8_dequant_idct_add_v6 (q+16, dq, pre+4, dstu+4, 8, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[16]*dq[0], pre+4, dstu+4, 8, stride);
            ((int *)(q+16))[0] = 0;
        }

        q    += 32;
        pre  += 32;
        dstu += 4*stride;
        eobs += 2;
    }

    for (i = 0; i < 2; i++)
    {
        if (eobs[0] > 1)
            vp8_dequant_idct_add_v6 (q, dq, pre, dstv, 8, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[0]*dq[0], pre, dstv, 8, stride);
            ((int *)q)[0] = 0;
        }

        if (eobs[1] > 1)
            vp8_dequant_idct_add_v6 (q+16, dq, pre+4, dstv+4, 8, stride);
        else
        {
            vp8_dc_only_idct_add_v6 (q[16]*dq[0], pre+4, dstv+4, 8, stride);
            ((int *)(q+16))[0] = 0;
        }

        q    += 32;
        pre  += 32;
        dstv += 4*stride;
        eobs += 2;
    }
}
