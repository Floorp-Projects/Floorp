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

#define  SYNC_PAYLOAD_LEN_BYTES  7
static const uint8_t kSyncPayload[SYNC_PAYLOAD_LEN_BYTES] = {
    'a', 'v', 's', 'y', 'n', 'c', '\0' };

/* Struct to hold the NetEQ instance */
typedef struct
{
    DSPInst_t DSPinst; /* DSP part of the NetEQ instance */
    MCUInst_t MCUinst; /* MCU part of the NetEQ instance */
    int16_t ErrorCode; /* Store last error code */
#ifdef NETEQ_STEREO
    int16_t masterSlave; /* 0 = not set, 1 = master, 2 = slave */
#endif /* NETEQ_STEREO */
} MainInst_t;

/* Struct used for communication between DSP and MCU sides of NetEQ */
typedef struct
{
    uint32_t playedOutTS; /* Timestamp position at end of DSP data */
    uint16_t samplesLeft; /* Number of samples stored */
    int16_t MD; /* Multiple description codec information */
    int16_t lastMode; /* Latest mode of NetEQ playout */
    int16_t frameLen; /* Frame length of previously decoded packet */
} DSP2MCU_info_t;

/* Initialize instances with read and write address */
int WebRtcNetEQ_DSPinit(MainInst_t *inst);

/* The DSP side will call this function to interrupt the MCU side */
int WebRtcNetEQ_DSP2MCUinterrupt(MainInst_t *inst, int16_t *pw16_shared_mem);

/* Returns 1 if the given payload matches |kSyncPayload| payload, otherwise
 * 0 is returned. */
int WebRtcNetEQ_IsSyncPayload(const void* payload, int payload_len_bytes);

#endif
