/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs echo control (suppression) with fft routines in fixed-point

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AECM_MAIN_SOURCE_AECM_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AECM_MAIN_SOURCE_AECM_CORE_H_

#define AECM_DYNAMIC_Q // turn on/off dynamic Q-domain
//#define AECM_WITH_ABS_APPROX
//#define AECM_SHORT                // for 32 sample partition length (otherwise 64)

#include "typedefs.h"
#include "signal_processing_library.h"

// Algorithm parameters

#define FRAME_LEN       80              // Total frame length, 10 ms
#ifdef AECM_SHORT

#define PART_LEN        32              // Length of partition
#define PART_LEN_SHIFT  6               // Length of (PART_LEN * 2) in base 2

#else

#define PART_LEN        64              // Length of partition
#define PART_LEN_SHIFT  7               // Length of (PART_LEN * 2) in base 2

#endif

#define PART_LEN1       (PART_LEN + 1)  // Unique fft coefficients
#define PART_LEN2       (PART_LEN << 1) // Length of partition * 2
#define PART_LEN4       (PART_LEN << 2) // Length of partition * 4
#define FAR_BUF_LEN     PART_LEN4       // Length of buffers
#define MAX_DELAY 100

// Counter parameters
#ifdef AECM_SHORT

#define CONV_LEN        1024            // Convergence length used at startup
#else

#define CONV_LEN        512             // Convergence length used at startup
#endif

#define CONV_LEN2       (CONV_LEN << 1) // Convergence length * 2 used at startup
// Energy parameters
#define MAX_BUF_LEN     64              // History length of energy signals

#define FAR_ENERGY_MIN  1025            // Lowest Far energy level: At least 2 in energy
#define FAR_ENERGY_DIFF 929             // Allowed difference between max and min

#define ENERGY_DEV_OFFSET       0       // The energy error offset in Q8
#define ENERGY_DEV_TOL  400             // The energy estimation tolerance in Q8
#define FAR_ENERGY_VAD_REGION   230     // Far VAD tolerance region
// Stepsize parameters
#define MU_MIN          10              // Min stepsize 2^-MU_MIN (far end energy dependent)
#define MU_MAX          1               // Max stepsize 2^-MU_MAX (far end energy dependent)
#define MU_DIFF         9               // MU_MIN - MU_MAX
// Channel parameters
#define MIN_MSE_COUNT   20              // Min number of consecutive blocks with enough far end
                                        // energy to compare channel estimates
#define MIN_MSE_DIFF    29              // The ratio between adapted and stored channel to
                                        // accept a new storage (0.8 in Q-MSE_RESOLUTION)
#define MSE_RESOLUTION  5               // MSE parameter resolution
#define RESOLUTION_CHANNEL16    12      // W16 Channel in Q-RESOLUTION_CHANNEL16
#define RESOLUTION_CHANNEL32    28      // W32 Channel in Q-RESOLUTION_CHANNEL
#define CHANNEL_VAD     16              // Minimum energy in frequency band to update channel
// Suppression gain parameters: SUPGAIN_ parameters in Q-(RESOLUTION_SUPGAIN)
#define RESOLUTION_SUPGAIN      8       // Channel in Q-(RESOLUTION_SUPGAIN)
#define SUPGAIN_DEFAULT (1 << RESOLUTION_SUPGAIN)   // Default suppression gain
#define SUPGAIN_ERROR_PARAM_A   3072    // Estimation error parameter (Maximum gain) (8 in Q8)
#define SUPGAIN_ERROR_PARAM_B   1536    // Estimation error parameter (Gain before going down)
#define SUPGAIN_ERROR_PARAM_D   SUPGAIN_DEFAULT // Estimation error parameter
                                                // (Should be the same as Default) (1 in Q8)
#define SUPGAIN_EPC_DT  200             // = SUPGAIN_ERROR_PARAM_C * ENERGY_DEV_TOL
// Defines for "check delay estimation"
#define CORR_WIDTH      31              // Number of samples to correlate over.
#define CORR_MAX        16              // Maximum correlation offset
#define CORR_MAX_BUF    63
#define CORR_DEV        4
#define CORR_MAX_LEVEL  20
#define CORR_MAX_LOW    4
#define CORR_BUF_LEN    (CORR_MAX << 1) + 1
// Note that CORR_WIDTH + 2*CORR_MAX <= MAX_BUF_LEN

#define ONE_Q14         (1 << 14)

// NLP defines
#define NLP_COMP_LOW    3277            // 0.2 in Q14
#define NLP_COMP_HIGH   ONE_Q14         // 1 in Q14

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

// Initialization of the above function pointers for ARM Neon.
void WebRtcAecm_InitNeon(void);


#endif
