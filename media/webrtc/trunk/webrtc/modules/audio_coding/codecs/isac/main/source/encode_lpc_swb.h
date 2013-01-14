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
 * encode_lpc_swb.h
 *
 * This file contains declaration of functions used to
 * encode LPC parameters (Shape & gain) of the upper band.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENCODE_LPC_SWB_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENCODE_LPC_SWB_H_

#include "typedefs.h"
#include "settings.h"
#include "structs.h"


/******************************************************************************
 * WebRtcIsac_RemoveLarMean()
 *
 * Remove the means from LAR coefficients.
 *
 * Input:
 *      -lar                : pointer to lar vectors. LAR vectors are
 *                            concatenated.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -lar                : pointer to mean-removed LAR:s.
 *
 *
 */
WebRtc_Word16 WebRtcIsac_RemoveLarMean(
    double*     lar,
    WebRtc_Word16 bandwidth);

/******************************************************************************
 * WebRtcIsac_DecorrelateIntraVec()
 *
 * Remove the correlation amonge the components of LAR vectors. If LAR vectors
 * of one frame are put in a matrix where each column is a LAR vector of a
 * sub-frame, then this is equivalent to multiplying the LAR matrix with
 * a decorrelting mtrix from left.
 *
 * Input:
 *      -inLar              : pointer to mean-removed LAR vecrtors.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -out                : decorrelated LAR vectors.
 */
WebRtc_Word16 WebRtcIsac_DecorrelateIntraVec(
    const double* inLAR,
    double*       out,
    WebRtc_Word16   bandwidth);


/******************************************************************************
 * WebRtcIsac_DecorrelateInterVec()
 *
 * Remover the correlation among mean-removed LAR vectors. If LAR vectors
 * of one frame are put in a matrix where each column is a LAR vector of a
 * sub-frame, then this is equivalent to multiplying the LAR matrix with
 * a decorrelting mtrix from right.
 *
 * Input:
 *      -data               : pointer to matrix of LAR vectors. The matrix
 *                            is stored column-wise.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -out                : decorrelated LAR vectors.
 */
WebRtc_Word16 WebRtcIsac_DecorrelateInterVec(
    const double* data,
    double*       out,
    WebRtc_Word16   bandwidth);


/******************************************************************************
 * WebRtcIsac_QuantizeUncorrLar()
 *
 * Quantize the uncorrelated parameters.
 *
 * Input:
 *      -data               : uncorrelated LAR vectors.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -data               : quantized version of the input.
 *      -idx                : pointer to quantization indices.
 */
double WebRtcIsac_QuantizeUncorrLar(
    double*     data,
    int*        idx,
    WebRtc_Word16 bandwidth);


/******************************************************************************
 * WebRtcIsac_CorrelateIntraVec()
 *
 * This is the inverse of WebRtcIsac_DecorrelateIntraVec().
 *
 * Input:
 *      -data               : uncorrelated parameters.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -out                : correlated parametrs.
 */
WebRtc_Word16 WebRtcIsac_CorrelateIntraVec(
    const double* data,
    double*       out,
    WebRtc_Word16   bandwidth);


/******************************************************************************
 * WebRtcIsac_CorrelateInterVec()
 *
 * This is the inverse of WebRtcIsac_DecorrelateInterVec().
 *
 * Input:
 *      -data
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -out                : correlated parametrs.
 */
WebRtc_Word16 WebRtcIsac_CorrelateInterVec(
    const double* data,
    double*       out,
    WebRtc_Word16   bandwidth);


/******************************************************************************
 * WebRtcIsac_AddLarMean()
 *
 * This is the inverse of WebRtcIsac_RemoveLarMean()
 * 
 * Input:
 *      -data               : pointer to mean-removed LAR:s.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -data               : pointer to LARs.
 */
WebRtc_Word16 WebRtcIsac_AddLarMean(
    double*     data,
    WebRtc_Word16 bandwidth);


/******************************************************************************
 * WebRtcIsac_DequantizeLpcParam()
 *
 * Get the quantized value of uncorrelated LARs given the quantization indices.
 *
 * Input:
 *      -idx                : pointer to quantiztion indices.
 *      -bandwidth          : indicates if the given LAR vectors belong
 *                            to SWB-12kHz or SWB-16kHz.
 *
 * Output:
 *      -out                : pointer to quantized values.
 */
WebRtc_Word16 WebRtcIsac_DequantizeLpcParam(
    const int*  idx,
    double*     out,
    WebRtc_Word16 bandwidth);


/******************************************************************************
 * WebRtcIsac_ToLogDomainRemoveMean()
 *
 * Transform the LPC gain to log domain then remove the mean value.
 *
 * Input:
 *      -lpcGain            : pointer to LPC Gain, expecting 6 LPC gains
 *
 * Output:
 *      -lpcGain            : mean-removed in log domain.
 */
WebRtc_Word16 WebRtcIsac_ToLogDomainRemoveMean(
    double* lpGains);


/******************************************************************************
 * WebRtcIsac_DecorrelateLPGain()
 *
 * Decorrelate LPC gains. There are 6 LPC Gains per frame. This is like
 * multiplying gain vector with decorrelating matrix.
 *
 * Input:
 *      -data               : LPC gain in log-domain with mean removed.
 *
 * Output:
 *      -out                : decorrelated parameters.
 */
WebRtc_Word16 WebRtcIsac_DecorrelateLPGain(
    const double* data,
    double*       out);


/******************************************************************************
 * WebRtcIsac_QuantizeLpcGain()
 *
 * Quantize the decorrelated log-domain gains.
 * 
 * Input:
 *      -lpcGain            : uncorrelated LPC gains.
 *
 * Output:
 *      -idx                : quantization indices
 *      -lpcGain            : quantized value of the inpt.
 */
double WebRtcIsac_QuantizeLpcGain(
    double* lpGains,
    int*    idx);


/******************************************************************************
 * WebRtcIsac_DequantizeLpcGain()
 *
 * Get the quantized values given the quantization indices.
 *
 * Input:
 *      -idx                : pointer to quantization indices.
 *
 * Output:
 *      -lpcGains           : quantized values of the given parametes.
 */
WebRtc_Word16 WebRtcIsac_DequantizeLpcGain(
    const int* idx,
    double*    lpGains);


/******************************************************************************
 * WebRtcIsac_CorrelateLpcGain()
 *
 * This is the inverse of WebRtcIsac_DecorrelateLPGain().
 *
 * Input:
 *      -data               : decorrelated parameters.
 *
 * Output:
 *      -out                : correlated parameters.
 */
WebRtc_Word16 WebRtcIsac_CorrelateLpcGain(
    const double* data,
    double*       out);


/******************************************************************************
 * WebRtcIsac_AddMeanToLinearDomain()
 *
 * This is the inverse of WebRtcIsac_ToLogDomainRemoveMean().
 *
 * Input:
 *      -lpcGain            : LPC gain in log-domain & mean removed
 *
 * Output:
 *      -lpcGain            : LPC gain in normal domain.
 */
WebRtc_Word16 WebRtcIsac_AddMeanToLinearDomain(
    double* lpcGains);


#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENCODE_LPC_SWB_H_
