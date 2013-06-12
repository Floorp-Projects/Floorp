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
 * codec.h
 *
 * This header file contains the calls to the internal encoder
 * and decoder functions.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_CODEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_CODEC_H_

#include "structs.h"

#ifdef __cplusplus
extern "C" {
#endif

int WebRtcIsacfix_EstimateBandwidth(BwEstimatorstr   *bwest_str,
                                    Bitstr_dec       *streamdata,
                                    WebRtc_Word32      packet_size,
                                    WebRtc_UWord16     rtp_seq_number,
                                    WebRtc_UWord32     send_ts,
                                    WebRtc_UWord32     arr_ts);

WebRtc_Word16 WebRtcIsacfix_DecodeImpl(WebRtc_Word16   *signal_out16,
                                       ISACFIX_DecInst_t  *ISACdec_obj,
                                       WebRtc_Word16        *current_framesamples);

WebRtc_Word16 WebRtcIsacfix_DecodePlcImpl(WebRtc_Word16       *decoded,
                                          ISACFIX_DecInst_t *ISACdec_obj,
                                          WebRtc_Word16       *current_framesample );

int WebRtcIsacfix_EncodeImpl(WebRtc_Word16      *in,
                             ISACFIX_EncInst_t  *ISACenc_obj,
                             BwEstimatorstr      *bw_estimatordata,
                             WebRtc_Word16         CodingMode);

int WebRtcIsacfix_EncodeStoredData(ISACFIX_EncInst_t  *ISACenc_obj,
                                   int     BWnumber,
                                   float              scale);

/* initialization functions */

void WebRtcIsacfix_InitMaskingEnc(MaskFiltstr_enc *maskdata);
void WebRtcIsacfix_InitMaskingDec(MaskFiltstr_dec *maskdata);

void WebRtcIsacfix_InitPreFilterbank(PreFiltBankstr *prefiltdata);

void WebRtcIsacfix_InitPostFilterbank(PostFiltBankstr *postfiltdata);

void WebRtcIsacfix_InitPitchFilter(PitchFiltstr *pitchfiltdata);

void WebRtcIsacfix_InitPitchAnalysis(PitchAnalysisStruct *State);

void WebRtcIsacfix_InitPlc( PLCstr *State );


/* transform functions */

void WebRtcIsacfix_InitTransform();


void WebRtcIsacfix_Time2Spec(WebRtc_Word16 *inre1Q9,
                             WebRtc_Word16 *inre2Q9,
                             WebRtc_Word16 *outre,
                             WebRtc_Word16 *outim);

typedef void (*Spec2Time)(WebRtc_Word16* inreQ7,
                          WebRtc_Word16* inimQ7,
                          WebRtc_Word32* outre1Q16,
                          WebRtc_Word32* outre2Q16);
extern Spec2Time WebRtcIsacfix_Spec2Time;

void WebRtcIsacfix_Spec2TimeC(WebRtc_Word16* inreQ7,
                              WebRtc_Word16* inimQ7,
                              WebRtc_Word32* outre1Q16,
                              WebRtc_Word32* outre2Q16);

#if (defined WEBRTC_DETECT_ARM_NEON) || (defined WEBRTC_ARCH_ARM_NEON)
void WebRtcIsacfix_Spec2TimeNeon(WebRtc_Word16* inreQ7,
                                 WebRtc_Word16* inimQ7,
                                 WebRtc_Word32* outre1Q16,
                                 WebRtc_Word32* outre2Q16);
#endif




/* filterbank functions */

void WebRtcIsacfix_SplitAndFilter1(WebRtc_Word16    *in,
                                   WebRtc_Word16    *LP16,
                                   WebRtc_Word16    *HP16,
                                   PreFiltBankstr *prefiltdata);

void WebRtcIsacfix_FilterAndCombine1(WebRtc_Word16     *tempin_ch1,
                                     WebRtc_Word16     *tempin_ch2,
                                     WebRtc_Word16     *out16,
                                     PostFiltBankstr *postfiltdata);

#ifdef WEBRTC_ISAC_FIX_NB_CALLS_ENABLED

void WebRtcIsacfix_SplitAndFilter2(WebRtc_Word16    *in,
                                   WebRtc_Word16    *LP16,
                                   WebRtc_Word16    *HP16,
                                   PreFiltBankstr *prefiltdata);

void WebRtcIsacfix_FilterAndCombine2(WebRtc_Word16     *tempin_ch1,
                                     WebRtc_Word16     *tempin_ch2,
                                     WebRtc_Word16     *out16,
                                     PostFiltBankstr *postfiltdata,
                                     WebRtc_Word16     len);

#endif

/* normalized lattice filters */

void WebRtcIsacfix_NormLatticeFilterMa(WebRtc_Word16 orderCoef,
                                       WebRtc_Word32 *stateGQ15,
                                       WebRtc_Word16 *lat_inQ0,
                                       WebRtc_Word16 *filt_coefQ15,
                                       WebRtc_Word32 *gain_lo_hiQ17,
                                       WebRtc_Word16 lo_hi,
                                       WebRtc_Word16 *lat_outQ9);

void WebRtcIsacfix_NormLatticeFilterAr(WebRtc_Word16 orderCoef,
                                       WebRtc_Word16 *stateGQ0,
                                       WebRtc_Word32 *lat_inQ25,
                                       WebRtc_Word16 *filt_coefQ15,
                                       WebRtc_Word32 *gain_lo_hiQ17,
                                       WebRtc_Word16 lo_hi,
                                       WebRtc_Word16 *lat_outQ0);

/* TODO(kma): Remove the following functions into individual header files. */

/* Internal functions in both C and ARM Neon versions */

int WebRtcIsacfix_AutocorrC(WebRtc_Word32* __restrict r,
                            const WebRtc_Word16* __restrict x,
                            WebRtc_Word16 N,
                            WebRtc_Word16 order,
                            WebRtc_Word16* __restrict scale);

void WebRtcIsacfix_FilterMaLoopC(int16_t input0,
                                 int16_t input1,
                                 int32_t input2,
                                 int32_t* ptr0,
                                 int32_t* ptr1,
                                 int32_t* ptr2);

#if (defined WEBRTC_DETECT_ARM_NEON) || (defined WEBRTC_ARCH_ARM_NEON)
int WebRtcIsacfix_AutocorrNeon(WebRtc_Word32* __restrict r,
                               const WebRtc_Word16* __restrict x,
                               WebRtc_Word16 N,
                               WebRtc_Word16 order,
                               WebRtc_Word16* __restrict scale);

void WebRtcIsacfix_FilterMaLoopNeon(int16_t input0,
                                    int16_t input1,
                                    int32_t input2,
                                    int32_t* ptr0,
                                    int32_t* ptr1,
                                    int32_t* ptr2);
#endif

/* Function pointers associated with the above functions. */

typedef int (*AutocorrFix)(WebRtc_Word32* __restrict r,
                           const WebRtc_Word16* __restrict x,
                           WebRtc_Word16 N,
                           WebRtc_Word16 order,
                           WebRtc_Word16* __restrict scale);
extern AutocorrFix WebRtcIsacfix_AutocorrFix;

typedef void (*FilterMaLoopFix)(int16_t input0,
                                int16_t input1,
                                int32_t input2,
                                int32_t* ptr0,
                                int32_t* ptr1,
                                int32_t* ptr2);
extern FilterMaLoopFix WebRtcIsacfix_FilterMaLoopFix;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_CODEC_H_ */
