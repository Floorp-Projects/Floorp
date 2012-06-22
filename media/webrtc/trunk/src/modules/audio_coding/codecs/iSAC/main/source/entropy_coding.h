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
 * entropy_coding.h
 *
 * This header file declares all of the functions used to arithmetically
 * encode the iSAC bistream
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENTROPY_CODING_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENTROPY_CODING_H_

#include "structs.h"

/* decode complex spectrum (return number of bytes in stream) */
int WebRtcIsac_DecodeSpecLb(Bitstr *streamdata,
                            double *fr,
                            double *fi,
                            WebRtc_Word16 AvgPitchGain_Q12);

/******************************************************************************
 * WebRtcIsac_DecodeSpecUB16()
 * Decode real and imaginary part of the DFT coefficients, given a bit-stream.
 * This function is called when the codec is in 0-16 kHz bandwidth.
 * The decoded DFT coefficient can be transformed to time domain by
 * WebRtcIsac_Time2Spec().
 *
 * Input:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are written to.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are written to.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_DecodeSpecUB16(
    Bitstr* streamdata,
    double* fr,
    double* fi);


/******************************************************************************
 * WebRtcIsac_DecodeSpecUB12()
 * Decode real and imaginary part of the DFT coefficients, given a bit-stream.
 * This function is called when the codec is in 0-12 kHz bandwidth.
 * The decoded DFT coefficient can be transformed to time domain by
 * WebRtcIsac_Time2Spec().
 *
 * Input:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are written to.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are written to.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_DecodeSpecUB12(
    Bitstr* streamdata,
    double* fr,
    double* fi);


/* encode complex spectrum */
int WebRtcIsac_EncodeSpecLb(const WebRtc_Word16* fr,
                            const WebRtc_Word16* fi,
                            Bitstr* streamdata,
                            WebRtc_Word16 AvgPitchGain_Q12);


/******************************************************************************
 * WebRtcIsac_EncodeSpecUB16()
 * Quantize and encode real and imaginary part of the DFT coefficients.
 * This function is called when the codec is in 0-16 kHz bandwidth.
 * The real and imaginary part are computed by calling WebRtcIsac_Time2Spec().
 *
 *
 * Input:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are stored.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are stored.
 *
 * Output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_EncodeSpecUB16(
    const WebRtc_Word16* fr,
    const WebRtc_Word16* fi,
    Bitstr*            streamdata);


/******************************************************************************
 * WebRtcIsac_EncodeSpecUB12()
 * Quantize and encode real and imaginary part of the DFT coefficients.
 * This function is called when the codec is in 0-12 kHz bandwidth.
 * The real and imaginary part are computed by calling WebRtcIsac_Time2Spec().
 *
 *
 * Input:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are stored.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are stored.
 *
 * Output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_EncodeSpecUB12(
    const WebRtc_Word16* fr,
    const WebRtc_Word16* fi,
    Bitstr*            streamdata);


/* decode & dequantize LPC Coef */
int WebRtcIsac_DecodeLpcCoef(Bitstr *streamdata, double *LPCCoef, int *outmodel);
int WebRtcIsac_DecodeLpcCoefUB(
    Bitstr*     streamdata,
    double*     lpcVecs,
    double*     percepFilterGains,
    WebRtc_Word16 bandwidth);

int WebRtcIsac_DecodeLpc(Bitstr *streamdata, double *LPCCoef_lo, double *LPCCoef_hi, int *outmodel);

/* quantize & code LPC Coef */
void WebRtcIsac_EncodeLpcLb(double *LPCCoef_lo, double *LPCCoef_hi, int *model, double *size, Bitstr *streamdata, ISAC_SaveEncData_t* encData);
void WebRtcIsac_EncodeLpcGainLb(double *LPCCoef_lo, double *LPCCoef_hi, int model, Bitstr *streamdata, ISAC_SaveEncData_t* encData);

/******************************************************************************
 * WebRtcIsac_EncodeLpcUB()
 * Encode LPC parameters, given as A-polynomial, of upper-band. The encoding
 * is performed in LAR domain.
 * For the upper-band, we compute and encode LPC of some sub-frames, LPC of
 * other sub-frames are computed by linear interpolation, in LAR domain. This
 * function performs the interpolation and returns the LPC of all sub-frames.
 *
 * Inputs:
 *  - lpcCoef               : a buffer containing A-polynomials of sub-frames
 *                            (excluding first coefficient that is 1).
 *  - bandwidth             : specifies if the codec is operating at 0-12 kHz
 *                            or 0-16 kHz mode.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - interpolLPCCoeff      : Decoded and interpolated LPC (A-polynomial)
 *                            of all sub-frames.
 *                            If LP analysis is of order K, and there are N
 *                            sub-frames then this is a buffer of size
 *                            (k + 1) * N, each vector starts with the LPC gain
 *                            of the corresponding sub-frame. The LPC gains
 *                            are encoded and inserted after this function is
 *                            called. The first A-coefficient which is 1 is not
 *                            included.
 *
 * Return value             : 0 if encoding is successful,
 *                           <0 if failed to encode.
 */
WebRtc_Word16 WebRtcIsac_EncodeLpcUB(
    double*                  lpcCoeff,
    Bitstr*                  streamdata,
    double*                  interpolLPCCoeff,
    WebRtc_Word16              bandwidth,
    ISACUBSaveEncDataStruct* encData);

/******************************************************************************
 * WebRtcIsac_DecodeInterpolLpcUb()
 * Decode LPC coefficients and interpolate to get the coefficients fo all
 * sub-frmaes.
 *
 * Inputs:
 *  - bandwidth             : spepecifies if the codec is in 0-12 kHz or
 *                            0-16 kHz mode.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - percepFilterParam     : Decoded and interpolated LPC (A-polynomial) of
 *                            all sub-frames.
 *                            If LP analysis is of order K, and there are N
 *                            sub-frames then this is a buffer of size
 *                            (k + 1) * N, each vector starts with the LPC gain
 *                            of the corresponding sub-frame. The LPC gains
 *                            are encoded and inserted after this function is
 *                            called. The first A-coefficient which is 1 is not
 *                            included.
 *
 * Return value             : 0 if encoding is successful,
 *                           <0 if failed to encode.
 */
WebRtc_Word16 WebRtcIsac_DecodeInterpolLpcUb(
    Bitstr*     streamdata,
    double*     percepFilterParam,
    WebRtc_Word16 bandwidth);

/* decode & dequantize RC */
int WebRtcIsac_DecodeRc(Bitstr *streamdata, WebRtc_Word16 *RCQ15);

/* quantize & code RC */
void WebRtcIsac_EncodeRc(WebRtc_Word16 *RCQ15, Bitstr *streamdata);

/* decode & dequantize squared Gain */
int WebRtcIsac_DecodeGain2(Bitstr *streamdata, WebRtc_Word32 *Gain2);

/* quantize & code squared Gain (input is squared gain) */
int WebRtcIsac_EncodeGain2(WebRtc_Word32 *gain2, Bitstr *streamdata);

void WebRtcIsac_EncodePitchGain(WebRtc_Word16* PitchGains_Q12, Bitstr* streamdata,  ISAC_SaveEncData_t* encData);

void WebRtcIsac_EncodePitchLag(double* PitchLags, WebRtc_Word16* PitchGain_Q12, Bitstr* streamdata, ISAC_SaveEncData_t* encData);

int WebRtcIsac_DecodePitchGain(Bitstr *streamdata, WebRtc_Word16 *PitchGain_Q12);
int WebRtcIsac_DecodePitchLag(Bitstr *streamdata, WebRtc_Word16 *PitchGain_Q12, double *PitchLag);

int WebRtcIsac_DecodeFrameLen(Bitstr *streamdata, WebRtc_Word16 *framelength);
int WebRtcIsac_EncodeFrameLen(WebRtc_Word16 framelength, Bitstr *streamdata);
int WebRtcIsac_DecodeSendBW(Bitstr *streamdata, WebRtc_Word16 *BWno);
void WebRtcIsac_EncodeReceiveBw(int *BWno, Bitstr *streamdata);

/* step-down */
void WebRtcIsac_Poly2Rc(double *a, int N, double *RC);

/* step-up */
void WebRtcIsac_Rc2Poly(double *RC, int N, double *a);

void WebRtcIsac_TranscodeLPCCoef(double *LPCCoef_lo, double *LPCCoef_hi, int model,
                                 int *index_g);


/******************************************************************************
 * WebRtcIsac_EncodeLpcGainUb()
 * Encode LPC gains of sub-Frames.
 *
 * Input/outputs:
 *  - lpGains               : a buffer which contains 'SUBFRAME' number of
 *                            LP gains to be encoded. The input values are
 *                            overwritten by the quantized values.
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - lpcGainIndex          : quantization indices for lpc gains, these will
 *                            be stored to be used  for FEC.
 */
void WebRtcIsac_EncodeLpcGainUb(
    double* lpGains,
    Bitstr* streamdata,
    int*    lpcGainIndex);


/******************************************************************************
 * WebRtcIsac_EncodeLpcGainUb()
 * Store LPC gains of sub-Frames in 'streamdata'.
 *
 * Input:
 *  - lpGains               : a buffer which contains 'SUBFRAME' number of
 *                            LP gains to be encoded.
 * Input/outputs:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 */
void WebRtcIsac_StoreLpcGainUb(
    double* lpGains,
    Bitstr* streamdata);


/******************************************************************************
 * WebRtcIsac_DecodeLpcGainUb()
 * Decode the LPC gain of sub-frames.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - lpGains               : a buffer where decoded LPC gians will be stored.
 *
 * Return value             : 0 if succeeded.
 *                           <0 if failed.
 */
WebRtc_Word16 WebRtcIsac_DecodeLpcGainUb(
    double* lpGains,
    Bitstr* streamdata);


/******************************************************************************
 * WebRtcIsac_EncodeBandwidth()
 * Encode if the bandwidth of encoded audio is 0-12 kHz or 0-16 kHz.
 *
 * Input:
 *  - bandwidth             : an enumerator specifying if the codec in is
 *                            0-12 kHz or 0-16 kHz mode.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Return value             : 0 if succeeded.
 *                           <0 if failed.
 */
WebRtc_Word16 WebRtcIsac_EncodeBandwidth(
    enum ISACBandwidth bandwidth,
    Bitstr*            streamData);


/******************************************************************************
 * WebRtcIsac_DecodeBandwidth()
 * Decode the bandwidth of the encoded audio, i.e. if the bandwidth is 0-12 kHz
 * or 0-16 kHz.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - bandwidth             : an enumerator specifying if the codec is in
 *                            0-12 kHz or 0-16 kHz mode.
 *
 * Return value             : 0 if succeeded.
 *                           <0 if failed.
 */
WebRtc_Word16 WebRtcIsac_DecodeBandwidth(
    Bitstr*             streamData,
    enum ISACBandwidth* bandwidth);


/******************************************************************************
 * WebRtcIsac_EncodeJitterInfo()
 * Decode the jitter information.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Input:
 *  - jitterInfo            : one bit of info specifying if the channel is
 *                            in high/low jitter. Zero indicates low jitter
 *                            and one indicates high jitter.
 *
 * Return value             : 0 if succeeded.
 *                           <0 if failed.
 */
WebRtc_Word16 WebRtcIsac_EncodeJitterInfo(
    WebRtc_Word32 jitterIndex,
    Bitstr*     streamData);


/******************************************************************************
 * WebRtcIsac_DecodeJitterInfo()
 * Decode the jitter information.
 *
 * Input/output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *  - jitterInfo            : one bit of info specifying if the channel is
 *                            in high/low jitter. Zero indicates low jitter
 *                            and one indicates high jitter.
 *
 * Return value             : 0 if succeeded.
 *                           <0 if failed.
 */
WebRtc_Word16 WebRtcIsac_DecodeJitterInfo(
    Bitstr*      streamData,
    WebRtc_Word32* jitterInfo);

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENTROPY_CODING_H_ */
