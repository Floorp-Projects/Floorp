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

WebRtc_Word16 WebRtcNetEQ_Correlator(DSPInst_t *inst,
#ifdef SCRATCH
                                     WebRtc_Word16 *pw16_scratchPtr,
#endif
                                     WebRtc_Word16 *pw16_data, WebRtc_Word16 w16_dataLen,
                                     WebRtc_Word16 *pw16_corrOut,
                                     WebRtc_Word16 *pw16_corrScale);

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

WebRtc_Word16 WebRtcNetEQ_PeakDetection(WebRtc_Word16 *pw16_data, WebRtc_Word16 w16_dataLen,
                                        WebRtc_Word16 w16_nmbPeaks, WebRtc_Word16 fs_mult,
                                        WebRtc_Word16 *pw16_corrIndex,
                                        WebRtc_Word16 *pw16_winners);

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

WebRtc_Word16 WebRtcNetEQ_PrblFit(WebRtc_Word16 *pw16_3pts, WebRtc_Word16 *pw16_Ind,
                                  WebRtc_Word16 *pw16_outVal, WebRtc_Word16 fs_mult);

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

WebRtc_Word16 WebRtcNetEQ_MinDistortion(const WebRtc_Word16 *pw16_data,
                                        WebRtc_Word16 w16_minLag, WebRtc_Word16 w16_maxLag,
                                        WebRtc_Word16 len, WebRtc_Word32 *pw16_dist);

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

void WebRtcNetEQ_RandomVec(WebRtc_UWord32 *w32_seed, WebRtc_Word16 *pw16_randVec,
                           WebRtc_Word16 w16_len, WebRtc_Word16 w16_incval);

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

void WebRtcNetEQ_MixVoiceUnvoice(WebRtc_Word16 *pw16_outData, WebRtc_Word16 *pw16_voicedVec,
                                 WebRtc_Word16 *pw16_unvoicedVec,
                                 WebRtc_Word16 *w16_current_vfraction,
                                 WebRtc_Word16 w16_vfraction_change, WebRtc_Word16 N);

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

void WebRtcNetEQ_UnmuteSignal(WebRtc_Word16 *pw16_inVec, WebRtc_Word16 *startMuteFact,
                              WebRtc_Word16 *pw16_outVec, WebRtc_Word16 unmuteFact,
                              WebRtc_Word16 N);

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

void WebRtcNetEQ_MuteSignal(WebRtc_Word16 *pw16_inout, WebRtc_Word16 muteSlope,
                            WebRtc_Word16 N);

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

WebRtc_Word16 WebRtcNetEQ_CalcFsMult(WebRtc_UWord16 fsHz);

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

int WebRtcNetEQ_DownSampleTo4kHz(const WebRtc_Word16 *in, int inLen, WebRtc_UWord16 inFsHz,
                                 WebRtc_Word16 *out, int outLen, int compensateDelay);

#endif

