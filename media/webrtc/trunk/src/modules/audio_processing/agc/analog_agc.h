/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_ANALOG_AGC_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_ANALOG_AGC_H_

#include "typedefs.h"
#include "gain_control.h"
#include "digital_agc.h"

//#define AGC_DEBUG
//#define MIC_LEVEL_FEEDBACK
#ifdef AGC_DEBUG
#include <stdio.h>
#endif

/* Analog Automatic Gain Control variables:
 * Constant declarations (inner limits inside which no changes are done)
 * In the beginning the range is narrower to widen as soon as the measure
 * 'Rxx160_LP' is inside it. Currently the starting limits are -22.2+/-1dBm0
 * and the final limits -22.2+/-2.5dBm0. These levels makes the speech signal
 * go towards -25.4dBm0 (-31.4dBov). Tuned with wbfile-31.4dBov.pcm
 * The limits are created by running the AGC with a file having the desired
 * signal level and thereafter plotting Rxx160_LP in the dBm0-domain defined
 * by out=10*log10(in/260537279.7); Set the target level to the average level
 * of our measure Rxx160_LP. Remember that the levels are in blocks of 16 in
 * Q(-7). (Example matlab code: round(db2pow(-21.2)*16/2^7) )
 */
#define RXX_BUFFER_LEN  10

static const WebRtc_Word16 kMsecSpeechInner = 520;
static const WebRtc_Word16 kMsecSpeechOuter = 340;

static const WebRtc_Word16 kNormalVadThreshold = 400;

static const WebRtc_Word16 kAlphaShortTerm = 6; // 1 >> 6 = 0.0156
static const WebRtc_Word16 kAlphaLongTerm = 10; // 1 >> 10 = 0.000977

typedef struct
{
    // Configurable parameters/variables
    WebRtc_UWord32      fs;                 // Sampling frequency
    WebRtc_Word16       compressionGaindB;  // Fixed gain level in dB
    WebRtc_Word16       targetLevelDbfs;    // Target level in -dBfs of envelope (default -3)
    WebRtc_Word16       agcMode;            // Hard coded mode (adaptAna/adaptDig/fixedDig)
    WebRtc_UWord8       limiterEnable;      // Enabling limiter (on/off (default off))
    WebRtcAgc_config_t  defaultConfig;
    WebRtcAgc_config_t  usedConfig;

    // General variables
    WebRtc_Word16       initFlag;
    WebRtc_Word16       lastError;

    // Target level parameters
    // Based on the above: analogTargetLevel = round((32767*10^(-22/20))^2*16/2^7)
    WebRtc_Word32       analogTargetLevel;  // = RXX_BUFFER_LEN * 846805;       -22 dBfs
    WebRtc_Word32       startUpperLimit;    // = RXX_BUFFER_LEN * 1066064;      -21 dBfs
    WebRtc_Word32       startLowerLimit;    // = RXX_BUFFER_LEN * 672641;       -23 dBfs
    WebRtc_Word32       upperPrimaryLimit;  // = RXX_BUFFER_LEN * 1342095;      -20 dBfs
    WebRtc_Word32       lowerPrimaryLimit;  // = RXX_BUFFER_LEN * 534298;       -24 dBfs
    WebRtc_Word32       upperSecondaryLimit;// = RXX_BUFFER_LEN * 2677832;      -17 dBfs
    WebRtc_Word32       lowerSecondaryLimit;// = RXX_BUFFER_LEN * 267783;       -27 dBfs
    WebRtc_UWord16      targetIdx;          // Table index for corresponding target level
#ifdef MIC_LEVEL_FEEDBACK
    WebRtc_UWord16      targetIdxOffset;    // Table index offset for level compensation
#endif
    WebRtc_Word16       analogTarget;       // Digital reference level in ENV scale

    // Analog AGC specific variables
    WebRtc_Word32       filterState[8];     // For downsampling wb to nb
    WebRtc_Word32       upperLimit;         // Upper limit for mic energy
    WebRtc_Word32       lowerLimit;         // Lower limit for mic energy
    WebRtc_Word32       Rxx160w32;          // Average energy for one frame
    WebRtc_Word32       Rxx16_LPw32;        // Low pass filtered subframe energies
    WebRtc_Word32       Rxx160_LPw32;       // Low pass filtered frame energies
    WebRtc_Word32       Rxx16_LPw32Max;     // Keeps track of largest energy subframe
    WebRtc_Word32       Rxx16_vectorw32[RXX_BUFFER_LEN];// Array with subframe energies
    WebRtc_Word32       Rxx16w32_array[2][5];// Energy values of microphone signal
    WebRtc_Word32       env[2][10];         // Envelope values of subframes

    WebRtc_Word16       Rxx16pos;           // Current position in the Rxx16_vectorw32
    WebRtc_Word16       envSum;             // Filtered scaled envelope in subframes
    WebRtc_Word16       vadThreshold;       // Threshold for VAD decision
    WebRtc_Word16       inActive;           // Inactive time in milliseconds
    WebRtc_Word16       msTooLow;           // Milliseconds of speech at a too low level
    WebRtc_Word16       msTooHigh;          // Milliseconds of speech at a too high level
    WebRtc_Word16       changeToSlowMode;   // Change to slow mode after some time at target
    WebRtc_Word16       firstCall;          // First call to the process-function
    WebRtc_Word16       msZero;             // Milliseconds of zero input
    WebRtc_Word16       msecSpeechOuterChange;// Min ms of speech between volume changes
    WebRtc_Word16       msecSpeechInnerChange;// Min ms of speech between volume changes
    WebRtc_Word16       activeSpeech;       // Milliseconds of active speech
    WebRtc_Word16       muteGuardMs;        // Counter to prevent mute action
    WebRtc_Word16       inQueue;            // 10 ms batch indicator

    // Microphone level variables
    WebRtc_Word32       micRef;             // Remember ref. mic level for virtual mic
    WebRtc_UWord16      gainTableIdx;       // Current position in virtual gain table
    WebRtc_Word32       micGainIdx;         // Gain index of mic level to increase slowly
    WebRtc_Word32       micVol;             // Remember volume between frames
    WebRtc_Word32       maxLevel;           // Max possible vol level, incl dig gain
    WebRtc_Word32       maxAnalog;          // Maximum possible analog volume level
    WebRtc_Word32       maxInit;            // Initial value of "max"
    WebRtc_Word32       minLevel;           // Minimum possible volume level
    WebRtc_Word32       minOutput;          // Minimum output volume level
    WebRtc_Word32       zeroCtrlMax;        // Remember max gain => don't amp low input

    WebRtc_Word16       scale;              // Scale factor for internal volume levels
#ifdef MIC_LEVEL_FEEDBACK
    WebRtc_Word16       numBlocksMicLvlSat;
    WebRtc_UWord8 micLvlSat;
#endif
    // Structs for VAD and digital_agc
    AgcVad_t            vadMic;
    DigitalAgc_t        digitalAgc;

#ifdef AGC_DEBUG
    FILE*               fpt;
    FILE*               agcLog;
    WebRtc_Word32       fcount;
#endif

    WebRtc_Word16       lowLevelSignal;
} Agc_t;

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_ANALOG_AGC_H_
