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
 * This file contains the functionality for automatic buffer level optimization.
 */

#ifndef AUTOMODE_H
#define AUTOMODE_H

#include "typedefs.h"

/*************/
/* Constants */
/*************/

/* The beta parameter defines the trade-off between delay and underrun probability. */
/* It is defined through its inverse in Q30 */
#define AUTOMODE_BETA_INV_Q30 53687091  /* 1/20 in Q30 */
#define AUTOMODE_STREAMING_BETA_INV_Q30 536871 /* 1/2000 in Q30 */

/* Forgetting factor for the inter-arrival time statistics */
#define IAT_PROB_FACT 32745       /* 0.9993 in Q15 */

/* Maximum inter-arrival time to register (in "packet-times") */
#define MAX_IAT 64
#define PEAK_HEIGHT 20            /* 0.08s in Q8 */

/* The value (1<<5) sets maximum accelerate "speed" to about 100 ms/s */
#define AUTOMODE_TIMESCALE_LIMIT (1<<5)

/* Peak mode related parameters */
/* Number of peaks in peak vector; must be a power of 2 */
#define NUM_PEAKS 8

/* Must be NUM_PEAKS-1 */
#define PEAK_INDEX_MASK 0x0007

/* Longest accepted peak distance */
#define MAX_PEAK_PERIOD 10
#define MAX_STREAMING_PEAK_PERIOD 600 /* 10 minutes */

/* Number of peaks required before peak mode can be engaged */
#define NUM_PEAKS_REQUIRED 3

/* Drift term for cumulative sum */
#define CSUM_IAT_DRIFT 2

/*******************/
/* Automode struct */
/*******************/

/* The automode struct is a sub-struct of the
 bufstats-struct (BufstatsInst_t). */

typedef struct
{

    /* Filtered current buffer level */
    uint16_t levelFiltFact; /* filter forgetting factor in Q8 */
    int buffLevelFilt; /* filtered buffer level in Q8 */

    /* Inter-arrival time (iat) statistics */
    int32_t iatProb[MAX_IAT + 1]; /* iat probabilities in Q30 */
    int16_t iatProbFact; /* iat forgetting factor in Q15 */
    uint32_t packetIatCountSamp; /* time (in timestamps) elapsed since last
     packet arrival, based on RecOut calls */
    int optBufLevel; /* current optimal buffer level in Q8 */

    /* Packet related information */
    int16_t packetSpeechLenSamp; /* speech samples per incoming packet */
    int16_t lastPackCNGorDTMF; /* indicates that the last received packet
     contained special information */
    uint16_t lastSeqNo; /* sequence number for last packet received */
    uint32_t lastTimeStamp; /* timestamp for the last packet received */
    int32_t sampleMemory; /* memory position for keeping track of how many
     samples we cut during expand */
    int16_t prevTimeScale; /* indicates that the last mode was an accelerate
     or pre-emptive expand operation */
    uint32_t timescaleHoldOff; /* counter that is shifted one step right each
     RecOut call; time-scaling allowed when it has
     reached 0 */
    int16_t extraDelayMs; /* extra delay for sync with video */

    /* Peak-detection */
    /* vector with the latest peak periods (peak spacing in samples) */
    uint32_t peakPeriodSamp[NUM_PEAKS];
    /* vector with the latest peak heights (in packets) */
    int16_t peakHeightPkt[NUM_PEAKS];
    int16_t peakIndex; /* index for the vectors peakPeriodSamp and peakHeightPkt;
     -1 if still waiting for first peak */
    uint16_t peakThresholdPkt; /* definition of peak (in packets);
     calculated from PEAK_HEIGHT */
    uint32_t peakIatCountSamp; /* samples elapsed since last peak was observed */
    uint32_t curPeakPeriod; /* current maximum of peakPeriodSamp vector */
    int16_t curPeakHeight; /* derived from peakHeightPkt vector;
     used as optimal buffer level in peak mode */
    int16_t peakModeDisabled; /* ==0 if peak mode can be engaged; >0 if not */
    uint16_t peakFound; /* 1 if peaks are detected and extra delay is applied;
                        * 0 otherwise. */

    /* Post-call statistics */
    uint32_t countIAT500ms; /* number of times we got small network outage */
    uint32_t countIAT1000ms; /* number of times we got medium network outage */
    uint32_t countIAT2000ms; /* number of times we got large network outage */
    uint32_t longestIATms; /* mSec duration of longest network outage */

    int16_t cSumIatQ8; /* cumulative sum of inter-arrival times */
    int16_t maxCSumIatQ8; /* max cumulative sum IAT */
    uint32_t maxCSumUpdateTimer;/* time elapsed since maximum was observed */

} AutomodeInst_t;

/*************/
/* Functions */
/*************/

/****************************************************************************
 * WebRtcNetEQ_UpdateIatStatistics(...)
 *
 * Update the packet inter-arrival time statistics when a new packet arrives.
 * This function should be called for every arriving packet, with some
 * exceptions when using DTX/VAD and DTMF. A new optimal buffer level is
 * calculated after the update.
 *
 * Input:
 *		- inst	        : Automode instance
 *		- maxBufLen		: Maximum number of packets the buffer can hold
 *		- seqNumber     : RTP sequence number of incoming packet
 *      - timeStamp     : RTP timestamp of incoming packet
 *      - fsHz          : Sample rate in Hz
 *      - mdCodec       : Non-zero if the current codec is a multiple-
 *                        description codec
 *      - streamingMode : A non-zero value will increase jitter robustness (and delay)
 *
 * Output:
 *      - inst          : Updated automode instance
 *
 * Return value			:  0 - Ok
 *                        <0 - Error
 */

int WebRtcNetEQ_UpdateIatStatistics(AutomodeInst_t *inst, int maxBufLen,
                                    uint16_t seqNumber, uint32_t timeStamp,
                                    int32_t fsHz, int mdCodec, int streamingMode);

/****************************************************************************
 * WebRtcNetEQ_CalcOptimalBufLvl(...)
 *
 * Calculate the optimal buffer level based on packet inter-arrival time
 * statistics.
 *
 * Input:
 *		- inst	        : Automode instance
 *      - fsHz          : Sample rate in Hz
 *      - mdCodec       : Non-zero if the current codec is a multiple-
 *                        description codec
 *      - timeIatPkts   : Currently observed inter-arrival time in packets
 *      - streamingMode : A non-zero value will increase jitter robustness (and delay)
 *
 * Output:
 *      - inst          : Updated automode instance
 *
 * Return value			: >0 - Optimal buffer level
 *                        <0 - Error
 */

int16_t WebRtcNetEQ_CalcOptimalBufLvl(AutomodeInst_t *inst, int32_t fsHz,
                                      int mdCodec, uint32_t timeIatPkts,
                                      int streamingMode);

/****************************************************************************
 * WebRtcNetEQ_BufferLevelFilter(...)
 *
 * Update filtered buffer level. The function must be called once for each
 * RecOut call, since the timing of automode hinges on counters that are
 * updated by this function.
 *
 * Input:
 *      - curSizeMs8    : Total length of unused speech data in packet buffer
 *                        and sync buffer, in ms * 8
 *		- inst	        : Automode instance
 *		- sampPerCall	: Number of samples per RecOut call
 *      - fsMult        : Sample rate in Hz divided by 8000
 *
 * Output:
 *      - inst          : Updated automode instance
 *
 * Return value			:  0 - Ok
 *                      : <0 - Error
 */

int WebRtcNetEQ_BufferLevelFilter(int32_t curSizeMs8, AutomodeInst_t *inst,
                                  int sampPerCall, int16_t fsMult);

/****************************************************************************
 * WebRtcNetEQ_SetPacketSpeechLen(...)
 *
 * Provide the number of speech samples extracted from a packet to the
 * automode instance. Several of the calculations within automode depend
 * on knowing the packet size.
 *
 *
 * Input:
 *		- inst	        : Automode instance
 *		- newLenSamp    : Number of samples per RecOut call
 *      - fsHz          : Sample rate in Hz
 *
 * Output:
 *      - inst          : Updated automode instance
 *
 * Return value			:  0 - Ok
 *                        <0 - Error
 */

int WebRtcNetEQ_SetPacketSpeechLen(AutomodeInst_t *inst, int16_t newLenSamp,
                                   int32_t fsHz);

/****************************************************************************
 * WebRtcNetEQ_ResetAutomode(...)
 *
 * Reset the automode instance.
 *
 *
 * Input:
 *		- inst	            : Automode instance
 *		- maxBufLenPackets  : Maximum number of packets that the packet
 *                            buffer can hold (>1)
 *
 * Output:
 *      - inst              : Updated automode instance
 *
 * Return value			    :  0 - Ok
 */

int WebRtcNetEQ_ResetAutomode(AutomodeInst_t *inst, int maxBufLenPackets);

/****************************************************************************
 * WebRtcNetEQ_AverageIAT(...)
 *
 * Calculate the average inter-arrival time based on current statistics.
 * The average is expressed in parts per million relative the nominal. That is,
 * if the average inter-arrival time is equal to the nominal frame time,
 * the return value is zero. A positive value corresponds to packet spacing
 * being too large, while a negative value means that the packets arrive with
 * less spacing than expected.
 *
 *
 * Input:
 *    - inst              : Automode instance.
 *
 * Return value           : Average relative inter-arrival time in samples.
 */

int32_t WebRtcNetEQ_AverageIAT(const AutomodeInst_t *inst);

#endif /* AUTOMODE_H */
