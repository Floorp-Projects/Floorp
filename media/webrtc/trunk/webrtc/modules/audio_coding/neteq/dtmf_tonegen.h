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
 * This file contains the DTMF tone generator function.
 */

#ifndef DTMF_TONEGEN_H
#define DTMF_TONEGEN_H

#include "typedefs.h"

#include "neteq_defines.h"

#ifdef NETEQ_ATEVENT_DECODE
/* Must compile NetEQ with DTMF support to enable the functionality */

#define DTMF_AMP_LOW	23171	/* 3 dB lower than the high frequency */

/* The DTMF generator struct (part of DSP main struct DSPInst_t) */
typedef struct dtmf_tone_inst_t_
{

    int16_t reinit; /* non-zero if the oscillator model should
     be reinitialized for next event */
    int16_t oldOutputLow[2]; /* oscillator recursion history (low tone) */
    int16_t oldOutputHigh[2]; /* oscillator recursion history (high tone) */

    int lastDtmfSample; /* index to the first non-DTMF sample in the
     speech history, if non-negative */
}dtmf_tone_inst_t;

/****************************************************************************
 * WebRtcNetEQ_DTMFGenerate(...)
 *
 * Generate 10 ms DTMF signal according to input parameters.
 *
 * Input:
 *		- DTMFdecInst	: DTMF instance
 *      - value         : DTMF event number (0-15)
 *      - volume        : Volume of generated signal (0-36)
 *                        Volume is given in negative dBm0, i.e., volume == 0
 *                        means 0 dBm0 while volume == 36 mean -36 dBm0.
 *      - sampFreq      : Sample rate in Hz
 *
 * Output:
 *      - signal        : Pointer to vector where DTMF signal is stored;
 *                        Vector must be at least sampFreq/100 samples long.
 *		- DTMFdecInst	: Updated DTMF instance
 *
 * Return value			: >0 - Number of samples written to signal
 *                      : <0 - Error
 */

int16_t WebRtcNetEQ_DTMFGenerate(dtmf_tone_inst_t *DTMFdecInst,
                int16_t value,
                int16_t volume,
                int16_t *signal,
                uint16_t sampFreq,
                int16_t frameLen
);

#endif /* NETEQ_ATEVENT_DECODE */

#endif /* DTMF_TONEGEN_H */

