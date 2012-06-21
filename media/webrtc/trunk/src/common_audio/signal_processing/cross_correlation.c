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
 * This file contains the function WebRtcSpl_CrossCorrelation().
 * The description header can be found in signal_processing_library.h
 *
 */

/* TODO(kma): Clean up the code in this file, and break it up for
 * various platforms (Xscale, ARM/Neon etc.).
 */

#include "signal_processing_library.h"

void WebRtcSpl_CrossCorrelation(WebRtc_Word32* cross_correlation, WebRtc_Word16* seq1,
                                WebRtc_Word16* seq2, WebRtc_Word16 dim_seq,
                                WebRtc_Word16 dim_cross_correlation,
                                WebRtc_Word16 right_shifts,
                                WebRtc_Word16 step_seq2)
{
    int i, j;
    WebRtc_Word16* seq1Ptr;
    WebRtc_Word16* seq2Ptr;
    WebRtc_Word32* CrossCorrPtr;

#ifdef _XSCALE_OPT_

#ifdef _WIN32
#pragma message("NOTE: _XSCALE_OPT_ optimizations are used (overrides _ARM_OPT_ and requires /QRxscale compiler flag)")
#endif

    __int64 macc40;

    int iseq1[250];
    int iseq2[250];
    int iseq3[250];
    int * iseq1Ptr;
    int * iseq2Ptr;
    int * iseq3Ptr;
    int len, i_len;

    seq1Ptr = seq1;
    iseq1Ptr = iseq1;
    for(i = 0; i < ((dim_seq + 1) >> 1); i++)
    {
        *iseq1Ptr = (unsigned short)*seq1Ptr++;
        *iseq1Ptr++ |= (WebRtc_Word32)*seq1Ptr++ << 16;

    }

    if(dim_seq%2)
    {
        *(iseq1Ptr-1) &= 0x0000ffff;
    }
    *iseq1Ptr = 0;
    iseq1Ptr++;
    *iseq1Ptr = 0;
    iseq1Ptr++;
    *iseq1Ptr = 0;

    if(step_seq2 < 0)
    {
        seq2Ptr = seq2 - dim_cross_correlation + 1;
        CrossCorrPtr = &cross_correlation[dim_cross_correlation - 1];
    }
    else
    {
        seq2Ptr = seq2;
        CrossCorrPtr = cross_correlation;
    }

    len = dim_seq + dim_cross_correlation - 1;
    i_len = (len + 1) >> 1;
    iseq2Ptr = iseq2;

    iseq3Ptr = iseq3;
    for(i = 0; i < i_len; i++)
    {
        *iseq2Ptr = (unsigned short)*seq2Ptr++;
        *iseq3Ptr = (unsigned short)*seq2Ptr;
        *iseq2Ptr++ |= (WebRtc_Word32)*seq2Ptr++ << 16;
        *iseq3Ptr++ |= (WebRtc_Word32)*seq2Ptr << 16;
    }

    if(len % 2)
    {
        iseq2[i_len - 1] &= 0x0000ffff;
        iseq3[i_len - 1] = 0;
    }
    else
    iseq3[i_len - 1] &= 0x0000ffff;

    iseq2[i_len] = 0;
    iseq3[i_len] = 0;
    iseq2[i_len + 1] = 0;
    iseq3[i_len + 1] = 0;
    iseq2[i_len + 2] = 0;
    iseq3[i_len + 2] = 0;

    // Set pointer to start value
    iseq2Ptr = iseq2;
    iseq3Ptr = iseq3;

    i_len = (dim_seq + 7) >> 3;
    for (i = 0; i < dim_cross_correlation; i++)
    {

        iseq1Ptr = iseq1;

        macc40 = 0;

        _WriteCoProcessor(macc40, 0);

        if((i & 1))
        {
            iseq3Ptr = iseq3 + (i >> 1);
            for (j = i_len; j > 0; j--)
            {
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq3Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq3Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq3Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq3Ptr++);
            }
        }
        else
        {
            iseq2Ptr = iseq2 + (i >> 1);
            for (j = i_len; j > 0; j--)
            {
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq2Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq2Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq2Ptr++);
                _SmulAddPack_2SW_ACC(*iseq1Ptr++, *iseq2Ptr++);
            }

        }

        macc40 = _ReadCoProcessor(0);
        *CrossCorrPtr = (WebRtc_Word32)(macc40 >> right_shifts);
        CrossCorrPtr += step_seq2;
    }
#else // #ifdef _XSCALE_OPT_
#ifdef _ARM_OPT_
    WebRtc_Word16 dim_seq8 = (dim_seq >> 3) << 3;
#endif

    CrossCorrPtr = cross_correlation;

    for (i = 0; i < dim_cross_correlation; i++)
    {
        // Set the pointer to the static vector, set the pointer to the sliding vector
        // and initialize cross_correlation
        seq1Ptr = seq1;
        seq2Ptr = seq2 + (step_seq2 * i);
        (*CrossCorrPtr) = 0;

#ifndef _ARM_OPT_ 
#ifdef _WIN32
#pragma message("NOTE: default implementation is used")
#endif
        // Perform the cross correlation
        for (j = 0; j < dim_seq; j++)
        {
            (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr), right_shifts);
            seq1Ptr++;
            seq2Ptr++;
        }
#else
#ifdef _WIN32
#pragma message("NOTE: _ARM_OPT_ optimizations are used")
#endif
        if (right_shifts == 0)
        {
            // Perform the optimized cross correlation
            for (j = 0; j < dim_seq8; j = j + 8)
            {
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
            }

            for (j = dim_seq8; j < dim_seq; j++)
            {
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
                seq1Ptr++;
                seq2Ptr++;
            }
        }
        else // right_shifts != 0

        {
            // Perform the optimized cross correlation
            for (j = 0; j < dim_seq8; j = j + 8)
            {
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
            }

            for (j = dim_seq8; j < dim_seq; j++)
            {
                (*CrossCorrPtr) += WEBRTC_SPL_MUL_16_16_RSFT((*seq1Ptr), (*seq2Ptr),
                                                             right_shifts);
                seq1Ptr++;
                seq2Ptr++;
            }
        }
#endif
        CrossCorrPtr++;
    }
#endif
}
