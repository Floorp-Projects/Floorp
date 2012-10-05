/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs echo control (suppression) with fft routines in fixed-point

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AECM_AECM_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AECM_AECM_CORE_H_

#include "typedefs.h"
#include "signal_processing_library.h"

#include "aecm_defines.h"

extern const WebRtc_Word16 WebRtcAecm_kSqrtHanning[];

typedef struct {
    WebRtc_Word16 real;
    WebRtc_Word16 imag;
} complex16_t;

typedef struct
{
    int farBufWritePos;
    int farBufReadPos;
    int knownDelay;
    int lastKnownDelay;
    int firstVAD; // Parameter to control poorly initialized channels

    void *farFrameBuf;
    void *nearNoisyFrameBuf;
    void *nearCleanFrameBuf;
    void *outFrameBuf;

    WebRtc_Word16 farBuf[FAR_BUF_LEN];

    WebRtc_Word16 mult;
    WebRtc_UWord32 seed;

    // Delay estimation variables
    void* delay_estimator;
    WebRtc_UWord16 currentDelay;
    // Far end history variables
    // TODO(bjornv): Replace |far_history| with ring_buffer.
    uint16_t far_history[PART_LEN1 * MAX_DELAY];
    int far_history_pos;
    int far_q_domains[MAX_DELAY];

    WebRtc_Word16 nlpFlag;
    WebRtc_Word16 fixedDelay;

    WebRtc_UWord32 totCount;

    WebRtc_Word16 dfaCleanQDomain;
    WebRtc_Word16 dfaCleanQDomainOld;
    WebRtc_Word16 dfaNoisyQDomain;
    WebRtc_Word16 dfaNoisyQDomainOld;

    WebRtc_Word16 nearLogEnergy[MAX_BUF_LEN];
    WebRtc_Word16 farLogEnergy;
    WebRtc_Word16 echoAdaptLogEnergy[MAX_BUF_LEN];
    WebRtc_Word16 echoStoredLogEnergy[MAX_BUF_LEN];

    // The extra 16 or 32 bytes in the following buffers are for alignment based Neon code.
    // It's designed this way since the current GCC compiler can't align a buffer in 16 or 32
    // byte boundaries properly.
    WebRtc_Word16 channelStored_buf[PART_LEN1 + 8];
    WebRtc_Word16 channelAdapt16_buf[PART_LEN1 + 8];
    WebRtc_Word32 channelAdapt32_buf[PART_LEN1 + 8];
    WebRtc_Word16 xBuf_buf[PART_LEN2 + 16]; // farend
    WebRtc_Word16 dBufClean_buf[PART_LEN2 + 16]; // nearend
    WebRtc_Word16 dBufNoisy_buf[PART_LEN2 + 16]; // nearend
    WebRtc_Word16 outBuf_buf[PART_LEN + 8];

    // Pointers to the above buffers
    WebRtc_Word16 *channelStored;
    WebRtc_Word16 *channelAdapt16;
    WebRtc_Word32 *channelAdapt32;
    WebRtc_Word16 *xBuf;
    WebRtc_Word16 *dBufClean;
    WebRtc_Word16 *dBufNoisy;
    WebRtc_Word16 *outBuf;

    WebRtc_Word32 echoFilt[PART_LEN1];
    WebRtc_Word16 nearFilt[PART_LEN1];
    WebRtc_Word32 noiseEst[PART_LEN1];
    int           noiseEstTooLowCtr[PART_LEN1];
    int           noiseEstTooHighCtr[PART_LEN1];
    WebRtc_Word16 noiseEstCtr;
    WebRtc_Word16 cngMode;

    WebRtc_Word32 mseAdaptOld;
    WebRtc_Word32 mseStoredOld;
    WebRtc_Word32 mseThreshold;

    WebRtc_Word16 farEnergyMin;
    WebRtc_Word16 farEnergyMax;
    WebRtc_Word16 farEnergyMaxMin;
    WebRtc_Word16 farEnergyVAD;
    WebRtc_Word16 farEnergyMSE;
    int currentVADValue;
    WebRtc_Word16 vadUpdateCount;

    WebRtc_Word16 startupState;
    WebRtc_Word16 mseChannelCount;
    WebRtc_Word16 supGain;
    WebRtc_Word16 supGainOld;

    WebRtc_Word16 supGainErrParamA;
    WebRtc_Word16 supGainErrParamD;
    WebRtc_Word16 supGainErrParamDiffAB;
    WebRtc_Word16 supGainErrParamDiffBD;

#ifdef AEC_DEBUG
    FILE *farFile;
    FILE *nearFile;
    FILE *outFile;
#endif
} AecmCore_t;

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_CreateCore(...)
//
// Allocates the memory needed by the AECM. The memory needs to be
// initialized separately using the WebRtcAecm_InitCore() function.
//
// Input:
//      - aecm          : Instance that should be created
//
// Output:
//      - aecm          : Created instance
//
// Return value         :  0 - Ok
//                        -1 - Error
//
int WebRtcAecm_CreateCore(AecmCore_t **aecm);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_InitCore(...)
//
// This function initializes the AECM instant created with WebRtcAecm_CreateCore(...)
// Input:
//      - aecm          : Pointer to the AECM instance
//      - samplingFreq  : Sampling Frequency
//
// Output:
//      - aecm          : Initialized instance
//
// Return value         :  0 - Ok
//                        -1 - Error
//
int WebRtcAecm_InitCore(AecmCore_t * const aecm, int samplingFreq);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_FreeCore(...)
//
// This function releases the memory allocated by WebRtcAecm_CreateCore()
// Input:
//      - aecm          : Pointer to the AECM instance
//
// Return value         :  0 - Ok
//                        -1 - Error
//           11001-11016: Error
//
int WebRtcAecm_FreeCore(AecmCore_t *aecm);

int WebRtcAecm_Control(AecmCore_t *aecm, int delay, int nlpFlag);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_InitEchoPathCore(...)
//
// This function resets the echo channel adaptation with the specified channel.
// Input:
//      - aecm          : Pointer to the AECM instance
//      - echo_path     : Pointer to the data that should initialize the echo path
//
// Output:
//      - aecm          : Initialized instance
//
void WebRtcAecm_InitEchoPathCore(AecmCore_t* aecm, const WebRtc_Word16* echo_path);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_ProcessFrame(...)
//
// This function processes frames and sends blocks to WebRtcAecm_ProcessBlock(...)
//
// Inputs:
//      - aecm          : Pointer to the AECM instance
//      - farend        : In buffer containing one frame of echo signal
//      - nearendNoisy  : In buffer containing one frame of nearend+echo signal without NS
//      - nearendClean  : In buffer containing one frame of nearend+echo signal with NS
//
// Output:
//      - out           : Out buffer, one frame of nearend signal          :
//
//
int WebRtcAecm_ProcessFrame(AecmCore_t * aecm, const WebRtc_Word16 * farend,
                            const WebRtc_Word16 * nearendNoisy,
                            const WebRtc_Word16 * nearendClean,
                            WebRtc_Word16 * out);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_ProcessBlock(...)
//
// This function is called for every block within one frame
// This function is called by WebRtcAecm_ProcessFrame(...)
//
// Inputs:
//      - aecm          : Pointer to the AECM instance
//      - farend        : In buffer containing one block of echo signal
//      - nearendNoisy  : In buffer containing one frame of nearend+echo signal without NS
//      - nearendClean  : In buffer containing one frame of nearend+echo signal with NS
//
// Output:
//      - out           : Out buffer, one block of nearend signal          :
//
//
int WebRtcAecm_ProcessBlock(AecmCore_t * aecm, const WebRtc_Word16 * farend,
                            const WebRtc_Word16 * nearendNoisy,
                            const WebRtc_Word16 * noisyClean,
                            WebRtc_Word16 * out);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_BufferFarFrame()
//
// Inserts a frame of data into farend buffer.
//
// Inputs:
//      - aecm          : Pointer to the AECM instance
//      - farend        : In buffer containing one frame of farend signal
//      - farLen        : Length of frame
//
void WebRtcAecm_BufferFarFrame(AecmCore_t * const aecm, const WebRtc_Word16 * const farend,
                               const int farLen);

///////////////////////////////////////////////////////////////////////////////////////////////
// WebRtcAecm_FetchFarFrame()
//
// Read the farend buffer to account for known delay
//
// Inputs:
//      - aecm          : Pointer to the AECM instance
//      - farend        : In buffer containing one frame of farend signal
//      - farLen        : Length of frame
//      - knownDelay    : known delay
//
void WebRtcAecm_FetchFarFrame(AecmCore_t * const aecm, WebRtc_Word16 * const farend,
                              const int farLen, const int knownDelay);

///////////////////////////////////////////////////////////////////////////////
// Some function pointers, for internal functions shared by ARM NEON and 
// generic C code.
//
typedef void (*CalcLinearEnergies)(
    AecmCore_t* aecm,
    const WebRtc_UWord16* far_spectrum,
    WebRtc_Word32* echoEst,
    WebRtc_UWord32* far_energy,
    WebRtc_UWord32* echo_energy_adapt,
    WebRtc_UWord32* echo_energy_stored);
extern CalcLinearEnergies WebRtcAecm_CalcLinearEnergies;

typedef void (*StoreAdaptiveChannel)(
    AecmCore_t* aecm,
    const WebRtc_UWord16* far_spectrum,
    WebRtc_Word32* echo_est);
extern StoreAdaptiveChannel WebRtcAecm_StoreAdaptiveChannel;

typedef void (*ResetAdaptiveChannel)(AecmCore_t* aecm);
extern ResetAdaptiveChannel WebRtcAecm_ResetAdaptiveChannel;

typedef void (*WindowAndFFT)(
    WebRtc_Word16* fft,
    const WebRtc_Word16* time_signal,
    complex16_t* freq_signal,
    int time_signal_scaling);
extern WindowAndFFT WebRtcAecm_WindowAndFFT;

typedef void (*InverseFFTAndWindow)(
    AecmCore_t* aecm,
    WebRtc_Word16* fft, complex16_t* efw,
    WebRtc_Word16* output,
    const WebRtc_Word16* nearendClean);
extern InverseFFTAndWindow WebRtcAecm_InverseFFTAndWindow;

// For the above function pointers, functions for generic platforms are declared
// and defined as static in file aecm_core.c, while those for ARM Neon platforms
// are declared below and defined in file aecm_core_neon.s.
#if (defined WEBRTC_DETECT_ARM_NEON) || defined (WEBRTC_ARCH_ARM_NEON)
void WebRtcAecm_WindowAndFFTNeon(WebRtc_Word16* fft,
                                 const WebRtc_Word16* time_signal,
                                 complex16_t* freq_signal,
                                 int time_signal_scaling);

void WebRtcAecm_InverseFFTAndWindowNeon(AecmCore_t* aecm,
                                        WebRtc_Word16* fft,
                                        complex16_t* efw,
                                        WebRtc_Word16* output,
                                        const WebRtc_Word16* nearendClean);

void WebRtcAecm_CalcLinearEnergiesNeon(AecmCore_t* aecm,
                                       const WebRtc_UWord16* far_spectrum,
                                       WebRtc_Word32* echo_est,
                                       WebRtc_UWord32* far_energy,
                                       WebRtc_UWord32* echo_energy_adapt,
                                       WebRtc_UWord32* echo_energy_stored);

void WebRtcAecm_StoreAdaptiveChannelNeon(AecmCore_t* aecm,
                                         const WebRtc_UWord16* far_spectrum,
                                         WebRtc_Word32* echo_est);

void WebRtcAecm_ResetAdaptiveChannelNeon(AecmCore_t* aecm);
#endif

#endif
