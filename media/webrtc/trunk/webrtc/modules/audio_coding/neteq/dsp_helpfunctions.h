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
 * Various help functions used by the DSP functions.
 */

#ifndef DSP_HELPFUNCTIONS_H
#define DSP_HELPFUNCTIONS_H

#include "typedefs.h"

#include "dsp.h"

/****************************************************************************
 * WebRtcNetEQ_Correlator(...)
 *
 * Calculate signal correlation.
 *
 * Input:
 *      - inst          : DSP instance
 *      - data          : Speech history to do expand from (older history in data[-4..-1])
 *      - dataLen       : Length of data
 *
 * Output:
 *      - corrOut       : CC of downsampled signal
 *      - corrScale     : Scale factor for correlation (-Qdomain)
 *
 * Return value         : Length of correlated data
 */

int16_t WebRtcNetEQ_Correlator(DSPInst_t *inst,
#ifdef SCRATCH
                               int16_t *pw16_scratchPtr,
#endif
                               int16_t *pw16_data, int16_t w16_dataLen,
                               int16_t *pw16_corrOut,
                               int16_t *pw16_corrScale);

/****************************************************************************
 * WebRtcNetEQ_PeakDetection(...)
 *
 * Peak detection with parabolic fit.
 *
 * Input:
 *      - data          : Data sequence for peak detection
 *      - dataLen       : Length of data
 *      - nmbPeaks      : Number of peaks to detect
 *      - fs_mult       : Sample rate multiplier
 *
 * Output:
 *      - corrIndex     : Index of the peak
 *      - winner        : Value of the peak
 *
 * Return value         : 0 for ok
 */

int16_t WebRtcNetEQ_PeakDetection(int16_t *pw16_data, int16_t w16_dataLen,
                                  int16_t w16_nmbPeaks, int16_t fs_mult,
                                  int16_t *pw16_corrIndex,
                                  int16_t *pw16_winners);

/****************************************************************************
 * WebRtcNetEQ_PrblFit(...)
 *
 * Three-point parbola fit.
 *
 * Input:
 *      - 3pts          : Three input samples
 *      - fs_mult       : Sample rate multiplier
 *
 * Output:
 *      - Ind           : Index of the peak
 *      - outVal        : Value of the peak
 *
 * Return value         : 0 for ok
 */

int16_t WebRtcNetEQ_PrblFit(int16_t *pw16_3pts, int16_t *pw16_Ind,
                            int16_t *pw16_outVal, int16_t fs_mult);

/****************************************************************************
 * WebRtcNetEQ_MinDistortion(...)
 *
 * Find the lag that results in minimum distortion.
 *
 * Input:
 *      - data          : Start of speech to perform distortion on, second vector is assumed
 *                        to be data[-Lag]
 *      - minLag        : Start lag
 *      - maxLag        : End lag
 *      - len           : Length to correlate
 *
 * Output:
 *      - dist          : Distorion value
 *
 * Return value         : Lag for minimum distortion
 */

int16_t WebRtcNetEQ_MinDistortion(const int16_t *pw16_data,
                                  int16_t w16_minLag, int16_t w16_maxLag,
                                  int16_t len, int32_t *pw16_dist);

/****************************************************************************
 * WebRtcNetEQ_RandomVec(...)
 *
 * Generate random vector.
 *
 * Input:
 *      - seed          : Current seed (input/output)
 *      - len           : Number of samples to generate
 *      - incVal        : Jump step
 *
 * Output:
 *      - randVec       : Generated random vector
 */

void WebRtcNetEQ_RandomVec(uint32_t *w32_seed, int16_t *pw16_randVec,
                           int16_t w16_len, int16_t w16_incval);

/****************************************************************************
 * WebRtcNetEQ_MixVoiceUnvoice(...)
 *
 * Mix voiced and unvoiced signal.
 *
 * Input:
 *      - voicedVec         : Voiced input signal
 *      - unvoicedVec       : Unvoiced input signal
 *      - current_vfraction : Current mixing factor
 *      - vfraction_change  : Mixing factor change per sample
 *      - N                 : Number of samples
 *
 * Output:
 *      - outData           : Mixed signal
 */

void WebRtcNetEQ_MixVoiceUnvoice(int16_t *pw16_outData, int16_t *pw16_voicedVec,
                                 int16_t *pw16_unvoicedVec,
                                 int16_t *w16_current_vfraction,
                                 int16_t w16_vfraction_change, int16_t N);

/****************************************************************************
 * WebRtcNetEQ_UnmuteSignal(...)
 *
 * Gradually reduce attenuation.
 *
 * Input:
 *      - inVec         : Input signal
 *      - startMuteFact : Starting attenuation
 *      - unmuteFact    : Factor to "unmute" with (Q20)
 *      - N             : Number of samples
 *
 * Output:
 *      - outVec        : Output signal
 */

void WebRtcNetEQ_UnmuteSignal(int16_t *pw16_inVec, int16_t *startMuteFact,
                              int16_t *pw16_outVec, int16_t unmuteFact,
                              int16_t N);

/****************************************************************************
 * WebRtcNetEQ_MuteSignal(...)
 *
 * Gradually increase attenuation.
 *
 * Input:
 *      - inout         : Input/output signal
 *      - muteSlope     : Slope of muting
 *      - N             : Number of samples
 */

void WebRtcNetEQ_MuteSignal(int16_t *pw16_inout, int16_t muteSlope,
                            int16_t N);

/****************************************************************************
 * WebRtcNetEQ_CalcFsMult(...)
 *
 * Calculate the sample rate divided by 8000.
 *
 * Input:
 *		- fsHz			: Sample rate in Hz in {8000, 16000, 32000, 48000}.
 *
 * Return value			: fsHz/8000 for the valid values, 1 for other inputs
 */

int16_t WebRtcNetEQ_CalcFsMult(uint16_t fsHz);

/****************************************************************************
 * WebRtcNetEQ_DownSampleTo4kHz(...)
 *
 * Lowpass filter and downsample a signal to 4 kHz sample rate.
 *
 * Input:
 *      - in                : Input signal samples.
 *      - inLen             : Number of input samples.
 *		- inFsHz		    : Input sample rate in Hz.
 *      - outLen            : Desired number of samples in decimated signal.
 *      - compensateDelay   : If non-zero, compensate for the phase delay of
 *                            of the anti-alias filter.
 *
 * Output:
 *      - out               : Output signal samples.
 *
 * Return value			    : 0 - Ok
 *                           -1 - Error
 *
 */

int WebRtcNetEQ_DownSampleTo4kHz(const int16_t *in, int inLen, uint16_t inFsHz,
                                 int16_t *out, int outLen, int compensateDelay);

#endif

