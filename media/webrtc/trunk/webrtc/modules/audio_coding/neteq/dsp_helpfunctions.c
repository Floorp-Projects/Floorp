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
 * This file contains some help functions that did not fit elsewhere.
 */

#include "dsp_helpfunctions.h"


int16_t WebRtcNetEQ_CalcFsMult(uint16_t fsHz)
{
    switch (fsHz)
    {
        case 8000:
        {
            return 1;
        }
        case 16000:
        {
            return 2;
        }
        case 32000:
        {
            return 4;
        }
        case 48000:
        {
            return 6;
        }
        default:
        {
            return 1;
        }
    }
}


int WebRtcNetEQ_DownSampleTo4kHz(const int16_t *in, int inLen, uint16_t inFsHz,
                                 int16_t *out, int outLen, int compensateDelay)
{
    int16_t *B; /* filter coefficients */
    int16_t Blen; /* number of coefficients */
    int16_t filterDelay; /* phase delay in samples */
    int16_t factor; /* conversion rate (inFsHz/8000) */
    int ok;

    /* Set constants depending on frequency used */
    /* NOTE: The phase delay values are wrong compared to the true phase delay
     of the filters. However, the error is preserved (through the +1 term)
     for consistency. */
    switch (inFsHz)
    {
        case 8000:
        {
            Blen = 3;
            factor = 2;
            B = (int16_t*) WebRtcNetEQ_kDownsample8kHzTbl;
            filterDelay = 1 + 1;
            break;
        }
#ifdef NETEQ_WIDEBAND
            case 16000:
            {
                Blen = 5;
                factor = 4;
                B = (int16_t*) WebRtcNetEQ_kDownsample16kHzTbl;
                filterDelay = 2 + 1;
                break;
            }
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
            case 32000:
            {
                Blen = 7;
                factor = 8;
                B = (int16_t*) WebRtcNetEQ_kDownsample32kHzTbl;
                filterDelay = 3 + 1;
                break;
            }
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
            case 48000:
            {
                Blen = 7;
                factor = 12;
                B = (int16_t*) WebRtcNetEQ_kDownsample48kHzTbl;
                filterDelay = 3 + 1;
                break;
            }
#endif
        default:
        {
            /* unsupported or wrong sample rate */
            return -1;
        }
    }

    if (!compensateDelay)
    {
        /* disregard delay compensation */
        filterDelay = 0;
    }

    ok = WebRtcSpl_DownsampleFast((int16_t*) &in[Blen - 1],
        (int16_t) (inLen - (Blen - 1)), /* number of input samples */
        out, (int16_t) outLen, /* number of output samples to produce */
        B, Blen, factor, filterDelay); /* filter parameters */

    return ok; /* return value is -1 if input signal is too short */

}

