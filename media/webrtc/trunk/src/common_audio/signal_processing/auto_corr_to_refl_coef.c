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
 * This file contains the function WebRtcSpl_AutoCorrToReflCoef().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_AutoCorrToReflCoef(G_CONST WebRtc_Word32 *R, int use_order, WebRtc_Word16 *K)
{
    int i, n;
    WebRtc_Word16 tmp;
    G_CONST WebRtc_Word32 *rptr;
    WebRtc_Word32 L_num, L_den;
    WebRtc_Word16 *acfptr, *pptr, *wptr, *p1ptr, *w1ptr, ACF[WEBRTC_SPL_MAX_LPC_ORDER],
            P[WEBRTC_SPL_MAX_LPC_ORDER], W[WEBRTC_SPL_MAX_LPC_ORDER];

    // Initialize loop and pointers.
    acfptr = ACF;
    rptr = R;
    pptr = P;
    p1ptr = &P[1];
    w1ptr = &W[1];
    wptr = w1ptr;

    // First loop; n=0. Determine shifting.
    tmp = WebRtcSpl_NormW32(*R);
    *acfptr = (WebRtc_Word16)((*rptr++ << tmp) >> 16);
    *pptr++ = *acfptr++;

    // Initialize ACF, P and W.
    for (i = 1; i <= use_order; i++)
    {
        *acfptr = (WebRtc_Word16)((*rptr++ << tmp) >> 16);
        *wptr++ = *acfptr;
        *pptr++ = *acfptr++;
    }

    // Compute reflection coefficients.
    for (n = 1; n <= use_order; n++, K++)
    {
        tmp = WEBRTC_SPL_ABS_W16(*p1ptr);
        if (*P < tmp)
        {
            for (i = n; i <= use_order; i++)
                *K++ = 0;

            return;
        }

        // Division: WebRtcSpl_div(tmp, *P)
        *K = 0;
        if (tmp != 0)
        {
            L_num = tmp;
            L_den = *P;
            i = 15;
            while (i--)
            {
                (*K) <<= 1;
                L_num <<= 1;
                if (L_num >= L_den)
                {
                    L_num -= L_den;
                    (*K)++;
                }
            }
            if (*p1ptr > 0)
                *K = -*K;
        }

        // Last iteration; don't do Schur recursion.
        if (n == use_order)
            return;

        // Schur recursion.
        pptr = P;
        wptr = w1ptr;
        tmp = (WebRtc_Word16)(((WebRtc_Word32)*p1ptr * (WebRtc_Word32)*K + 16384) >> 15);
        *pptr = WEBRTC_SPL_ADD_SAT_W16( *pptr, tmp );
        pptr++;
        for (i = 1; i <= use_order - n; i++)
        {
            tmp = (WebRtc_Word16)(((WebRtc_Word32)*wptr * (WebRtc_Word32)*K + 16384) >> 15);
            *pptr = WEBRTC_SPL_ADD_SAT_W16( *(pptr+1), tmp );
            pptr++;
            tmp = (WebRtc_Word16)(((WebRtc_Word32)*pptr * (WebRtc_Word32)*K + 16384) >> 15);
            *wptr = WEBRTC_SPL_ADD_SAT_W16( *wptr, tmp );
            wptr++;
        }
    }
}
