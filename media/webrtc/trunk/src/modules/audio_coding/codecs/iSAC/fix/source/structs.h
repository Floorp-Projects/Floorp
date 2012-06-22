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
 * structs.h
 *
 * This header file contains all the structs used in the ISAC codec
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_STRUCTS_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_STRUCTS_H_


#include "typedefs.h"
#include "signal_processing_library.h"
#include "settings.h"

/* Bitstream struct for decoder */
typedef struct Bitstreamstruct_dec {

  WebRtc_UWord16  *stream;          /* Pointer to bytestream to decode */
  WebRtc_UWord32  W_upper;          /* Upper boundary of interval W */
  WebRtc_UWord32  streamval;
  WebRtc_UWord16  stream_index;     /* Index to the current position in bytestream */
  WebRtc_Word16   full;             /* 0 - first byte in memory filled, second empty*/
  /* 1 - both bytes are empty (we just filled the previous memory */

} Bitstr_dec;

/* Bitstream struct for encoder */
typedef struct Bitstreamstruct_enc {

  WebRtc_UWord16  stream[STREAM_MAXW16_60MS];   /* Vector for adding encoded bytestream */
  WebRtc_UWord32  W_upper;          /* Upper boundary of interval W */
  WebRtc_UWord32  streamval;
  WebRtc_UWord16  stream_index;     /* Index to the current position in bytestream */
  WebRtc_Word16   full;             /* 0 - first byte in memory filled, second empty*/
  /* 1 - both bytes are empty (we just filled the previous memory */

} Bitstr_enc;


typedef struct {

  WebRtc_Word16 DataBufferLoQ0[WINLEN];
  WebRtc_Word16 DataBufferHiQ0[WINLEN];

  WebRtc_Word32 CorrBufLoQQ[ORDERLO+1];
  WebRtc_Word32 CorrBufHiQQ[ORDERHI+1];

  WebRtc_Word16 CorrBufLoQdom[ORDERLO+1];
  WebRtc_Word16 CorrBufHiQdom[ORDERHI+1];

  WebRtc_Word32 PreStateLoGQ15[ORDERLO+1];
  WebRtc_Word32 PreStateHiGQ15[ORDERHI+1];

  WebRtc_UWord32 OldEnergy;

} MaskFiltstr_enc;



typedef struct {

  WebRtc_Word16 PostStateLoGQ0[ORDERLO+1];
  WebRtc_Word16 PostStateHiGQ0[ORDERHI+1];

  WebRtc_UWord32 OldEnergy;

} MaskFiltstr_dec;








typedef struct {

  //state vectors for each of the two analysis filters

  WebRtc_Word32 INSTAT1_fix[2*(QORDER-1)];
  WebRtc_Word32 INSTAT2_fix[2*(QORDER-1)];
  WebRtc_Word16 INLABUF1_fix[QLOOKAHEAD];
  WebRtc_Word16 INLABUF2_fix[QLOOKAHEAD];

  /* High pass filter */
  WebRtc_Word32 HPstates_fix[HPORDER];

} PreFiltBankstr;


typedef struct {

  //state vectors for each of the two analysis filters
  WebRtc_Word32 STATE_0_LOWER_fix[2*POSTQORDER];
  WebRtc_Word32 STATE_0_UPPER_fix[2*POSTQORDER];

  /* High pass filter */

  WebRtc_Word32 HPstates1_fix[HPORDER];
  WebRtc_Word32 HPstates2_fix[HPORDER];

} PostFiltBankstr;

typedef struct {


  /* data buffer for pitch filter */
  WebRtc_Word16 ubufQQ[PITCH_BUFFSIZE];

  /* low pass state vector */
  WebRtc_Word16 ystateQQ[PITCH_DAMPORDER];

  /* old lag and gain */
  WebRtc_Word16 oldlagQ7;
  WebRtc_Word16 oldgainQ12;

} PitchFiltstr;



typedef struct {

  //for inital estimator
  WebRtc_Word16   dec_buffer16[PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2-PITCH_FRAME_LEN/2+2];
  WebRtc_Word32   decimator_state32[2*ALLPASSSECTIONS+1];
  WebRtc_Word16   inbuf[QLOOKAHEAD];

  PitchFiltstr  PFstr_wght;
  PitchFiltstr  PFstr;


} PitchAnalysisStruct;


typedef struct {
  /* Parameters used in PLC to avoid re-computation       */

  /* --- residual signals --- */
  WebRtc_Word16 prevPitchInvIn[FRAMESAMPLES/2];
  WebRtc_Word16 prevPitchInvOut[PITCH_MAX_LAG + 10];            // [FRAMESAMPLES/2]; save 90
  WebRtc_Word32 prevHP[PITCH_MAX_LAG + 10];                     // [FRAMESAMPLES/2]; save 90


  WebRtc_Word16 decayCoeffPriodic; /* how much to supress a sample */
  WebRtc_Word16 decayCoeffNoise;
  WebRtc_Word16 used;       /* if PLC is used */


  WebRtc_Word16 *lastPitchLP;                                  // [FRAMESAMPLES/2]; saved 240;


  /* --- LPC side info --- */
  WebRtc_Word16 lofilt_coefQ15[ ORDERLO ];
  WebRtc_Word16 hifilt_coefQ15[ ORDERHI ];
  WebRtc_Word32 gain_lo_hiQ17[2];

  /* --- LTP side info --- */
  WebRtc_Word16 AvgPitchGain_Q12;
  WebRtc_Word16 lastPitchGain_Q12;
  WebRtc_Word16 lastPitchLag_Q7;

  /* --- Add-overlap in recovery packet --- */
  WebRtc_Word16 overlapLP[ RECOVERY_OVERLAP ];                 // [FRAMESAMPLES/2]; saved 160

  WebRtc_Word16 pitchCycles;
  WebRtc_Word16 A;
  WebRtc_Word16 B;
  WebRtc_Word16 pitchIndex;
  WebRtc_Word16 stretchLag;
  WebRtc_Word16 *prevPitchLP;                                  // [ FRAMESAMPLES/2 ]; saved 240
  WebRtc_Word16 seed;

  WebRtc_Word16 std;
} PLCstr;



/* Have instance of struct together with other iSAC structs */
typedef struct {

  WebRtc_Word16   prevFrameSizeMs;      /* Previous frame size (in ms) */
  WebRtc_UWord16  prevRtpNumber;      /* Previous RTP timestamp from received packet */
  /* (in samples relative beginning)  */
  WebRtc_UWord32  prevSendTime;   /* Send time for previous packet, from RTP header */
  WebRtc_UWord32  prevArrivalTime;      /* Arrival time for previous packet (in ms using timeGetTime()) */
  WebRtc_UWord16  prevRtpRate;          /* rate of previous packet, derived from RTP timestamps (in bits/s) */
  WebRtc_UWord32  lastUpdate;           /* Time since the last update of the Bottle Neck estimate (in samples) */
  WebRtc_UWord32  lastReduction;        /* Time sinse the last reduction (in samples) */
  WebRtc_Word32   countUpdates;         /* How many times the estimate was update in the beginning */

  /* The estimated bottle neck rate from there to here (in bits/s)                */
  WebRtc_UWord32  recBw;
  WebRtc_UWord32  recBwInv;
  WebRtc_UWord32  recBwAvg;
  WebRtc_UWord32  recBwAvgQ;

  WebRtc_UWord32  minBwInv;
  WebRtc_UWord32  maxBwInv;

  /* The estimated mean absolute jitter value, as seen on this side (in ms)       */
  WebRtc_Word32   recJitter;
  WebRtc_Word32   recJitterShortTerm;
  WebRtc_Word32   recJitterShortTermAbs;
  WebRtc_Word32   recMaxDelay;
  WebRtc_Word32   recMaxDelayAvgQ;


  WebRtc_Word16   recHeaderRate;         /* (assumed) bitrate for headers (bps) */

  WebRtc_UWord32  sendBwAvg;           /* The estimated bottle neck rate from here to there (in bits/s) */
  WebRtc_Word32   sendMaxDelayAvg;    /* The estimated mean absolute jitter value, as seen on the other siee (in ms)  */


  WebRtc_Word16   countRecPkts;          /* number of packets received since last update */
  WebRtc_Word16   highSpeedRec;        /* flag for marking that a high speed network has been detected downstream */

  /* number of consecutive pkts sent during which the bwe estimate has
     remained at a value greater than the downstream threshold for determining highspeed network */
  WebRtc_Word16   countHighSpeedRec;

  /* flag indicating bwe should not adjust down immediately for very late pckts */
  WebRtc_Word16   inWaitPeriod;

  /* variable holding the time of the start of a window of time when
     bwe should not adjust down immediately for very late pckts */
  WebRtc_UWord32  startWaitPeriod;

  /* number of consecutive pkts sent during which the bwe estimate has
     remained at a value greater than the upstream threshold for determining highspeed network */
  WebRtc_Word16   countHighSpeedSent;

  /* flag indicated the desired number of packets over threshold rate have been sent and
     bwe will assume the connection is over broadband network */
  WebRtc_Word16   highSpeedSend;




} BwEstimatorstr;


typedef struct {

  /* boolean, flags if previous packet exceeded B.N. */
  WebRtc_Word16    PrevExceed;
  /* ms */
  WebRtc_Word16    ExceedAgo;
  /* packets left to send in current burst */
  WebRtc_Word16    BurstCounter;
  /* packets */
  WebRtc_Word16    InitCounter;
  /* ms remaining in buffer when next packet will be sent */
  WebRtc_Word16    StillBuffered;

} RateModel;

/* The following strutc is used to store data from encoding, to make it
   fast and easy to construct a new bitstream with a different Bandwidth
   estimate. All values (except framelength and minBytes) is double size to
   handle 60 ms of data.
*/
typedef struct {

  /* Used to keep track of if it is first or second part of 60 msec packet */
  int     startIdx;

  /* Frame length in samples */
  WebRtc_Word16         framelength;

  /* Pitch Gain */
  WebRtc_Word16   pitchGain_index[2];

  /* Pitch Lag */
  WebRtc_Word32   meanGain[2];
  WebRtc_Word16   pitchIndex[PITCH_SUBFRAMES*2];

  /* LPC */
  WebRtc_Word32         LPCcoeffs_g[12*2]; /* KLT_ORDER_GAIN = 12 */
  WebRtc_Word16   LPCindex_s[108*2]; /* KLT_ORDER_SHAPE = 108 */
  WebRtc_Word16   LPCindex_g[12*2];  /* KLT_ORDER_GAIN = 12 */

  /* Encode Spec */
  WebRtc_Word16   fre[FRAMESAMPLES];
  WebRtc_Word16   fim[FRAMESAMPLES];
  WebRtc_Word16   AvgPitchGain[2];

  /* Used in adaptive mode only */
  int     minBytes;

} ISAC_SaveEncData_t;

typedef struct {

  Bitstr_enc          bitstr_obj;
  MaskFiltstr_enc     maskfiltstr_obj;
  PreFiltBankstr      prefiltbankstr_obj;
  PitchFiltstr        pitchfiltstr_obj;
  PitchAnalysisStruct pitchanalysisstr_obj;
  RateModel           rate_data_obj;

  WebRtc_Word16         buffer_index;
  WebRtc_Word16         current_framesamples;

  WebRtc_Word16      data_buffer_fix[FRAMESAMPLES]; // the size was MAX_FRAMESAMPLES

  WebRtc_Word16         frame_nb;
  WebRtc_Word16         BottleNeck;
  WebRtc_Word16         MaxDelay;
  WebRtc_Word16         new_framelength;
  WebRtc_Word16         s2nr;
  WebRtc_UWord16        MaxBits;

  WebRtc_Word16         bitstr_seed;
#ifdef WEBRTC_ISAC_FIX_NB_CALLS_ENABLED
  PostFiltBankstr     interpolatorstr_obj;
#endif

  ISAC_SaveEncData_t *SaveEnc_ptr;
  WebRtc_Word16         payloadLimitBytes30; /* Maximum allowed number of bits for a 30 msec packet */
  WebRtc_Word16         payloadLimitBytes60; /* Maximum allowed number of bits for a 30 msec packet */
  WebRtc_Word16         maxPayloadBytes;     /* Maximum allowed number of bits for both 30 and 60 msec packet */
  WebRtc_Word16         maxRateInBytes;      /* Maximum allowed rate in bytes per 30 msec packet */
  WebRtc_Word16         enforceFrameSize;    /* If set iSAC will never change packet size */

} ISACFIX_EncInst_t;


typedef struct {

  Bitstr_dec          bitstr_obj;
  MaskFiltstr_dec     maskfiltstr_obj;
  PostFiltBankstr     postfiltbankstr_obj;
  PitchFiltstr        pitchfiltstr_obj;
  PLCstr              plcstr_obj;               /* TS; for packet loss concealment */

#ifdef WEBRTC_ISAC_FIX_NB_CALLS_ENABLED
  PreFiltBankstr      decimatorstr_obj;
#endif

} ISACFIX_DecInst_t;



typedef struct {

  ISACFIX_EncInst_t ISACenc_obj;
  ISACFIX_DecInst_t ISACdec_obj;
  BwEstimatorstr     bwestimator_obj;
  WebRtc_Word16         CodingMode;       /* 0 = adaptive; 1 = instantaneous */
  WebRtc_Word16   errorcode;
  WebRtc_Word16   initflag;  /* 0 = nothing initiated; 1 = encoder or decoder */
  /* not initiated; 2 = all initiated */
} ISACFIX_SubStruct;


typedef struct {
  WebRtc_Word32   lpcGains[12];     /* 6 lower-band & 6 upper-band we may need to double it for 60*/
  /* */
  WebRtc_UWord32  W_upper;          /* Upper boundary of interval W */
  WebRtc_UWord32  streamval;
  WebRtc_UWord16  stream_index;     /* Index to the current position in bytestream */
  WebRtc_Word16   full;             /* 0 - first byte in memory filled, second empty*/
  /* 1 - both bytes are empty (we just filled the previous memory */
  WebRtc_UWord16  beforeLastWord;
  WebRtc_UWord16  lastWord;
} transcode_obj;


//Bitstr_enc myBitStr;

#endif  /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_STRUCTS_H_ */
