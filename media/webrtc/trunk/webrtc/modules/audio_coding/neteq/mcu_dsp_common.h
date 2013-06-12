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
 * The main NetEQ instance, which is where the DSP and MCU sides join.
 */

#ifndef MCU_DSP_COMMON_H
#define MCU_DSP_COMMON_H

#include "typedefs.h"

#include "dsp.h"
#include "mcu.h"

/* Define size of shared memory area. */
#if defined(NETEQ_48KHZ_WIDEBAND)
    #define SHARED_MEM_SIZE (6*640)
#elif defined(NETEQ_32KHZ_WIDEBAND)
    #define SHARED_MEM_SIZE (4*640)
#elif defined(NETEQ_WIDEBAND)
    #define SHARED_MEM_SIZE (2*640)
#else
    #define SHARED_MEM_SIZE 640
#endif

/* Struct to hold the NetEQ instance */
typedef struct
{
    DSPInst_t DSPinst; /* DSP part of the NetEQ instance */
    MCUInst_t MCUinst; /* MCU part of the NetEQ instance */
    WebRtc_Word16 ErrorCode; /* Store last error code */
#ifdef NETEQ_STEREO
    WebRtc_Word16 masterSlave; /* 0 = not set, 1 = master, 2 = slave */
#endif /* NETEQ_STEREO */
} MainInst_t;

/* Struct used for communication between DSP and MCU sides of NetEQ */
typedef struct
{
    WebRtc_UWord32 playedOutTS; /* Timestamp position at end of DSP data */
    WebRtc_UWord16 samplesLeft; /* Number of samples stored */
    WebRtc_Word16 MD; /* Multiple description codec information */
    WebRtc_Word16 lastMode; /* Latest mode of NetEQ playout */
    WebRtc_Word16 frameLen; /* Frame length of previously decoded packet */
} DSP2MCU_info_t;

/* Initialize instances with read and write address */
int WebRtcNetEQ_DSPinit(MainInst_t *inst);

/* The DSP side will call this function to interrupt the MCU side */
int WebRtcNetEQ_DSP2MCUinterrupt(MainInst_t *inst, WebRtc_Word16 *pw16_shared_mem);

#endif
