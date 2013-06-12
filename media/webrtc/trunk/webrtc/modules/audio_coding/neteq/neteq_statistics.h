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
 * Definitions of statistics data structures for MCU and DSP sides.
 */

#include "typedefs.h"

#ifndef NETEQ_STATISTICS_H
#define NETEQ_STATISTICS_H

/*
 * Statistics struct on DSP side
 */
typedef struct
{

    /* variables for in-call statistics; queried through WebRtcNetEQ_GetNetworkStatistics */
    WebRtc_UWord32 expandLength; /* number of samples produced through expand */
    WebRtc_UWord32 preemptiveLength; /* number of samples produced through pre-emptive
     expand */
    WebRtc_UWord32 accelerateLength; /* number of samples removed through accelerate */
    int addedSamples; /* number of samples inserted in off mode */

    /* variables for post-call statistics; queried through WebRtcNetEQ_GetJitterStatistics */
    WebRtc_UWord32 expandedVoiceSamples; /* number of voice samples produced through expand */
    WebRtc_UWord32 expandedNoiseSamples; /* number of noise (background) samples produced
     through expand */

} DSPStats_t;

#endif

