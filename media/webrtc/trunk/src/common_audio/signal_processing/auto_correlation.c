/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file contains the function WebRtcSpl_AutoCorrelation().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_AutoCorrelation(G_CONST WebRtc_Word16* in_vector,
                              int in_vector_length,
                              int order,
                              WebRtc_Word32* result,
                              int* scale)
{
    WebRtc_Word32 sum;
    int i, j;
    WebRtc_Word16 smax; // Sample max
    G_CONST WebRtc_Word16* xptr1;
    G_CONST WebRtc_Word16* xptr2;
    WebRtc_Word32* resultptr;
    int scaling = 0;

#ifdef _ARM_OPT_
#pragma message("NOTE: _ARM_OPT_ optimizations are used")
    WebRtc_Word16 loops4;
#endif

    if (order < 0)
        order = in_vector_length;

    // Find the max. sample
    smax = WebRtcSpl_MaxAbsValueW16(in_vector, in_vector_length);

    // In order to avoid overflow when computing the sum we should scale the samples so that
    // (in_vector_length * smax * smax) will not overflow.

    if (smax == 0)
    {
        scaling = 0;
    } else
    {
        int nbits = WebRtcSpl_GetSizeInBits(in_vector_length); // # of bits in the sum loop
        int t = WebRtcSpl_NormW32(WEBRTC_SPL_MUL(smax, smax)); // # of bits to normalize smax

        if (t > nbits)
        {
            scaling = 0;
        } else
        {
            scaling = nbits - t;
        }

    }

    resultptr = result;

    // Perform the actual correlation calculation
    for (i = 0; i < order + 1; i++)
    {
        int loops = (in_vector_length - i);
        sum = 0;
        xptr1 = in_vector;
        xptr2 = &in_vector[i];
#ifndef _ARM_OPT_
        for (j = loops; j > 0; j--)
        {
            sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1++, *xptr2++, scaling);
        }
#else
        loops4 = (loops >> 2) << 2;

        if (scaling == 0)
        {
            for (j = 0; j < loops4; j = j + 4)
            {
                sum += WEBRTC_SPL_MUL_16_16(*xptr1, *xptr2);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16(*xptr1, *xptr2);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16(*xptr1, *xptr2);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16(*xptr1, *xptr2);
                xptr1++;
                xptr2++;
            }

            for (j = loops4; j < loops; j++)
            {
                sum += WEBRTC_SPL_MUL_16_16(*xptr1, *xptr2);
                xptr1++;
                xptr2++;
            }
        }
        else
        {
            for (j = 0; j < loops4; j = j + 4)
            {
                sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1, *xptr2, scaling);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1, *xptr2, scaling);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1, *xptr2, scaling);
                xptr1++;
                xptr2++;
                sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1, *xptr2, scaling);
                xptr1++;
                xptr2++;
            }

            for (j = loops4; j < loops; j++)
            {
                sum += WEBRTC_SPL_MUL_16_16_RSFT(*xptr1, *xptr2, scaling);
                xptr1++;
                xptr2++;
            }
        }

#endif
        *resultptr++ = sum;
    }

    *scale = scaling;

    return order + 1;
}
