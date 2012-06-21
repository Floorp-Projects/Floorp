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
 * bandwidth_estimator.c
 *
 * This file contains the code for the Bandwidth Estimator designed
 * for iSAC.
 *
 * NOTE! Castings needed for C55, do not remove!
 *
 */

#include "bandwidth_estimator.h"
#include "settings.h"


/* array of quantization levels for bottle neck info; Matlab code: */
/* sprintf('%4.1ff, ', logspace(log10(5000), log10(40000), 12)) */
static const WebRtc_Word16 kQRateTable[12] = {
  10000, 11115, 12355, 13733, 15265, 16967,
  18860, 20963, 23301, 25900, 28789, 32000
};

/* 0.1 times the values in the table kQRateTable */
/* values are in Q16                                         */
static const WebRtc_Word32 KQRate01[12] = {
  65536000,  72843264,  80969728,  90000589,  100040704, 111194931,
  123600896, 137383117, 152705434, 169738240, 188671590, 209715200
};

/* Bits per Bytes Seconds
 * 8 bits/byte * 1000 msec/sec * 1/framelength (in msec)->bits/byte*sec
 * frame length will either be 30 or 60 msec. 8738 is 1/60 in Q19 and 1/30 in Q18
 * The following number is either in Q15 or Q14 depending on the current frame length */
static const WebRtc_Word32 kBitsByteSec = 4369000;

/* Received header rate. First value is for 30 ms packets and second for 60 ms */
static const WebRtc_Word16 kRecHeaderRate[2] = {
  9333, 4666
};

/* Inverted minimum and maximum bandwidth in Q30.
   minBwInv 30 ms, maxBwInv 30 ms,
   minBwInv 60 ms, maxBwInv 69 ms
*/
static const WebRtc_Word32 kInvBandwidth[4] = {
  55539, 25978,
  73213, 29284
};

/* Number of samples in 25 msec */
static const WebRtc_Word32 kSamplesIn25msec = 400;


/****************************************************************************
 * WebRtcIsacfix_InitBandwidthEstimator(...)
 *
 * This function initializes the struct for the bandwidth estimator
 *
 * Input/Output:
 *      - bweStr        : Struct containing bandwidth information.
 *
 * Return value            : 0
 */
WebRtc_Word32 WebRtcIsacfix_InitBandwidthEstimator(BwEstimatorstr *bweStr)
{
  bweStr->prevFrameSizeMs       = INIT_FRAME_LEN;
  bweStr->prevRtpNumber         = 0;
  bweStr->prevSendTime          = 0;
  bweStr->prevArrivalTime       = 0;
  bweStr->prevRtpRate           = 1;
  bweStr->lastUpdate            = 0;
  bweStr->lastReduction         = 0;
  bweStr->countUpdates          = -9;

  /* INIT_BN_EST = 20000
   * INIT_BN_EST_Q7 = 2560000
   * INIT_HDR_RATE = 4666
   * INIT_REC_BN_EST_Q5 = 789312
   *
   * recBwInv = 1/(INIT_BN_EST + INIT_HDR_RATE) in Q30
   * recBwAvg = INIT_BN_EST + INIT_HDR_RATE in Q5
   */
  bweStr->recBwInv              = 43531;
  bweStr->recBw                 = INIT_BN_EST;
  bweStr->recBwAvgQ             = INIT_BN_EST_Q7;
  bweStr->recBwAvg              = INIT_REC_BN_EST_Q5;
  bweStr->recJitter             = (WebRtc_Word32) 327680;   /* 10 in Q15 */
  bweStr->recJitterShortTerm    = 0;
  bweStr->recJitterShortTermAbs = (WebRtc_Word32) 40960;    /* 5 in Q13 */
  bweStr->recMaxDelay           = (WebRtc_Word32) 10;
  bweStr->recMaxDelayAvgQ       = (WebRtc_Word32) 5120;     /* 10 in Q9 */
  bweStr->recHeaderRate         = INIT_HDR_RATE;
  bweStr->countRecPkts          = 0;
  bweStr->sendBwAvg             = INIT_BN_EST_Q7;
  bweStr->sendMaxDelayAvg       = (WebRtc_Word32) 5120;     /* 10 in Q9 */

  bweStr->countHighSpeedRec     = 0;
  bweStr->highSpeedRec          = 0;
  bweStr->countHighSpeedSent    = 0;
  bweStr->highSpeedSend         = 0;
  bweStr->inWaitPeriod          = 0;

  /* Find the inverse of the max bw and min bw in Q30
   *  (1 / (MAX_ISAC_BW + INIT_HDR_RATE) in Q30
   *  (1 / (MIN_ISAC_BW + INIT_HDR_RATE) in Q30
   */
  bweStr->maxBwInv              = kInvBandwidth[3];
  bweStr->minBwInv              = kInvBandwidth[2];

  return 0;
}

/****************************************************************************
 * WebRtcIsacfix_UpdateUplinkBwImpl(...)
 *
 * This function updates bottle neck rate received from other side in payload
 * and calculates a new bottle neck to send to the other side.
 *
 * Input/Output:
 *      - bweStr           : struct containing bandwidth information.
 *      - rtpNumber        : value from RTP packet, from NetEq
 *      - frameSize        : length of signal frame in ms, from iSAC decoder
 *      - sendTime         : value in RTP header giving send time in samples
 *      - arrivalTime      : value given by timeGetTime() time of arrival in
 *                           samples of packet from NetEq
 *      - pksize           : size of packet in bytes, from NetEq
 *      - Index            : integer (range 0...23) indicating bottle neck &
 *                           jitter as estimated by other side
 *
 * Return value            : 0 if everything went fine,
 *                           -1 otherwise
 */
WebRtc_Word32 WebRtcIsacfix_UpdateUplinkBwImpl(BwEstimatorstr *bweStr,
                                               const WebRtc_UWord16 rtpNumber,
                                               const WebRtc_Word16  frameSize,
                                               const WebRtc_UWord32 sendTime,
                                               const WebRtc_UWord32 arrivalTime,
                                               const WebRtc_Word16  pksize,
                                               const WebRtc_UWord16 Index)
{
  WebRtc_UWord16  weight = 0;
  WebRtc_UWord32  currBwInv = 0;
  WebRtc_UWord16  recRtpRate;
  WebRtc_UWord32  arrTimeProj;
  WebRtc_Word32   arrTimeDiff;
  WebRtc_Word32   arrTimeNoise;
  WebRtc_Word32   arrTimeNoiseAbs;
  WebRtc_Word32   sendTimeDiff;

  WebRtc_Word32 delayCorrFactor = DELAY_CORRECTION_MED;
  WebRtc_Word32 lateDiff = 0;
  WebRtc_Word16 immediateSet = 0;
  WebRtc_Word32 frameSizeSampl;

  WebRtc_Word32  temp;
  WebRtc_Word32  msec;
  WebRtc_UWord32 exponent;
  WebRtc_UWord32 reductionFactor;
  WebRtc_UWord32 numBytesInv;
  WebRtc_Word32  sign;

  WebRtc_UWord32 byteSecondsPerBit;
  WebRtc_UWord32 tempLower;
  WebRtc_UWord32 tempUpper;
  WebRtc_Word32 recBwAvgInv;
  WebRtc_Word32 numPktsExpected;

  WebRtc_Word16 errCode;

  /* UPDATE ESTIMATES FROM OTHER SIDE */

  /* The function also checks if Index has a valid value */
  errCode = WebRtcIsacfix_UpdateUplinkBwRec(bweStr, Index);
  if (errCode <0) {
    return(errCode);
  }


  /* UPDATE ESTIMATES ON THIS SIDE */

  /* Bits per second per byte * 1/30 or 1/60 */
  if (frameSize == 60) {
    /* If frameSize changed since last call, from 30 to 60, recalculate some values */
    if ( (frameSize != bweStr->prevFrameSizeMs) && (bweStr->countUpdates > 0)) {
      bweStr->countUpdates = 10;
      bweStr->recHeaderRate = kRecHeaderRate[1];

      bweStr->maxBwInv = kInvBandwidth[3];
      bweStr->minBwInv = kInvBandwidth[2];
      bweStr->recBwInv = WEBRTC_SPL_UDIV(1073741824, (bweStr->recBw + bweStr->recHeaderRate));
    }

    /* kBitsByteSec is in Q15 */
    recRtpRate = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(kBitsByteSec,
                                                                     (WebRtc_Word32)pksize), 15) + bweStr->recHeaderRate;

  } else {
    /* If frameSize changed since last call, from 60 to 30, recalculate some values */
    if ( (frameSize != bweStr->prevFrameSizeMs) && (bweStr->countUpdates > 0)) {
      bweStr->countUpdates = 10;
      bweStr->recHeaderRate = kRecHeaderRate[0];

      bweStr->maxBwInv = kInvBandwidth[1];
      bweStr->minBwInv = kInvBandwidth[0];
      bweStr->recBwInv = WEBRTC_SPL_UDIV(1073741824, (bweStr->recBw + bweStr->recHeaderRate));
    }

    /* kBitsByteSec is in Q14 */
    recRtpRate = (WebRtc_UWord16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(kBitsByteSec,
                                                                      (WebRtc_Word32)pksize), 14) + bweStr->recHeaderRate;
  }


  /* Check for timer wrap-around */
  if (arrivalTime < bweStr->prevArrivalTime) {
    bweStr->prevArrivalTime = arrivalTime;
    bweStr->lastUpdate      = arrivalTime;
    bweStr->lastReduction   = arrivalTime + FS3;

    bweStr->countRecPkts      = 0;

    /* store frame size */
    bweStr->prevFrameSizeMs = frameSize;

    /* store far-side transmission rate */
    bweStr->prevRtpRate = recRtpRate;

    /* store far-side RTP time stamp */
    bweStr->prevRtpNumber = rtpNumber;

    return 0;
  }

  bweStr->countRecPkts++;

  /* Calculate framesize in msec */
  frameSizeSampl = WEBRTC_SPL_MUL_16_16((WebRtc_Word16)SAMPLES_PER_MSEC, frameSize);

  /* Check that it's not one of the first 9 packets */
  if ( bweStr->countUpdates > 0 ) {

    /* Stay in Wait Period for 1.5 seconds (no updates in wait period) */
    if(bweStr->inWaitPeriod) {
      if ((arrivalTime - bweStr->startWaitPeriod)> FS_1_HALF) {
        bweStr->inWaitPeriod = 0;
      }
    }

    /* If not been updated for a long time, reduce the BN estimate */

    /* Check send time difference between this packet and previous received      */
    sendTimeDiff = sendTime - bweStr->prevSendTime;
    if (sendTimeDiff <= WEBRTC_SPL_LSHIFT_W32(frameSizeSampl, 1)) {

      /* Only update if 3 seconds has past since last update */
      if ((arrivalTime - bweStr->lastUpdate) > FS3) {

        /* Calculate expected number of received packets since last update */
        numPktsExpected =  WEBRTC_SPL_UDIV(arrivalTime - bweStr->lastUpdate, frameSizeSampl);

        /* If received number of packets is more than 90% of expected (922 = 0.9 in Q10): */
        /* do the update, else not                                                        */
        if(WEBRTC_SPL_LSHIFT_W32(bweStr->countRecPkts, 10)  > WEBRTC_SPL_MUL_16_16(922, numPktsExpected)) {
          /* Q4 chosen to approx dividing by 16 */
          msec = (arrivalTime - bweStr->lastReduction);

          /* the number below represents 13 seconds, highly unlikely
             but to insure no overflow when reduction factor is multiplied by recBw inverse */
          if (msec > 208000) {
            msec = 208000;
          }

          /* Q20 2^(negative number: - 76/1048576) = .99995
             product is Q24 */
          exponent = WEBRTC_SPL_UMUL(0x0000004C, msec);

          /* do the approx with positive exponent so that value is actually rf^-1
             and multiply by bw inverse */
          reductionFactor = WEBRTC_SPL_RSHIFT_U32(0x01000000 | (exponent & 0x00FFFFFF),
                                                  WEBRTC_SPL_RSHIFT_U32(exponent, 24));

          /* reductionFactor in Q13 */
          reductionFactor = WEBRTC_SPL_RSHIFT_U32(reductionFactor, 11);

          if ( reductionFactor != 0 ) {
            bweStr->recBwInv = WEBRTC_SPL_MUL((WebRtc_Word32)bweStr->recBwInv, (WebRtc_Word32)reductionFactor);
            bweStr->recBwInv = WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32)bweStr->recBwInv, 13);

          } else {
            /* recBwInv = 1 / (INIT_BN_EST + INIT_HDR_RATE) in Q26 (Q30??)*/
            bweStr->recBwInv = WEBRTC_SPL_DIV((1073741824 +
                                               WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)INIT_BN_EST + INIT_HDR_RATE), 1)), INIT_BN_EST + INIT_HDR_RATE);
          }

          /* reset time-since-update counter */
          bweStr->lastReduction = arrivalTime;
        } else {
          /* Delay last reduction with 3 seconds */
          bweStr->lastReduction = arrivalTime + FS3;
          bweStr->lastUpdate    = arrivalTime;
          bweStr->countRecPkts  = 0;
        }
      }
    } else {
      bweStr->lastReduction = arrivalTime + FS3;
      bweStr->lastUpdate    = arrivalTime;
      bweStr->countRecPkts  = 0;
    }


    /*   update only if previous packet was not lost */
    if ( rtpNumber == bweStr->prevRtpNumber + 1 ) {
      arrTimeDiff = arrivalTime - bweStr->prevArrivalTime;

      if (!(bweStr->highSpeedSend && bweStr->highSpeedRec)) {
        if (arrTimeDiff > frameSizeSampl) {
          if (sendTimeDiff > 0) {
            lateDiff = arrTimeDiff - sendTimeDiff -
                WEBRTC_SPL_LSHIFT_W32(frameSizeSampl, 1);
          } else {
            lateDiff = arrTimeDiff - frameSizeSampl;
          }

          /* 8000 is 1/2 second (in samples at FS) */
          if (lateDiff > 8000) {
            delayCorrFactor = (WebRtc_Word32) DELAY_CORRECTION_MAX;
            bweStr->inWaitPeriod = 1;
            bweStr->startWaitPeriod = arrivalTime;
            immediateSet = 1;
          } else if (lateDiff > 5120) {
            delayCorrFactor = (WebRtc_Word32) DELAY_CORRECTION_MED;
            immediateSet = 1;
            bweStr->inWaitPeriod = 1;
            bweStr->startWaitPeriod = arrivalTime;
          }
        }
      }

      if ((bweStr->prevRtpRate > WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32) bweStr->recBwAvg, 5)) &&
          (recRtpRate > WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32)bweStr->recBwAvg, 5)) &&
          !bweStr->inWaitPeriod) {

        /* test if still in initiation period and increment counter */
        if (bweStr->countUpdates++ > 99) {
          /* constant weight after initiation part, 0.01 in Q13 */
          weight = (WebRtc_UWord16) 82;
        } else {
          /* weight decreases with number of updates, 1/countUpdates in Q13  */
          weight = (WebRtc_UWord16) WebRtcSpl_DivW32W16(
              (WebRtc_Word32)(8192 + WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32) bweStr->countUpdates, 1)),
              (WebRtc_Word16)bweStr->countUpdates);
        }

        /* Bottle Neck Estimation */

        /* limit outliers, if more than 25 ms too much */
        if (arrTimeDiff > frameSizeSampl + kSamplesIn25msec) {
          arrTimeDiff = frameSizeSampl + kSamplesIn25msec;
        }

        /* don't allow it to be less than frame rate - 10 ms */
        if (arrTimeDiff < frameSizeSampl - FRAMESAMPLES_10ms) {
          arrTimeDiff = frameSizeSampl - FRAMESAMPLES_10ms;
        }

        /* compute inverse receiving rate for last packet, in Q19 */
        numBytesInv = (WebRtc_UWord16) WebRtcSpl_DivW32W16(
            (WebRtc_Word32)(524288 + WEBRTC_SPL_RSHIFT_W32(((WebRtc_Word32)pksize + HEADER_SIZE), 1)),
            (WebRtc_Word16)(pksize + HEADER_SIZE));

        /* 8389 is  ~ 1/128000 in Q30 */
        byteSecondsPerBit = WEBRTC_SPL_MUL_16_16(arrTimeDiff, 8389);

        /* get upper N bits */
        tempUpper = WEBRTC_SPL_RSHIFT_U32(byteSecondsPerBit, 15);

        /* get lower 15 bits */
        tempLower = byteSecondsPerBit & 0x00007FFF;

        tempUpper = WEBRTC_SPL_MUL(tempUpper, numBytesInv);
        tempLower = WEBRTC_SPL_MUL(tempLower, numBytesInv);
        tempLower = WEBRTC_SPL_RSHIFT_U32(tempLower, 15);

        currBwInv = tempUpper + tempLower;
        currBwInv = WEBRTC_SPL_RSHIFT_U32(currBwInv, 4);

        /* Limit inv rate. Note that minBwInv > maxBwInv! */
        if(currBwInv < bweStr->maxBwInv) {
          currBwInv = bweStr->maxBwInv;
        } else if(currBwInv > bweStr->minBwInv) {
          currBwInv = bweStr->minBwInv;
        }

        /* update bottle neck rate estimate */
        bweStr->recBwInv = WEBRTC_SPL_UMUL(weight, currBwInv) +
            WEBRTC_SPL_UMUL((WebRtc_UWord32) 8192 - weight, bweStr->recBwInv);

        /* Shift back to Q30 from Q40 (actual used bits shouldn't be more than 27 based on minBwInv)
           up to 30 bits used with Q13 weight */
        bweStr->recBwInv = WEBRTC_SPL_RSHIFT_U32(bweStr->recBwInv, 13);

        /* reset time-since-update counter */
        bweStr->lastUpdate    = arrivalTime;
        bweStr->lastReduction = arrivalTime + FS3;
        bweStr->countRecPkts  = 0;

        /* to save resolution compute the inverse of recBwAvg in Q26 by left shifting numerator to 2^31
           and NOT right shifting recBwAvg 5 bits to an integer
           At max 13 bits are used
           shift to Q5 */
        recBwAvgInv = WEBRTC_SPL_UDIV((WebRtc_UWord32)(0x80000000 + WEBRTC_SPL_RSHIFT_U32(bweStr->recBwAvg, 1)),
                                      bweStr->recBwAvg);

        /* Calculate Projected arrival time difference */

        /* The numerator of the quotient can be 22 bits so right shift inv by 4 to avoid overflow
           result in Q22 */
        arrTimeProj = WEBRTC_SPL_MUL((WebRtc_Word32)8000, recBwAvgInv);
        /* shift to Q22 */
        arrTimeProj = WEBRTC_SPL_RSHIFT_U32(arrTimeProj, 4);
        /* complete calulation */
        arrTimeProj = WEBRTC_SPL_MUL(((WebRtc_Word32)pksize + HEADER_SIZE), arrTimeProj);
        /* shift to Q10 */
        arrTimeProj = WEBRTC_SPL_RSHIFT_U32(arrTimeProj, 12);

        /* difference between projected and actual arrival time differences */
        /* Q9 (only shift arrTimeDiff by 5 to simulate divide by 16 (need to revisit if change sampling rate) DH */
        if (WEBRTC_SPL_LSHIFT_W32(arrTimeDiff, 6) > (WebRtc_Word32)arrTimeProj) {
          arrTimeNoise = WEBRTC_SPL_LSHIFT_W32(arrTimeDiff, 6) -  arrTimeProj;
          sign = 1;
        } else {
          arrTimeNoise = arrTimeProj - WEBRTC_SPL_LSHIFT_W32(arrTimeDiff, 6);
          sign = -1;
        }

        /* Q9 */
        arrTimeNoiseAbs = arrTimeNoise;

        /* long term averaged absolute jitter, Q15 */
        weight = WEBRTC_SPL_RSHIFT_W32(weight, 3);
        bweStr->recJitter = WEBRTC_SPL_MUL(weight, WEBRTC_SPL_LSHIFT_W32(arrTimeNoiseAbs, 5))
            +  WEBRTC_SPL_MUL(1024 - weight, bweStr->recJitter);

        /* remove the fractional portion */
        bweStr->recJitter = WEBRTC_SPL_RSHIFT_W32(bweStr->recJitter, 10);

        /* Maximum jitter is 10 msec in Q15 */
        if (bweStr->recJitter > (WebRtc_Word32)327680) {
          bweStr->recJitter = (WebRtc_Word32)327680;
        }

        /* short term averaged absolute jitter */
        /* Calculation in Q13 products in Q23 */
        bweStr->recJitterShortTermAbs = WEBRTC_SPL_MUL(51, WEBRTC_SPL_LSHIFT_W32(arrTimeNoiseAbs, 3)) +
            WEBRTC_SPL_MUL(973, bweStr->recJitterShortTermAbs);
        bweStr->recJitterShortTermAbs = WEBRTC_SPL_RSHIFT_W32(bweStr->recJitterShortTermAbs , 10);

        /* short term averaged jitter */
        /* Calculation in Q13 products in Q23 */
        bweStr->recJitterShortTerm = WEBRTC_SPL_MUL(205, WEBRTC_SPL_LSHIFT_W32(arrTimeNoise, 3)) * sign +
            WEBRTC_SPL_MUL(3891, bweStr->recJitterShortTerm);

        if (bweStr->recJitterShortTerm < 0) {
          temp = -bweStr->recJitterShortTerm;
          temp = WEBRTC_SPL_RSHIFT_W32(temp, 12);
          bweStr->recJitterShortTerm = -temp;
        } else {
          bweStr->recJitterShortTerm = WEBRTC_SPL_RSHIFT_W32(bweStr->recJitterShortTerm, 12);
        }
      }
    }
  } else {
    /* reset time-since-update counter when receiving the first 9 packets */
    bweStr->lastUpdate    = arrivalTime;
    bweStr->lastReduction = arrivalTime + FS3;
    bweStr->countRecPkts  = 0;
    bweStr->countUpdates++;
  }

  /* Limit to minimum or maximum bottle neck rate (in Q30) */
  if (bweStr->recBwInv > bweStr->minBwInv) {
    bweStr->recBwInv = bweStr->minBwInv;
  } else if (bweStr->recBwInv < bweStr->maxBwInv) {
    bweStr->recBwInv = bweStr->maxBwInv;
  }


  /* store frame length */
  bweStr->prevFrameSizeMs = frameSize;

  /* store far-side transmission rate */
  bweStr->prevRtpRate = recRtpRate;

  /* store far-side RTP time stamp */
  bweStr->prevRtpNumber = rtpNumber;

  /* Replace bweStr->recMaxDelay by the new value (atomic operation) */
  if (bweStr->prevArrivalTime != 0xffffffff) {
    bweStr->recMaxDelay = WEBRTC_SPL_MUL(3, bweStr->recJitter);
  }

  /* store arrival time stamp */
  bweStr->prevArrivalTime = arrivalTime;
  bweStr->prevSendTime = sendTime;

  /* Replace bweStr->recBw by the new value */
  bweStr->recBw = WEBRTC_SPL_UDIV(1073741824, bweStr->recBwInv) - bweStr->recHeaderRate;

  if (immediateSet) {
    /* delay correction factor is in Q10 */
    bweStr->recBw = WEBRTC_SPL_UMUL(delayCorrFactor, bweStr->recBw);
    bweStr->recBw = WEBRTC_SPL_RSHIFT_U32(bweStr->recBw, 10);

    if (bweStr->recBw < (WebRtc_Word32) MIN_ISAC_BW) {
      bweStr->recBw = (WebRtc_Word32) MIN_ISAC_BW;
    }

    bweStr->recBwAvg = WEBRTC_SPL_LSHIFT_U32(bweStr->recBw + bweStr->recHeaderRate, 5);

    bweStr->recBwAvgQ = WEBRTC_SPL_LSHIFT_U32(bweStr->recBw, 7);

    bweStr->recJitterShortTerm = 0;

    bweStr->recBwInv = WEBRTC_SPL_UDIV(1073741824, bweStr->recBw + bweStr->recHeaderRate);

    immediateSet = 0;
  }


  return 0;
}

/* This function updates the send bottle neck rate                                                   */
/* Index         - integer (range 0...23) indicating bottle neck & jitter as estimated by other side */
/* returns 0 if everything went fine, -1 otherwise                                                   */
WebRtc_Word16 WebRtcIsacfix_UpdateUplinkBwRec(BwEstimatorstr *bweStr,
                                              const WebRtc_Word16 Index)
{
  WebRtc_UWord16 RateInd;

  if ( (Index < 0) || (Index > 23) ) {
    return -ISAC_RANGE_ERROR_BW_ESTIMATOR;
  }

  /* UPDATE ESTIMATES FROM OTHER SIDE */

  if ( Index > 11 ) {
    RateInd = Index - 12;
    /* compute the jitter estimate as decoded on the other side in Q9 */
    /* sendMaxDelayAvg = 0.9 * sendMaxDelayAvg + 0.1 * MAX_ISAC_MD */
    bweStr->sendMaxDelayAvg = WEBRTC_SPL_MUL(461, bweStr->sendMaxDelayAvg) +
        WEBRTC_SPL_MUL(51, WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)MAX_ISAC_MD, 9));
    bweStr->sendMaxDelayAvg = WEBRTC_SPL_RSHIFT_W32(bweStr->sendMaxDelayAvg, 9);

  } else {
    RateInd = Index;
    /* compute the jitter estimate as decoded on the other side in Q9 */
    /* sendMaxDelayAvg = 0.9 * sendMaxDelayAvg + 0.1 * MIN_ISAC_MD */
    bweStr->sendMaxDelayAvg = WEBRTC_SPL_MUL(461, bweStr->sendMaxDelayAvg) +
        WEBRTC_SPL_MUL(51, WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)MIN_ISAC_MD,9));
    bweStr->sendMaxDelayAvg = WEBRTC_SPL_RSHIFT_W32(bweStr->sendMaxDelayAvg, 9);

  }


  /* compute the BN estimate as decoded on the other side */
  /* sendBwAvg = 0.9 * sendBwAvg + 0.1 * kQRateTable[RateInd]; */
  bweStr->sendBwAvg = WEBRTC_SPL_UMUL(461, bweStr->sendBwAvg) +
      WEBRTC_SPL_UMUL(51, WEBRTC_SPL_LSHIFT_U32(kQRateTable[RateInd], 7));
  bweStr->sendBwAvg = WEBRTC_SPL_RSHIFT_U32(bweStr->sendBwAvg, 9);


  if (WEBRTC_SPL_RSHIFT_U32(bweStr->sendBwAvg, 7) > 28000 && !bweStr->highSpeedSend) {
    bweStr->countHighSpeedSent++;

    /* approx 2 seconds with 30ms frames */
    if (bweStr->countHighSpeedSent >= 66) {
      bweStr->highSpeedSend = 1;
    }
  } else if (!bweStr->highSpeedSend) {
    bweStr->countHighSpeedSent = 0;
  }

  return 0;
}

/****************************************************************************
 * WebRtcIsacfix_GetDownlinkBwIndexImpl(...)
 *
 * This function calculates and returns the bandwidth/jitter estimation code
 * (integer 0...23) to put in the sending iSAC payload.
 *
 * Input:
 *      - bweStr       : BWE struct
 *
 * Return:
 *      bandwith and jitter index (0..23)
 */
WebRtc_UWord16 WebRtcIsacfix_GetDownlinkBwIndexImpl(BwEstimatorstr *bweStr)
{
  WebRtc_Word32  rate;
  WebRtc_Word32  maxDelay;
  WebRtc_UWord16 rateInd;
  WebRtc_UWord16 maxDelayBit;
  WebRtc_Word32  tempTerm1;
  WebRtc_Word32  tempTerm2;
  WebRtc_Word32  tempTermX;
  WebRtc_Word32  tempTermY;
  WebRtc_Word32  tempMin;
  WebRtc_Word32  tempMax;

  /* Get Rate Index */

  /* Get unquantized rate. Always returns 10000 <= rate <= 32000 */
  rate = WebRtcIsacfix_GetDownlinkBandwidth(bweStr);

  /* Compute the averaged BN estimate on this side */

  /* recBwAvg = 0.9 * recBwAvg + 0.1 * (rate + bweStr->recHeaderRate), 0.9 and 0.1 in Q9 */
  bweStr->recBwAvg = WEBRTC_SPL_UMUL(922, bweStr->recBwAvg) +
      WEBRTC_SPL_UMUL(102, WEBRTC_SPL_LSHIFT_U32((WebRtc_UWord32)rate + bweStr->recHeaderRate, 5));
  bweStr->recBwAvg = WEBRTC_SPL_RSHIFT_U32(bweStr->recBwAvg, 10);

  /* find quantization index that gives the closest rate after averaging */
  for (rateInd = 1; rateInd < 12; rateInd++) {
    if (rate <= kQRateTable[rateInd]){
      break;
    }
  }

  /* find closest quantization index, and update quantized average by taking: */
  /* 0.9*recBwAvgQ + 0.1*kQRateTable[rateInd] */

  /* 0.9 times recBwAvgQ in Q16 */
  /* 461/512 - 25/65536 =0.900009 */
  tempTerm1 = WEBRTC_SPL_MUL(bweStr->recBwAvgQ, 25);
  tempTerm1 = WEBRTC_SPL_RSHIFT_W32(tempTerm1, 7);
  tempTermX = WEBRTC_SPL_UMUL(461, bweStr->recBwAvgQ) - tempTerm1;

  /* rate in Q16 */
  tempTermY = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)rate, 16);

  /* 0.1 * kQRateTable[rateInd] = KQRate01[rateInd] */
  tempTerm1 = tempTermX + KQRate01[rateInd] - tempTermY;
  tempTerm2 = tempTermY - tempTermX - KQRate01[rateInd-1];

  /* Compare (0.9 * recBwAvgQ + 0.1 * kQRateTable[rateInd] - rate) >
     (rate - 0.9 * recBwAvgQ - 0.1 * kQRateTable[rateInd-1]) */
  if (tempTerm1  > tempTerm2) {
    rateInd--;
  }

  /* Update quantized average by taking:                  */
  /* 0.9*recBwAvgQ + 0.1*kQRateTable[rateInd] */

  /* Add 0.1 times kQRateTable[rateInd], in Q16 */
  tempTermX += KQRate01[rateInd];

  /* Shift back to Q7 */
  bweStr->recBwAvgQ = WEBRTC_SPL_RSHIFT_W32(tempTermX, 9);

  /* Count consecutive received bandwidth above 28000 kbps (28000 in Q7 = 3584000) */
  /* If 66 high estimates in a row, set highSpeedRec to one */
  /* 66 corresponds to ~2 seconds in 30 msec mode */
  if ((bweStr->recBwAvgQ > 3584000) && !bweStr->highSpeedRec) {
    bweStr->countHighSpeedRec++;
    if (bweStr->countHighSpeedRec >= 66) {
      bweStr->highSpeedRec = 1;
    }
  } else if (!bweStr->highSpeedRec)    {
    bweStr->countHighSpeedRec = 0;
  }

  /* Get Max Delay Bit */

  /* get unquantized max delay */
  maxDelay = WebRtcIsacfix_GetDownlinkMaxDelay(bweStr);

  /* Update quantized max delay average */
  tempMax = 652800; /* MAX_ISAC_MD * 0.1 in Q18 */
  tempMin = 130560; /* MIN_ISAC_MD * 0.1 in Q18 */
  tempTermX = WEBRTC_SPL_MUL((WebRtc_Word32)bweStr->recMaxDelayAvgQ, (WebRtc_Word32)461);
  tempTermY = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)maxDelay, 18);

  tempTerm1 = tempTermX + tempMax - tempTermY;
  tempTerm2 = tempTermY - tempTermX - tempMin;

  if ( tempTerm1 > tempTerm2) {
    maxDelayBit = 0;
    tempTerm1 = tempTermX + tempMin;

    /* update quantized average, shift back to Q9 */
    bweStr->recMaxDelayAvgQ = WEBRTC_SPL_RSHIFT_W32(tempTerm1, 9);
  } else {
    maxDelayBit = 12;
    tempTerm1 =  tempTermX + tempMax;

    /* update quantized average, shift back to Q9 */
    bweStr->recMaxDelayAvgQ = WEBRTC_SPL_RSHIFT_W32(tempTerm1, 9);
  }

  /* Return bandwitdh and jitter index (0..23) */
  return (WebRtc_UWord16)(rateInd + maxDelayBit);
}

/* get the bottle neck rate from far side to here, as estimated on this side */
WebRtc_UWord16 WebRtcIsacfix_GetDownlinkBandwidth(const BwEstimatorstr *bweStr)
{
  WebRtc_UWord32  recBw;
  WebRtc_Word32   jitter_sign; /* Q8 */
  WebRtc_Word32   bw_adjust;   /* Q16 */
  WebRtc_Word32   rec_jitter_short_term_abs_inv; /* Q18 */
  WebRtc_Word32   temp;

  /* Q18  rec jitter short term abs is in Q13, multiply it by 2^13 to save precision
     2^18 then needs to be shifted 13 bits to 2^31 */
  rec_jitter_short_term_abs_inv = WEBRTC_SPL_UDIV(0x80000000, bweStr->recJitterShortTermAbs);

  /* Q27 = 9 + 18 */
  jitter_sign = WEBRTC_SPL_MUL(WEBRTC_SPL_RSHIFT_W32(bweStr->recJitterShortTerm, 4), (WebRtc_Word32)rec_jitter_short_term_abs_inv);

  if (jitter_sign < 0) {
    temp = -jitter_sign;
    temp = WEBRTC_SPL_RSHIFT_W32(temp, 19);
    jitter_sign = -temp;
  } else {
    jitter_sign = WEBRTC_SPL_RSHIFT_W32(jitter_sign, 19);
  }

  /* adjust bw proportionally to negative average jitter sign */
  //bw_adjust = 1.0f - jitter_sign * (0.15f + 0.15f * jitter_sign * jitter_sign);
  //Q8 -> Q16 .15 +.15 * jitter^2 first term is .15 in Q16 latter term is Q8*Q8*Q8
  //38 in Q8 ~.15 9830 in Q16 ~.15
  temp = 9830  + WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL(38, WEBRTC_SPL_MUL(jitter_sign, jitter_sign))), 8);

  if (jitter_sign < 0) {
    temp = WEBRTC_SPL_MUL(jitter_sign, temp);
    temp = -temp;
    temp = WEBRTC_SPL_RSHIFT_W32(temp, 8);
    bw_adjust = (WebRtc_UWord32)65536 + temp; /* (1 << 16) + temp; */
  } else {
    bw_adjust = (WebRtc_UWord32)65536 - WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(jitter_sign, temp), 8);/* (1 << 16) - ((jitter_sign * temp) >> 8); */
  }

  //make sure following multiplication won't overflow
  //bw adjust now Q14
  bw_adjust = WEBRTC_SPL_RSHIFT_W32(bw_adjust, 2);//see if good resolution is maintained

  /* adjust Rate if jitter sign is mostly constant */
  recBw = WEBRTC_SPL_UMUL(bweStr->recBw, bw_adjust);

  recBw = WEBRTC_SPL_RSHIFT_W32(recBw, 14);

  /* limit range of bottle neck rate */
  if (recBw < MIN_ISAC_BW) {
    recBw = MIN_ISAC_BW;
  } else if (recBw > MAX_ISAC_BW) {
    recBw = MAX_ISAC_BW;
  }

  return  (WebRtc_UWord16) recBw;
}

/* Returns the mmax delay (in ms) */
WebRtc_Word16 WebRtcIsacfix_GetDownlinkMaxDelay(const BwEstimatorstr *bweStr)
{
  WebRtc_Word16 recMaxDelay;

  recMaxDelay = (WebRtc_Word16)  WEBRTC_SPL_RSHIFT_W32(bweStr->recMaxDelay, 15);

  /* limit range of jitter estimate */
  if (recMaxDelay < MIN_ISAC_MD) {
    recMaxDelay = MIN_ISAC_MD;
  } else if (recMaxDelay > MAX_ISAC_MD) {
    recMaxDelay = MAX_ISAC_MD;
  }

  return recMaxDelay;
}

/* get the bottle neck rate from here to far side, as estimated by far side */
WebRtc_Word16 WebRtcIsacfix_GetUplinkBandwidth(const BwEstimatorstr *bweStr)
{
  WebRtc_Word16 send_bw;

  send_bw = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_U32(bweStr->sendBwAvg, 7);

  /* limit range of bottle neck rate */
  if (send_bw < MIN_ISAC_BW) {
    send_bw = MIN_ISAC_BW;
  } else if (send_bw > MAX_ISAC_BW) {
    send_bw = MAX_ISAC_BW;
  }

  return send_bw;
}



/* Returns the max delay value from the other side in ms */
WebRtc_Word16 WebRtcIsacfix_GetUplinkMaxDelay(const BwEstimatorstr *bweStr)
{
  WebRtc_Word16 send_max_delay;

  send_max_delay = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(bweStr->sendMaxDelayAvg, 9);

  /* limit range of jitter estimate */
  if (send_max_delay < MIN_ISAC_MD) {
    send_max_delay = MIN_ISAC_MD;
  } else if (send_max_delay > MAX_ISAC_MD) {
    send_max_delay = MAX_ISAC_MD;
  }

  return send_max_delay;
}




/*
 * update long-term average bitrate and amount of data in buffer
 * returns minimum payload size (bytes)
 */
WebRtc_UWord16 WebRtcIsacfix_GetMinBytes(RateModel *State,
                                         WebRtc_Word16 StreamSize,                    /* bytes in bitstream */
                                         const WebRtc_Word16 FrameSamples,            /* samples per frame */
                                         const WebRtc_Word16 BottleNeck,        /* bottle neck rate; excl headers (bps) */
                                         const WebRtc_Word16 DelayBuildUp)      /* max delay from bottle neck buffering (ms) */
{
  WebRtc_Word32 MinRate = 0;
  WebRtc_UWord16    MinBytes;
  WebRtc_Word16 TransmissionTime;
  WebRtc_Word32 inv_Q12;
  WebRtc_Word32 den;


  /* first 10 packets @ low rate, then INIT_BURST_LEN packets @ fixed rate of INIT_RATE bps */
  if (State->InitCounter > 0) {
    if (State->InitCounter-- <= INIT_BURST_LEN) {
      MinRate = INIT_RATE;
    } else {
      MinRate = 0;
    }
  } else {
    /* handle burst */
    if (State->BurstCounter) {
      if (State->StillBuffered < WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL((512 - WEBRTC_SPL_DIV(512, BURST_LEN)), DelayBuildUp), 9)) {
        /* max bps derived from BottleNeck and DelayBuildUp values */
        inv_Q12 = WEBRTC_SPL_DIV(4096, WEBRTC_SPL_MUL(BURST_LEN, FrameSamples));
        MinRate = WEBRTC_SPL_MUL(512 + WEBRTC_SPL_MUL(SAMPLES_PER_MSEC, WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(DelayBuildUp, inv_Q12), 3)), BottleNeck);
      } else {
        /* max bps derived from StillBuffered and DelayBuildUp values */
        inv_Q12 = WEBRTC_SPL_DIV(4096, FrameSamples);
        if (DelayBuildUp > State->StillBuffered) {
          MinRate = WEBRTC_SPL_MUL(512 + WEBRTC_SPL_MUL(SAMPLES_PER_MSEC, WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(DelayBuildUp - State->StillBuffered, inv_Q12), 3)), BottleNeck);
        } else if ((den = WEBRTC_SPL_MUL(SAMPLES_PER_MSEC, (State->StillBuffered - DelayBuildUp))) >= FrameSamples) {
          /* MinRate will be negative here */
          MinRate = 0;
        } else {
          MinRate = WEBRTC_SPL_MUL((512 - WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(den, inv_Q12), 3)), BottleNeck);
        }
        //if (MinRate < 1.04 * BottleNeck)
        //    MinRate = 1.04 * BottleNeck;
        //Q9
        if (MinRate < WEBRTC_SPL_MUL(532, BottleNeck)) {
          MinRate += WEBRTC_SPL_MUL(22, BottleNeck);
        }
      }

      State->BurstCounter--;
    }
  }


  /* convert rate from bits/second to bytes/packet */
  //round and shift before conversion
  MinRate += 256;
  MinRate = WEBRTC_SPL_RSHIFT_W32(MinRate, 9);
  MinBytes = (WebRtc_UWord16)WEBRTC_SPL_UDIV(WEBRTC_SPL_MUL(MinRate, FrameSamples), FS8);

  /* StreamSize will be adjusted if less than MinBytes */
  if (StreamSize < MinBytes) {
    StreamSize = MinBytes;
  }

  /* keep track of when bottle neck was last exceeded by at least 1% */
  //517/512 ~ 1.01
  if (WEBRTC_SPL_DIV(WEBRTC_SPL_MUL(StreamSize, FS8), FrameSamples) > (WEBRTC_SPL_MUL(517, BottleNeck) >> 9)) {
    if (State->PrevExceed) {
      /* bottle_neck exceded twice in a row, decrease ExceedAgo */
      State->ExceedAgo -= WEBRTC_SPL_DIV(BURST_INTERVAL, BURST_LEN - 1);
      if (State->ExceedAgo < 0) {
        State->ExceedAgo = 0;
      }
    } else {
      State->ExceedAgo += (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(FrameSamples, 4);       /* ms */
      State->PrevExceed = 1;
    }
  } else {
    State->PrevExceed = 0;
    State->ExceedAgo += (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(FrameSamples, 4);           /* ms */
  }

  /* set burst flag if bottle neck not exceeded for long time */
  if ((State->ExceedAgo > BURST_INTERVAL) && (State->BurstCounter == 0)) {
    if (State->PrevExceed) {
      State->BurstCounter = BURST_LEN - 1;
    } else {
      State->BurstCounter = BURST_LEN;
    }
  }


  /* Update buffer delay */
  TransmissionTime = (WebRtc_Word16)WEBRTC_SPL_DIV(WEBRTC_SPL_MUL(StreamSize, 8000), BottleNeck);    /* ms */
  State->StillBuffered += TransmissionTime;
  State->StillBuffered -= (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(FrameSamples, 4);  //>>4 =  SAMPLES_PER_MSEC        /* ms */
  if (State->StillBuffered < 0) {
    State->StillBuffered = 0;
  }

  if (State->StillBuffered > 2000) {
    State->StillBuffered = 2000;
  }

  return MinBytes;
}


/*
 * update long-term average bitrate and amount of data in buffer
 */
void WebRtcIsacfix_UpdateRateModel(RateModel *State,
                                   WebRtc_Word16 StreamSize,                    /* bytes in bitstream */
                                   const WebRtc_Word16 FrameSamples,            /* samples per frame */
                                   const WebRtc_Word16 BottleNeck)        /* bottle neck rate; excl headers (bps) */
{
  WebRtc_Word16 TransmissionTime;

  /* avoid the initial "high-rate" burst */
  State->InitCounter = 0;

  /* Update buffer delay */
  TransmissionTime = (WebRtc_Word16)WEBRTC_SPL_DIV(WEBRTC_SPL_MUL(WEBRTC_SPL_MUL(StreamSize, 8), 1000), BottleNeck);    /* ms */
  State->StillBuffered += TransmissionTime;
  State->StillBuffered -= (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(FrameSamples, 4);            /* ms */
  if (State->StillBuffered < 0) {
    State->StillBuffered = 0;
  }

}


void WebRtcIsacfix_InitRateModel(RateModel *State)
{
  State->PrevExceed      = 0;                        /* boolean */
  State->ExceedAgo       = 0;                        /* ms */
  State->BurstCounter    = 0;                        /* packets */
  State->InitCounter     = INIT_BURST_LEN + 10;    /* packets */
  State->StillBuffered   = 1;                    /* ms */
}





WebRtc_Word16 WebRtcIsacfix_GetNewFrameLength(WebRtc_Word16 bottle_neck, WebRtc_Word16 current_framesamples)
{
  WebRtc_Word16 new_framesamples;

  new_framesamples = current_framesamples;

  /* find new framelength */
  switch(current_framesamples) {
    case 480:
      if (bottle_neck < Thld_30_60) {
        new_framesamples = 960;
      }
      break;
    case 960:
      if (bottle_neck >= Thld_60_30) {
        new_framesamples = 480;
      }
      break;
    default:
      new_framesamples = -1; /* Error */
  }

  return new_framesamples;
}

WebRtc_Word16 WebRtcIsacfix_GetSnr(WebRtc_Word16 bottle_neck, WebRtc_Word16 framesamples)
{
  WebRtc_Word16 s2nr = 0;

  /* find new SNR value */
  //consider BottleNeck to be in Q10 ( * 1 in Q10)
  switch(framesamples) {
    case 480:
      /*s2nr = -1*(a_30 << 10) + ((b_30 * bottle_neck) >> 10);*/
      s2nr = -22500 + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(500, bottle_neck, 10); //* 0.001; //+ c_30 * bottle_neck * bottle_neck * 0.000001;
      break;
    case 960:
      /*s2nr = -1*(a_60 << 10) + ((b_60 * bottle_neck) >> 10);*/
      s2nr = -22500 + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(500, bottle_neck, 10); //* 0.001; //+ c_30 * bottle_neck * bottle_neck * 0.000001;
      break;
    default:
      s2nr = -1; /* Error */
  }

  return s2nr; //return in Q10

}
