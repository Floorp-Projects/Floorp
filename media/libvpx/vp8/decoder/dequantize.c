/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license 
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may 
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "dequantize.h"
#include "predictdc.h"
#include "idct.h"
#include "vpx_mem/vpx_mem.h"

extern void vp8_short_idct4x4llm_c(short *input, short *output, int pitch) ;
extern void vp8_short_idct4x4llm_1_c(short *input, short *output, int pitch);


void vp8_dequantize_b_c(BLOCKD *d)
{
    int i;
    short *DQ  = d->dqcoeff;
    short *Q   = d->qcoeff;
    short *DQC = &d->dequant[0][0];

    for (i = 0; i < 16; i++)
    {
        DQ[i] = Q[i] * DQC[i];
    }
}

void vp8_dequant_idct_c(short *input, short *dq, short *output, int pitch)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        input[i] = dq[i] * input[i];
    }

    vp8_short_idct4x4llm_c(input, output, pitch);
    vpx_memset(input, 0, 32);
}

void vp8_dequant_dc_idct_c(short *input, short *dq, short *output, int pitch, int Dc)
{
    int i;

    input[0] = (short)Dc;

    for (i = 1; i < 16; i++)
    {
        input[i] = dq[i] * input[i];
    }

    vp8_short_idct4x4llm_c(input, output, pitch);
    vpx_memset(input, 0, 32);
}
