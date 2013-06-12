/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the implementation of automatic buffer level optimization.
 */

#include "automode.h"

#include <assert.h>

#include "signal_processing_library.h"

#include "neteq_defines.h"

#ifdef NETEQ_DELAY_LOGGING
/* special code for offline delay logging */
#include <stdio.h>
#include "delay_logging.h"

extern FILE *delay_fid2; /* file pointer to delay log file */
#endif /* NETEQ_DELAY_LOGGING */


int WebRtcNetEQ_UpdateIatStatistics(AutomodeInst_t *inst, int maxBufLen,
                                    WebRtc_UWord16 seqNumber, WebRtc_UWord32 timeStamp,
                                    WebRtc_Word32 fsHz, int mdCodec, int streamingMode)
{
    WebRtc_UWord32 timeIat; /* inter-arrival time */
    int i;
    WebRtc_Word32 tempsum = 0; /* temp summation */
    WebRtc_Word32 tempvar; /* temporary variable */
    int retval = 0; /* return value */
    WebRtc_Word16 packetLenSamp; /* packet speech length in samples */

    /****************/
    /* Sanity check */
    /****************/

    if (maxBufLen <= 1 || fsHz <= 0)
    {
        /* maxBufLen must be at least 2 and fsHz must both be strictly positive */
        return -1;
    }

    /****************************/
    /* Update packet statistics */
    /****************************/

    /* Try calculating packet length from current and previous timestamps */
    if ((timeStamp <= inst->lastTimeStamp) || (seqNumber <= inst->lastSeqNo))
    {
        /* Wrong timestamp or sequence order; revert to backup plan */
        packetLenSamp = inst->packetSpeechLenSamp; /* use stored value */
    }
    else
    {
        /* calculate timestamps per packet */
        packetLenSamp = (WebRtc_Word16) WebRtcSpl_DivU32U16(timeStamp - inst->lastTimeStamp,
            seqNumber - inst->lastSeqNo);
    }

    /* Check that the packet size is positive; if not, the statistics cannot be updated. */
    if (packetLenSamp > 0)
    { /* packet size ok */

        /* calculate inter-arrival time in integer packets (rounding down) */
        timeIat = WebRtcSpl_DivW32W16(inst->packetIatCountSamp, packetLenSamp);

        /* Special operations for streaming mode */
        if (streamingMode != 0)
        {
            /*
             * Calculate IAT in Q8, including fractions of a packet (i.e., more accurate
             * than timeIat).
             */
            WebRtc_Word16 timeIatQ8 = (WebRtc_Word16) WebRtcSpl_DivW32W16(
                WEBRTC_SPL_LSHIFT_W32(inst->packetIatCountSamp, 8), packetLenSamp);

            /*
             * Calculate cumulative sum iat with sequence number compensation (ideal arrival
             * times makes this sum zero).
             */
            inst->cSumIatQ8 += (timeIatQ8
                - WEBRTC_SPL_LSHIFT_W32(seqNumber - inst->lastSeqNo, 8));

            /* subtract drift term */
            inst->cSumIatQ8 -= CSUM_IAT_DRIFT;

            /* ensure not negative */
            inst->cSumIatQ8 = WEBRTC_SPL_MAX(inst->cSumIatQ8, 0);

            /* remember max */
            if (inst->cSumIatQ8 > inst->maxCSumIatQ8)
            {
                inst->maxCSumIatQ8 = inst->cSumIatQ8;
                inst->maxCSumUpdateTimer = 0;
            }

            /* too long since the last maximum was observed; decrease max value */
            if (inst->maxCSumUpdateTimer > (WebRtc_UWord32) WEBRTC_SPL_MUL_32_16(fsHz,
                MAX_STREAMING_PEAK_PERIOD))
            {
                inst->maxCSumIatQ8 -= 4; /* remove 1000*4/256 = 15.6 ms/s */
            }
        } /* end of streaming mode */

        /* check for discontinuous packet sequence and re-ordering */
        if (seqNumber > inst->lastSeqNo + 1)
        {
            /* Compensate for gap in the sequence numbers.
             * Reduce IAT with expected extra time due to lost packets, but ensure that
             * the IAT is not negative.
             */
            timeIat -= WEBRTC_SPL_MIN(timeIat,
                (WebRtc_UWord32) (seqNumber - inst->lastSeqNo - 1));
        }
        else if (seqNumber < inst->lastSeqNo)
        {
            /* compensate for re-ordering */
            timeIat += (WebRtc_UWord32) (inst->lastSeqNo + 1 - seqNumber);
        }

        /* saturate IAT at maximum value */
        timeIat = WEBRTC_SPL_MIN( timeIat, MAX_IAT );

        /* update iatProb = forgetting_factor * iatProb for all elements */
        for (i = 0; i <= MAX_IAT; i++)
        {
            WebRtc_Word32 tempHi, tempLo; /* Temporary variables */

            /*
             * Multiply iatProbFact (Q15) with iatProb (Q30) and right-shift 15 steps
             * to come back to Q30. The operation is done in two steps:
             */

            /*
             * 1) Multiply the high 16 bits (15 bits + sign) of iatProb. Shift iatProb
             * 16 steps right to get the high 16 bits in a WebRtc_Word16 prior to
             * multiplication, and left-shift with 1 afterwards to come back to
             * Q30 = (Q15 * (Q30>>16)) << 1.
             */
            tempHi = WEBRTC_SPL_MUL_16_16(inst->iatProbFact,
                (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(inst->iatProb[i], 16));
            tempHi = WEBRTC_SPL_LSHIFT_W32(tempHi, 1); /* left-shift 1 step */

            /*
             * 2) Isolate and multiply the low 16 bits of iatProb. Right-shift 15 steps
             * afterwards to come back to Q30 = (Q15 * Q30) >> 15.
             */
            tempLo = inst->iatProb[i] & 0x0000FFFF; /* sift out the 16 low bits */
            tempLo = WEBRTC_SPL_MUL_16_U16(inst->iatProbFact,
                (WebRtc_UWord16) tempLo);
            tempLo = WEBRTC_SPL_RSHIFT_W32(tempLo, 15);

            /* Finally, add the high and low parts */
            inst->iatProb[i] = tempHi + tempLo;

            /* Sum all vector elements while we are at it... */
            tempsum += inst->iatProb[i];
        }

        /*
         * Increase the probability for the currently observed inter-arrival time
         * with 1 - iatProbFact. The factor is in Q15, iatProb in Q30;
         * hence, left-shift 15 steps to obtain result in Q30.
         */
        inst->iatProb[timeIat] += (32768 - inst->iatProbFact) << 15;

        tempsum += (32768 - inst->iatProbFact) << 15; /* add to vector sum */

        /*
         * Update iatProbFact (changes only during the first seconds after reset)
         * The factor converges to IAT_PROB_FACT.
         */
        inst->iatProbFact += (IAT_PROB_FACT - inst->iatProbFact + 3) >> 2;

        /* iatProb should sum up to 1 (in Q30). */
        tempsum -= 1 << 30; /* should be zero */

        /* Check if it does, correct if it doesn't. */
        if (tempsum > 0)
        {
            /* tempsum too large => decrease a few values in the beginning */
            i = 0;
            while (i <= MAX_IAT && tempsum > 0)
            {
                /* Remove iatProb[i] / 16 from iatProb, but not more than tempsum */
                tempvar = WEBRTC_SPL_MIN(tempsum, inst->iatProb[i] >> 4);
                inst->iatProb[i++] -= tempvar;
                tempsum -= tempvar;
            }
        }
        else if (tempsum < 0)
        {
            /* tempsum too small => increase a few values in the beginning */
            i = 0;
            while (i <= MAX_IAT && tempsum < 0)
            {
                /* Add iatProb[i] / 16 to iatProb, but not more than tempsum */
                tempvar = WEBRTC_SPL_MIN(-tempsum, inst->iatProb[i] >> 4);
                inst->iatProb[i++] += tempvar;
                tempsum += tempvar;
            }
        }

        /* Calculate optimal buffer level based on updated statistics */
        tempvar = (WebRtc_Word32) WebRtcNetEQ_CalcOptimalBufLvl(inst, fsHz, mdCodec, timeIat,
            streamingMode);
        if (tempvar > 0)
        {
            inst->optBufLevel = (WebRtc_UWord16) tempvar;

            if (streamingMode != 0)
            {
                inst->optBufLevel = WEBRTC_SPL_MAX(inst->optBufLevel,
                    inst->maxCSumIatQ8);
            }

            /*********/
            /* Limit */
            /*********/

            /* Subtract extra delay from maxBufLen */
            if (inst->extraDelayMs > 0 && inst->packetSpeechLenSamp > 0)
            {
                maxBufLen -= inst->extraDelayMs / inst->packetSpeechLenSamp * fsHz / 1000;
                maxBufLen = WEBRTC_SPL_MAX(maxBufLen, 1); // sanity: at least one packet
            }

            maxBufLen = WEBRTC_SPL_LSHIFT_W32(maxBufLen, 8); /* shift to Q8 */

            /* Enforce upper limit; 75% of maxBufLen */
            inst->optBufLevel = (WebRtc_UWord16) WEBRTC_SPL_MIN( inst->optBufLevel,
                (maxBufLen >> 1) + (maxBufLen >> 2) ); /* 1/2 + 1/4 = 75% */
        }
        else
        {
            retval = (int) tempvar;
        }

    } /* end if */

    /*******************************/
    /* Update post-call statistics */
    /*******************************/

    /* Calculate inter-arrival time in ms = packetIatCountSamp / (fsHz / 1000) */
    timeIat = WEBRTC_SPL_UDIV(
        WEBRTC_SPL_UMUL_32_16(inst->packetIatCountSamp, (WebRtc_Word16) 1000),
        (WebRtc_UWord32) fsHz);

    /* Increase counter corresponding to current inter-arrival time */
    if (timeIat > 2000)
    {
        inst->countIAT2000ms++;
    }
    else if (timeIat > 1000)
    {
        inst->countIAT1000ms++;
    }
    else if (timeIat > 500)
    {
        inst->countIAT500ms++;
    }

    if (timeIat > inst->longestIATms)
    {
        /* update maximum value */
        inst->longestIATms = timeIat;
    }

    /***********************************/
    /* Prepare for next packet arrival */
    /***********************************/

    inst->packetIatCountSamp = 0; /* reset inter-arrival time counter */

    inst->lastSeqNo = seqNumber; /* remember current sequence number */

    inst->lastTimeStamp = timeStamp; /* remember current timestamp */

    return retval;
}


WebRtc_Word16 WebRtcNetEQ_CalcOptimalBufLvl(AutomodeInst_t *inst, WebRtc_Word32 fsHz,
                                            int mdCodec, WebRtc_UWord32 timeIatPkts,
                                            int streamingMode)
{

    WebRtc_Word32 sum1 = 1 << 30; /* assign to 1 in Q30 */
    WebRtc_Word16 B;
    WebRtc_UWord16 Bopt;
    int i;
    WebRtc_Word32 betaInv; /* optimization parameter */

#ifdef NETEQ_DELAY_LOGGING
    /* special code for offline delay logging */
    int temp_var;
#endif

    /****************/
    /* Sanity check */
    /****************/

    if (fsHz <= 0)
    {
        /* fsHz must be strictly positive */
        return -1;
    }

    /***********************************************/
    /* Get betaInv parameter based on playout mode */
    /***********************************************/

    if (streamingMode)
    {
        /* streaming (listen-only) mode */
        betaInv = AUTOMODE_STREAMING_BETA_INV_Q30;
    }
    else
    {
        /* normal mode */
        betaInv = AUTOMODE_BETA_INV_Q30;
    }

    /*******************************************************************/
    /* Calculate optimal buffer level without considering jitter peaks */
    /*******************************************************************/

    /*
     * Find the B for which the probability of observing an inter-arrival time larger
     * than or equal to B is less than or equal to betaInv.
     */
    B = 0; /* start from the beginning of iatProb */
    sum1 -= inst->iatProb[B]; /* ensure that optimal level is not less than 1 */

    do
    {
        /*
         * Subtract the probabilities one by one until the sum is no longer greater
         * than betaInv.
         */
        sum1 -= inst->iatProb[++B];
    }
    while ((sum1 > betaInv) && (B < MAX_IAT));

    Bopt = B; /* This is our primary value for the optimal buffer level Bopt */

    if (mdCodec)
    {
        /*
         * Use alternative cost function when multiple description codec is in use.
         * Do not have to re-calculate all points, just back off a few steps from
         * previous value of B.
         */
        WebRtc_Word32 sum2 = sum1; /* copy sum1 */

        while ((sum2 <= betaInv + inst->iatProb[Bopt]) && (Bopt > 0))
        {
            /* Go backwards in the sum until the modified cost function solution is found */
            sum2 += inst->iatProb[Bopt--];
        }

        Bopt++; /* This is the optimal level when using an MD codec */

        /* Now, Bopt and B can have different values. */
    }

#ifdef NETEQ_DELAY_LOGGING
    /* special code for offline delay logging */
    temp_var = NETEQ_DELAY_LOGGING_SIGNAL_OPTBUF;
    if (fwrite( &temp_var, sizeof(int), 1, delay_fid2 ) != 1) {
      return -1;
    }
    temp_var = (int) (Bopt * inst->packetSpeechLenSamp);
#endif

    /******************************************************************/
    /* Make levelFiltFact adaptive: Larger B <=> larger levelFiltFact */
    /******************************************************************/

    switch (B)
    {
        case 0:
        case 1:
        {
            inst->levelFiltFact = 251;
            break;
        }
        case 2:
        case 3:
        {
            inst->levelFiltFact = 252;
            break;
        }
        case 4:
        case 5:
        case 6:
        case 7:
        {
            inst->levelFiltFact = 253;
            break;
        }
        default: /* B > 7 */
        {
            inst->levelFiltFact = 254;
            break;
        }
    }

    /************************/
    /* Peak mode operations */
    /************************/

    /* Compare current IAT with peak threshold
     *
     * If IAT > optimal level + threshold (+1 for MD codecs)
     * or if IAT > 2 * optimal level (note: optimal level is in Q8):
     */
    if (timeIatPkts > (WebRtc_UWord32) (Bopt + inst->peakThresholdPkt + (mdCodec != 0))
        || timeIatPkts > (WebRtc_UWord32) WEBRTC_SPL_LSHIFT_U16(Bopt, 1))
    {
        /* A peak is observed */

        if (inst->peakIndex == -1)
        {
            /* this is the first peak; prepare for next peak */
            inst->peakIndex = 0;
            /* set the mode-disable counter */
            inst->peakModeDisabled = WEBRTC_SPL_LSHIFT_W16(1, NUM_PEAKS_REQUIRED-2);
        }
        else if (inst->peakIatCountSamp
            <=
            (WebRtc_UWord32) WEBRTC_SPL_MUL_32_16(fsHz, MAX_PEAK_PERIOD))
        {
            /* This is not the first peak and the period time is valid */

            /* store time elapsed since last peak */
            inst->peakPeriodSamp[inst->peakIndex] = inst->peakIatCountSamp;

            /* saturate height to 16 bits */
            inst->peakHeightPkt[inst->peakIndex]
                =
                (WebRtc_Word16) WEBRTC_SPL_MIN(timeIatPkts, WEBRTC_SPL_WORD16_MAX);

            /* increment peakIndex and wrap/modulo */
            inst->peakIndex = (inst->peakIndex + 1) & PEAK_INDEX_MASK;

            /* process peak vectors */
            inst->curPeakHeight = 0;
            inst->curPeakPeriod = 0;

            for (i = 0; i < NUM_PEAKS; i++)
            {
                /* Find maximum of peak heights and peak periods */
                inst->curPeakHeight
                    = WEBRTC_SPL_MAX(inst->curPeakHeight, inst->peakHeightPkt[i]);
                inst->curPeakPeriod
                    = WEBRTC_SPL_MAX(inst->curPeakPeriod, inst->peakPeriodSamp[i]);

            }

            inst->peakModeDisabled >>= 1; /* decrease mode-disable "counter" */

        }
        else if (inst->peakIatCountSamp > (WebRtc_UWord32) WEBRTC_SPL_MUL_32_16(fsHz,
            WEBRTC_SPL_LSHIFT_W16(MAX_PEAK_PERIOD, 1)))
        {
            /*
             * More than 2 * MAX_PEAK_PERIOD has elapsed since last peak;
             * too long time => reset peak statistics
             */
            inst->curPeakHeight = 0;
            inst->curPeakPeriod = 0;
            for (i = 0; i < NUM_PEAKS; i++)
            {
                inst->peakHeightPkt[i] = 0;
                inst->peakPeriodSamp[i] = 0;
            }

            inst->peakIndex = -1; /* Next peak is first peak */
            inst->peakIatCountSamp = 0;
        }

        inst->peakIatCountSamp = 0; /* Reset peak interval timer */
    } /* end if peak is observed */

    /* Evaluate peak mode conditions */

    /*
     * If not disabled (enough peaks have been observed) and
     * time since last peak is less than two peak periods.
     */
    inst->peakFound = 0;
    if ((!inst->peakModeDisabled) && (inst->peakIatCountSamp
        <= WEBRTC_SPL_LSHIFT_W32(inst->curPeakPeriod , 1)))
    {
        /* Engage peak mode */
        inst->peakFound = 1;
        /* Set optimal buffer level to curPeakHeight (if it's not already larger) */
        Bopt = WEBRTC_SPL_MAX(Bopt, inst->curPeakHeight);

#ifdef NETEQ_DELAY_LOGGING
        /* special code for offline delay logging */
        temp_var = (int) -(Bopt * inst->packetSpeechLenSamp);
#endif
    }

    /* Scale Bopt to Q8 */
    Bopt = WEBRTC_SPL_LSHIFT_U16(Bopt,8);

#ifdef NETEQ_DELAY_LOGGING
    /* special code for offline delay logging */
    if (fwrite( &temp_var, sizeof(int), 1, delay_fid2 ) != 1) {
      return -1;
    }
#endif

    /* Sanity check: Bopt must be strictly positive */
    if (Bopt <= 0)
    {
        Bopt = WEBRTC_SPL_LSHIFT_W16(1, 8); /* 1 in Q8 */
    }

    return Bopt; /* return value in Q8 */
}


int WebRtcNetEQ_BufferLevelFilter(WebRtc_Word32 curSizeMs8, AutomodeInst_t *inst,
                                  int sampPerCall, WebRtc_Word16 fsMult)
{

    WebRtc_Word16 curSizeFrames;

    /****************/
    /* Sanity check */
    /****************/

    if (sampPerCall <= 0 || fsMult <= 0)
    {
        /* sampPerCall and fsMult must both be strictly positive */
        return -1;
    }

    /* Check if packet size has been detected */
    if (inst->packetSpeechLenSamp > 0)
    {
        /*
         * Current buffer level in packet lengths
         * = (curSizeMs8 * fsMult) / packetSpeechLenSamp
         */
        curSizeFrames = (WebRtc_Word16) WebRtcSpl_DivW32W16(
            WEBRTC_SPL_MUL_32_16(curSizeMs8, fsMult), inst->packetSpeechLenSamp);
    }
    else
    {
        curSizeFrames = 0;
    }

    /* Filter buffer level */
    if (inst->levelFiltFact > 0) /* check that filter factor is set */
    {
        /* Filter:
         * buffLevelFilt = levelFiltFact * buffLevelFilt
         *                  + (1-levelFiltFact) * curSizeFrames
         *
         * levelFiltFact is in Q8
         */
        inst->buffLevelFilt = (WebRtc_UWord16) (WEBRTC_SPL_RSHIFT_W32(
            WEBRTC_SPL_MUL_16_U16(inst->levelFiltFact, inst->buffLevelFilt), 8)
            + WEBRTC_SPL_MUL_16_16(256 - inst->levelFiltFact, curSizeFrames));
    }

    /* Account for time-scale operations (accelerate and pre-emptive expand) */
    if (inst->prevTimeScale)
    {
        /*
         * Time-scaling has been performed since last filter update.
         * Subtract the sampleMemory from buffLevelFilt after converting sampleMemory
         * from samples to packets in Q8. Make sure that the filtered value is
         * non-negative.
         */
        inst->buffLevelFilt = (WebRtc_UWord16) WEBRTC_SPL_MAX( inst->buffLevelFilt -
            WebRtcSpl_DivW32W16(
                WEBRTC_SPL_LSHIFT_W32(inst->sampleMemory, 8), /* sampleMemory in Q8 */
                inst->packetSpeechLenSamp ), /* divide by packetSpeechLenSamp */
            0);

        /*
         * Reset flag and set timescaleHoldOff timer to prevent further time-scaling
         * for some time.
         */
        inst->prevTimeScale = 0;
        inst->timescaleHoldOff = AUTOMODE_TIMESCALE_LIMIT;
    }

    /* Update time counters and HoldOff timer */
    inst->packetIatCountSamp += sampPerCall; /* packet inter-arrival time */
    inst->peakIatCountSamp += sampPerCall; /* peak inter-arrival time */
    inst->timescaleHoldOff >>= 1; /* time-scaling limiter */
    inst->maxCSumUpdateTimer += sampPerCall; /* cumulative-sum timer */

    return 0;

}


int WebRtcNetEQ_SetPacketSpeechLen(AutomodeInst_t *inst, WebRtc_Word16 newLenSamp,
                                   WebRtc_Word32 fsHz)
{

    /* Sanity check for newLenSamp and fsHz */
    if (newLenSamp <= 0 || fsHz <= 0)
    {
        return -1;
    }

    inst->packetSpeechLenSamp = newLenSamp; /* Store packet size in instance */

    /* Make NetEQ wait for first regular packet before starting the timer */
    inst->lastPackCNGorDTMF = 1;

    inst->packetIatCountSamp = 0; /* Reset packet time counter */

    /*
     * Calculate peak threshold from packet size. The threshold is defined as
     * the (fractional) number of packets that corresponds to PEAK_HEIGHT
     * (in Q8 seconds). That is, threshold = PEAK_HEIGHT/256 * fsHz / packLen.
     */
    inst->peakThresholdPkt = (WebRtc_UWord16) WebRtcSpl_DivW32W16ResW16(
        WEBRTC_SPL_MUL_16_16_RSFT(PEAK_HEIGHT,
            (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(fsHz, 6), 2), inst->packetSpeechLenSamp);

    return 0;
}


int WebRtcNetEQ_ResetAutomode(AutomodeInst_t *inst, int maxBufLenPackets)
{

    int i;
    WebRtc_UWord16 tempprob = 0x4002; /* 16384 + 2 = 100000000000010 binary; */

    /* Sanity check for maxBufLenPackets */
    if (maxBufLenPackets <= 1)
    {
        /* Invalid value; set to 10 instead (arbitary small number) */
        maxBufLenPackets = 10;
    }

    /* Reset filtered buffer level */
    inst->buffLevelFilt = 0;

    /* Reset packet size to unknown */
    inst->packetSpeechLenSamp = 0;

    /*
     * Flag that last packet was special payload, so that automode will treat the next speech
     * payload as the first payload received.
     */
    inst->lastPackCNGorDTMF = 1;

    /* Reset peak detection parameters */
    inst->peakModeDisabled = 1; /* disable peak mode */
    inst->peakIatCountSamp = 0;
    inst->peakIndex = -1; /* indicates that no peak is registered */
    inst->curPeakHeight = 0;
    inst->curPeakPeriod = 0;
    for (i = 0; i < NUM_PEAKS; i++)
    {
        inst->peakHeightPkt[i] = 0;
        inst->peakPeriodSamp[i] = 0;
    }

    /*
     * Set the iatProb PDF vector to an exponentially decaying distribution
     * iatProb[i] = 0.5^(i+1), i = 0, 1, 2, ...
     * iatProb is in Q30.
     */
    for (i = 0; i <= MAX_IAT; i++)
    {
        /* iatProb[i] = 0.5^(i+1) = iatProb[i-1] / 2 */
        tempprob = WEBRTC_SPL_RSHIFT_U16(tempprob, 1);
        /* store in PDF vector */
        inst->iatProb[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32) tempprob, 16);
    }

    /*
     * Calculate the optimal buffer level corresponding to the initial PDF.
     * No need to call WebRtcNetEQ_CalcOptimalBufLvl() since we have just hard-coded
     * all the variables that the buffer level depends on => we know the result
     */
    inst->optBufLevel = WEBRTC_SPL_MIN(4,
        (maxBufLenPackets >> 1) + (maxBufLenPackets >> 1)); /* 75% of maxBufLenPackets */
    inst->levelFiltFact = 253;

    /*
     * Reset the iat update forgetting factor to 0 to make the impact of the first
     * incoming packets greater.
     */
    inst->iatProbFact = 0;

    /* Reset packet inter-arrival time counter */
    inst->packetIatCountSamp = 0;

    /* Clear time-scaling related variables */
    inst->prevTimeScale = 0;
    inst->timescaleHoldOff = AUTOMODE_TIMESCALE_LIMIT; /* don't allow time-scaling immediately */

    inst->cSumIatQ8 = 0;
    inst->maxCSumIatQ8 = 0;

    return 0;
}

int32_t WebRtcNetEQ_AverageIAT(const AutomodeInst_t *inst) {
  int i;
  int32_t sum_q24 = 0;
  assert(inst);
  for (i = 0; i <= MAX_IAT; ++i) {
    /* Shift 6 to fit worst case: 2^30 * 64. */
    sum_q24 += (inst->iatProb[i] >> 6) * i;
  }
  /* Subtract the nominal inter-arrival time 1 = 2^24 in Q24. */
  sum_q24 -= (1 << 24);
  /*
   * Multiply with 1000000 / 2^24 = 15625 / 2^18 to get in parts-per-million.
   * Shift 7 to Q17 first, then multiply with 15625 and shift another 11.
   */
  return ((sum_q24 >> 7) * 15625) >> 11;
}
