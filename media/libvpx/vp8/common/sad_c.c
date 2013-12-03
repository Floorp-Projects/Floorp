/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <limits.h>
#include <stdlib.h>
#include "vpx_config.h"
#include "vpx/vpx_integer.h"

static unsigned int sad_mx_n_c(const unsigned char *src_ptr, int src_stride,
                               const unsigned char *ref_ptr, int ref_stride,
                               unsigned int max_sad, int m, int n)
{
    int r, c;
    unsigned int sad = 0;

    for (r = 0; r < n; r++)
    {
        for (c = 0; c < m; c++)
        {
            sad += abs(src_ptr[c] - ref_ptr[c]);
        }

        if (sad > max_sad)
          break;

        src_ptr += src_stride;
        ref_ptr += ref_stride;
    }

    return sad;
}

/* max_sad is provided as an optional optimization point. Alternative
 * implementations of these functions are not required to check it.
 */

unsigned int vp8_sad16x16_c(const unsigned char *src_ptr, int src_stride,
                            const unsigned char *ref_ptr, int ref_stride,
                            unsigned int max_sad)
{
    return sad_mx_n_c(src_ptr, src_stride, ref_ptr, ref_stride, max_sad, 16, 16);
}

unsigned int vp8_sad8x8_c(const unsigned char *src_ptr, int src_stride,
                          const unsigned char *ref_ptr, int ref_stride,
                          unsigned int max_sad)
{
    return sad_mx_n_c(src_ptr, src_stride, ref_ptr, ref_stride, max_sad, 8, 8);
}

unsigned int vp8_sad16x8_c(const unsigned char *src_ptr, int src_stride,
                           const unsigned char *ref_ptr, int ref_stride,
                           unsigned int max_sad)
{
    return sad_mx_n_c(src_ptr, src_stride, ref_ptr, ref_stride, max_sad, 16, 8);

}

unsigned int vp8_sad8x16_c(const unsigned char *src_ptr, int src_stride,
                           const unsigned char *ref_ptr, int ref_stride,
                           unsigned int max_sad)
{
    return sad_mx_n_c(src_ptr, src_stride, ref_ptr, ref_stride, max_sad, 8, 16);
}

unsigned int vp8_sad4x4_c(const unsigned char *src_ptr, int src_stride,
                          const unsigned char *ref_ptr, int ref_stride,
                          unsigned int max_sad)
{
    return sad_mx_n_c(src_ptr, src_stride, ref_ptr, ref_stride, max_sad, 4, 4);
}

void vp8_sad16x16x3_c(const unsigned char *src_ptr, int src_stride,
                      const unsigned char *ref_ptr, int ref_stride,
                      unsigned int *sad_array)
{
    sad_array[0] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
}

void vp8_sad16x16x8_c(const unsigned char *src_ptr, int src_stride,
                      const unsigned char *ref_ptr, int ref_stride,
                      unsigned short *sad_array)
{
    sad_array[0] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
    sad_array[3] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 3, ref_stride, UINT_MAX);
    sad_array[4] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 4, ref_stride, UINT_MAX);
    sad_array[5] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 5, ref_stride, UINT_MAX);
    sad_array[6] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 6, ref_stride, UINT_MAX);
    sad_array[7] = (unsigned short)vp8_sad16x16_c(src_ptr, src_stride, ref_ptr + 7, ref_stride, UINT_MAX);
}

void vp8_sad16x8x3_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char *ref_ptr, int ref_stride,
                     unsigned int *sad_array)
{
    sad_array[0] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
}

void vp8_sad16x8x8_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char *ref_ptr, int ref_stride,
                     unsigned short *sad_array)
{
    sad_array[0] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
    sad_array[3] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 3, ref_stride, UINT_MAX);
    sad_array[4] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 4, ref_stride, UINT_MAX);
    sad_array[5] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 5, ref_stride, UINT_MAX);
    sad_array[6] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 6, ref_stride, UINT_MAX);
    sad_array[7] = (unsigned short)vp8_sad16x8_c(src_ptr, src_stride, ref_ptr + 7, ref_stride, UINT_MAX);
}

void vp8_sad8x8x3_c(const unsigned char *src_ptr, int src_stride,
                    const unsigned char *ref_ptr, int ref_stride,
                    unsigned int *sad_array)
{
    sad_array[0] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
}

void vp8_sad8x8x8_c(const unsigned char *src_ptr, int src_stride,
                    const unsigned char *ref_ptr, int ref_stride,
                    unsigned short *sad_array)
{
    sad_array[0] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
    sad_array[3] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 3, ref_stride, UINT_MAX);
    sad_array[4] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 4, ref_stride, UINT_MAX);
    sad_array[5] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 5, ref_stride, UINT_MAX);
    sad_array[6] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 6, ref_stride, UINT_MAX);
    sad_array[7] = (unsigned short)vp8_sad8x8_c(src_ptr, src_stride, ref_ptr + 7, ref_stride, UINT_MAX);
}

void vp8_sad8x16x3_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char *ref_ptr, int ref_stride,
                     unsigned int *sad_array)
{
    sad_array[0] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
}

void vp8_sad8x16x8_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char *ref_ptr, int ref_stride,
                     unsigned short *sad_array)
{
    sad_array[0] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
    sad_array[3] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 3, ref_stride, UINT_MAX);
    sad_array[4] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 4, ref_stride, UINT_MAX);
    sad_array[5] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 5, ref_stride, UINT_MAX);
    sad_array[6] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 6, ref_stride, UINT_MAX);
    sad_array[7] = (unsigned short)vp8_sad8x16_c(src_ptr, src_stride, ref_ptr + 7, ref_stride, UINT_MAX);
}

void vp8_sad4x4x3_c(const unsigned char *src_ptr, int src_stride,
                    const unsigned char *ref_ptr, int ref_stride,
                    unsigned int *sad_array)
{
    sad_array[0] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
}

void vp8_sad4x4x8_c(const unsigned char *src_ptr, int src_stride,
                    const unsigned char *ref_ptr, int ref_stride,
                    unsigned short *sad_array)
{
    sad_array[0] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 0, ref_stride, UINT_MAX);
    sad_array[1] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 1, ref_stride, UINT_MAX);
    sad_array[2] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 2, ref_stride, UINT_MAX);
    sad_array[3] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 3, ref_stride, UINT_MAX);
    sad_array[4] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 4, ref_stride, UINT_MAX);
    sad_array[5] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 5, ref_stride, UINT_MAX);
    sad_array[6] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 6, ref_stride, UINT_MAX);
    sad_array[7] = (unsigned short)vp8_sad4x4_c(src_ptr, src_stride, ref_ptr + 7, ref_stride, UINT_MAX);
}

void vp8_sad16x16x4d_c(const unsigned char *src_ptr, int src_stride,
                       const unsigned char * const ref_ptr[], int ref_stride,
                       unsigned int *sad_array)
{
    sad_array[0] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr[0], ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr[1], ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr[2], ref_stride, UINT_MAX);
    sad_array[3] = vp8_sad16x16_c(src_ptr, src_stride, ref_ptr[3], ref_stride, UINT_MAX);
}

void vp8_sad16x8x4d_c(const unsigned char *src_ptr, int src_stride,
                      const unsigned char * const ref_ptr[], int ref_stride,
                      unsigned int *sad_array)
{
    sad_array[0] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr[0], ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr[1], ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr[2], ref_stride, UINT_MAX);
    sad_array[3] = vp8_sad16x8_c(src_ptr, src_stride, ref_ptr[3], ref_stride, UINT_MAX);
}

void vp8_sad8x8x4d_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char * const ref_ptr[], int ref_stride,
                     unsigned int *sad_array)
{
    sad_array[0] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr[0], ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr[1], ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr[2], ref_stride, UINT_MAX);
    sad_array[3] = vp8_sad8x8_c(src_ptr, src_stride, ref_ptr[3], ref_stride, UINT_MAX);
}

void vp8_sad8x16x4d_c(const unsigned char *src_ptr, int src_stride,
                      const unsigned char * const ref_ptr[], int ref_stride,
                      unsigned int *sad_array)
{
    sad_array[0] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr[0], ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr[1], ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr[2], ref_stride, UINT_MAX);
    sad_array[3] = vp8_sad8x16_c(src_ptr, src_stride, ref_ptr[3], ref_stride, UINT_MAX);
}

void vp8_sad4x4x4d_c(const unsigned char *src_ptr, int src_stride,
                     const unsigned char * const ref_ptr[], int  ref_stride,
                     unsigned int *sad_array)
{
    sad_array[0] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr[0], ref_stride, UINT_MAX);
    sad_array[1] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr[1], ref_stride, UINT_MAX);
    sad_array[2] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr[2], ref_stride, UINT_MAX);
    sad_array[3] = vp8_sad4x4_c(src_ptr, src_stride, ref_ptr[3], ref_stride, UINT_MAX);
}

/* Copy 2 macroblocks to a buffer */
void vp8_copy32xn_c(unsigned char *src_ptr, int src_stride,
                    unsigned char *dst_ptr, int dst_stride,
                    int height)
{
    int r;

    for (r = 0; r < height; r++)
    {
#if !(CONFIG_FAST_UNALIGNED)
        dst_ptr[0] = src_ptr[0];
        dst_ptr[1] = src_ptr[1];
        dst_ptr[2] = src_ptr[2];
        dst_ptr[3] = src_ptr[3];
        dst_ptr[4] = src_ptr[4];
        dst_ptr[5] = src_ptr[5];
        dst_ptr[6] = src_ptr[6];
        dst_ptr[7] = src_ptr[7];
        dst_ptr[8] = src_ptr[8];
        dst_ptr[9] = src_ptr[9];
        dst_ptr[10] = src_ptr[10];
        dst_ptr[11] = src_ptr[11];
        dst_ptr[12] = src_ptr[12];
        dst_ptr[13] = src_ptr[13];
        dst_ptr[14] = src_ptr[14];
        dst_ptr[15] = src_ptr[15];
        dst_ptr[16] = src_ptr[16];
        dst_ptr[17] = src_ptr[17];
        dst_ptr[18] = src_ptr[18];
        dst_ptr[19] = src_ptr[19];
        dst_ptr[20] = src_ptr[20];
        dst_ptr[21] = src_ptr[21];
        dst_ptr[22] = src_ptr[22];
        dst_ptr[23] = src_ptr[23];
        dst_ptr[24] = src_ptr[24];
        dst_ptr[25] = src_ptr[25];
        dst_ptr[26] = src_ptr[26];
        dst_ptr[27] = src_ptr[27];
        dst_ptr[28] = src_ptr[28];
        dst_ptr[29] = src_ptr[29];
        dst_ptr[30] = src_ptr[30];
        dst_ptr[31] = src_ptr[31];
#else
        ((uint32_t *)dst_ptr)[0] = ((uint32_t *)src_ptr)[0] ;
        ((uint32_t *)dst_ptr)[1] = ((uint32_t *)src_ptr)[1] ;
        ((uint32_t *)dst_ptr)[2] = ((uint32_t *)src_ptr)[2] ;
        ((uint32_t *)dst_ptr)[3] = ((uint32_t *)src_ptr)[3] ;
        ((uint32_t *)dst_ptr)[4] = ((uint32_t *)src_ptr)[4] ;
        ((uint32_t *)dst_ptr)[5] = ((uint32_t *)src_ptr)[5] ;
        ((uint32_t *)dst_ptr)[6] = ((uint32_t *)src_ptr)[6] ;
        ((uint32_t *)dst_ptr)[7] = ((uint32_t *)src_ptr)[7] ;
#endif
        src_ptr += src_stride;
        dst_ptr += dst_stride;

    }
}
