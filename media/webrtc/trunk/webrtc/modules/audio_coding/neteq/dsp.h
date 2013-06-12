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
 * This file contains some DSP initialization functions,
 * constant table definitions and other parameters.
 * Also contains definitions of all DSP-side data structures. 
 */


#ifndef DSP_H
#define DSP_H

#include "typedefs.h"

#include "webrtc_cng.h"

#include "codec_db_defines.h"
#include "neteq_defines.h"
#include "neteq_statistics.h"

#ifdef NETEQ_ATEVENT_DECODE
#include "dtmf_tonegen.h"
#endif



/*****************************/
/* Pre-processor definitions */
/*****************************/

/* FSMULT is the sample rate divided by 8000 */
#if defined(NETEQ_48KHZ_WIDEBAND)
	#define FSMULT	6
#elif defined(NETEQ_32KHZ_WIDEBAND)
	#define FSMULT	4
#elif defined(NETEQ_WIDEBAND)
	#define FSMULT 2
#else
	#define FSMULT 1
#endif

/* Size of the speech buffer (or synchronization buffer). */
/* 60 ms decoding + 10 ms syncbuff + 0.625ms lookahead */
#define SPEECH_BUF_SIZE (565 * FSMULT)

/* Misc definitions */
#define BGN_LPC_ORDER				(4 + FSMULT)  /* 5, 6, 8, or 10 */
#define UNVOICED_LPC_ORDER			6
#define RANDVEC_NO_OF_SAMPLES		256

/* Number of milliseconds to remove/add during accelerate/pre-emptive expand
   under BGNonly operation */
#define DEFAULT_TIME_ADJUST 8

/* Number of RecOut calls without CNG/SID before re-enabling post-decode VAD */
#define POST_DECODE_VAD_AUTO_ENABLE 3000  

/* 8kHz windowing in Q15 (over 5 samples) */
#define NETEQ_OVERLAP_WINMUTE_8KHZ_START	27307
#define NETEQ_OVERLAP_WINMUTE_8KHZ_INC		-5461
#define NETEQ_OVERLAP_WINUNMUTE_8KHZ_START	 5461
#define NETEQ_OVERLAP_WINUNMUTE_8KHZ_INC	 5461
/* 16kHz windowing in Q15 (over 10 samples) */
#define NETEQ_OVERLAP_WINMUTE_16KHZ_START	29789
#define NETEQ_OVERLAP_WINMUTE_16KHZ_INC		-2979
#define NETEQ_OVERLAP_WINUNMUTE_16KHZ_START	 2979
#define NETEQ_OVERLAP_WINUNMUTE_16KHZ_INC	 2979
/* 32kHz windowing in Q15 (over 20 samples) */
#define NETEQ_OVERLAP_WINMUTE_32KHZ_START	31208
#define NETEQ_OVERLAP_WINMUTE_32KHZ_INC		-1560
#define NETEQ_OVERLAP_WINUNMUTE_32KHZ_START	 1560
#define NETEQ_OVERLAP_WINUNMUTE_32KHZ_INC	 1560
/* 48kHz windowing in Q15 (over 30 samples) */
#define NETEQ_OVERLAP_WINMUTE_48KHZ_START	31711
#define NETEQ_OVERLAP_WINMUTE_48KHZ_INC		-1057
#define NETEQ_OVERLAP_WINUNMUTE_48KHZ_START	 1057
#define NETEQ_OVERLAP_WINUNMUTE_48KHZ_INC	 1057

/* Fade BGN towards zero after this many Expand calls */
#define FADE_BGN_TIME 200


/*******************/
/* Constant tables */
/*******************/

extern const int16_t WebRtcNetEQ_kDownsample8kHzTbl[];
extern const int16_t WebRtcNetEQ_kDownsample16kHzTbl[];
extern const int16_t WebRtcNetEQ_kDownsample32kHzTbl[];
extern const int16_t WebRtcNetEQ_kDownsample48kHzTbl[];
extern const int16_t WebRtcNetEQ_kRandnTbl[];
extern const int16_t WebRtcNetEQ_kMixFractionFuncTbl[];
extern const int16_t WebRtcNetEQ_k1049div[];
extern const int16_t WebRtcNetEQ_k2097div[];
extern const int16_t WebRtcNetEQ_k5243div[];



/************/
/* Typedefs */
/************/

enum BGNMode
{
    BGN_ON,     /* default "normal" behavior with eternal noise */
    BGN_FADE,   /* noise fades to zero after some time */
    BGN_OFF     /* background noise is always zero */
};

#ifdef NETEQ_STEREO
enum MasterSlaveMode
{
    NETEQ_MONO,     /* stand-alone instance */
    NETEQ_MASTER,   /* master instance in a spatial/stereo configuration */
    NETEQ_SLAVE     /* slave instance in a spatial/stereo configuration */
};

enum MasterSlaveExtraInfo
{
    NO_INFO,        /* no info to convey */
    ACC_FAIL,       /* signal that accelerate failed */
    PE_EXP_FAIL,    /* signal that pre-emptive expand failed */
    DTMF_OVERDUB,   /* signal that DTMF overdub is generated */
    DTMF_ONLY       /* signal that DTMF only is played */
};
#endif

/****************************/
/* DSP-side data structures */
/****************************/

/* Background noise (BGN) instance for storing BGN parameters 
 (sub-instance of NETEQDSP_inst) */
typedef struct BGNInst_t_
{

    int32_t w32_energy;
    int32_t w32_energyMax;
    int32_t w32_energyUpdate;
    int32_t w32_energyUpdateLow;
    int16_t pw16_filterState[BGN_LPC_ORDER];
    int16_t pw16_filter[BGN_LPC_ORDER + 1];
    int16_t w16_mutefactor;
    int16_t w16_scale;
    int16_t w16_scaleShift;
    int16_t w16_initialized;
    enum BGNMode bgnMode;

} BGNInst_t;

/* Expansion instance (sub-instance of NETEQDSP_inst) */
typedef struct ExpandInst_t_
{

    int16_t w16_overlap; /* Constant, 5 for NB and 10 for WB */
    int16_t w16_consecExp; /* Number of consecutive expand calls */
    int16_t *pw16_arFilter; /* length [UNVOICED_LPC_ORDER+1]	*/
    int16_t *pw16_arState; /* length [UNVOICED_LPC_ORDER]		*/
    int16_t w16_arGain;
    int16_t w16_arGainScale;
    int16_t w16_vFraction; /* Q14 */
    int16_t w16_currentVFraction; /* Q14 */
    int16_t *pw16_expVecs[2];
    int16_t w16_lags[3];
    int16_t w16_maxLag;
    int16_t *pw16_overlapVec; /* last samples of speech history */
    int16_t w16_lagsDirection;
    int16_t w16_lagsPosition;
    int16_t w16_expandMuteFactor; /* Q14 */
    int16_t w16_stopMuting;
    int16_t w16_onset;
    int16_t w16_muteSlope; /* Q20 */

} ExpandInst_t;

#ifdef NETEQ_VAD

/*
 * VAD function pointer types, replicating the typedefs in webrtc_neteq_internal.h.
 * These function pointers match the definitions of WebRtc VAD functions WebRtcVad_Init,
 * WebRtcVad_set_mode and WebRtcVad_Process, respectively, all found in webrtc_vad.h.
 */
typedef int (*VADInitFunction)(void *VAD_inst);
typedef int (*VADSetmodeFunction)(void *VAD_inst, int mode);
typedef int (*VADFunction)(void *VAD_inst, int fs, int16_t *frame,
                           int frameLen);

/* Post-decode VAD instance (sub-instance of NETEQDSP_inst) */
typedef struct PostDecodeVAD_t_
{

    void *VADState; /* pointer to a VAD instance */

    int16_t VADEnabled; /* 1 if enabled, 0 if disabled */
    int VADMode; /* mode parameter to pass to the VAD function */
    int VADDecision; /* 1 for active, 0 for passive */
    int16_t SIDintervalCounter; /* reset when decoding CNG/SID frame,
     increment for each recout call */

    /* Function pointers */
    VADInitFunction initFunction; /* VAD init function */
    VADSetmodeFunction setmodeFunction; /* VAD setmode function */
    VADFunction VADFunction; /* VAD function */

} PostDecodeVAD_t;

#endif /* NETEQ_VAD */

#ifdef NETEQ_STEREO
#define MAX_MS_DECODES 10

typedef struct 
{
    /* Stand-alone, master, or slave */
    enum MasterSlaveMode    msMode;

    enum MasterSlaveExtraInfo  extraInfo;

    uint16_t instruction;
    int16_t distLag;
    int16_t corrLag;
    int16_t bestIndex;

    uint32_t endTimestamp;
    uint16_t samplesLeftWithOverlap;

} MasterSlaveInfo;
#endif


/* "Main" NetEQ DSP instance */
typedef struct DSPInst_t_
{

    /* MCU/DSP Communication layer */
    int16_t *pw16_readAddress;
    int16_t *pw16_writeAddress;
    void *main_inst;

    /* Output frame size in ms and samples */
    int16_t millisecondsPerCall;
    int16_t timestampsPerCall;

    /*
     *	Example of speech buffer
     *
     *  -----------------------------------------------------------
     *  |            History  T-60 to T         |     Future      |
     *	-----------------------------------------------------------
     *						                    ^			      ^
     *					                    	|			      |
     *					                   curPosition	   endPosition
     *
     *		History is gradually shifted out to the left when inserting
     *      new data at the end.
     */

    int16_t speechBuffer[SPEECH_BUF_SIZE]; /* History/future speech buffer */
    int curPosition; /* Next sample to play */
    int endPosition; /* Position that ends future data */
    uint32_t endTimestamp; /* Timestamp value at end of future data */
    uint32_t videoSyncTimestamp; /* (Estimated) timestamp of the last
     played sample (usually same as
     endTimestamp-(endPosition-curPosition)
     except during Expand and CNG) */
    uint16_t fs; /* sample rate in Hz */
    int16_t w16_frameLen; /* decoder frame length in samples */
    int16_t w16_mode; /* operation used during last RecOut call */
    int16_t w16_muteFactor; /* speech mute factor in Q14 */
    int16_t *pw16_speechHistory; /* beginning of speech history during Expand */
    int16_t w16_speechHistoryLen; /* 256 for NB and 512 for WB */

    /* random noise seed parameters */
    int16_t w16_seedInc;
    uint32_t uw16_seed;

    /* VQmon related variable */
    int16_t w16_concealedTS;

    /*****************/
    /* Sub-instances */
    /*****************/

    /* Decoder data */
    CodecFuncInst_t codec_ptr_inst;

#ifdef NETEQ_CNG_CODEC
    /* CNG "decoder" instance */
    CNG_dec_inst *CNG_Codec_inst;
#endif /* NETEQ_CNG_CODEC */

#ifdef NETEQ_ATEVENT_DECODE
    /* DTMF generator instance */
    dtmf_tone_inst_t DTMFInst;
#endif /* NETEQ_CNG_CODEC */

#ifdef NETEQ_VAD
    /* Post-decode VAD instance */
    PostDecodeVAD_t VADInst;
#endif /* NETEQ_VAD */

    /* Expand instance (defined above) */
    ExpandInst_t ExpandInst;

    /* Background noise instance (defined above) */
    BGNInst_t BGNInst;

    /* Internal statistics instance */
    DSPStats_t statInst;

    /* Internal instance for short-term processing activity. */
    ActivityStats activity_stats;

#ifdef NETEQ_STEREO
    /* Pointer to Master/Slave info */
    MasterSlaveInfo *msInfo;
#endif

} DSPInst_t;


/*************************/
/* Function declarations */
/*************************/

/****************************************************************************
 * WebRtcNetEQ_DSPInit(...)
 *
 * Initializes DSP side of NetEQ.
 *
 * Input:
 *		- inst			: NetEq DSP instance 
 *      - fs            : Initial sample rate (may change when decoding data)
 *
 * Output:
 *		- inst			: Updated instance
 *
 * Return value			: 0 - ok
 *                      : non-zero - error
 */

int WebRtcNetEQ_DSPInit(DSPInst_t *inst, uint16_t fs);

/****************************************************************************
 * WebRtcNetEQ_AddressInit(...)
 *
 * Initializes the shared-memory communication on the DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *      - data2McuAddress   : Pointer to memory where DSP writes / MCU reads
 *      - data2DspAddress   : Pointer to memory where MCU writes / DSP reads
 *      - mainInst          : NetEQ main instance
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_AddressInit(DSPInst_t *inst, const void *data2McuAddress,
                            const void *data2DspAddress, const void *mainInst);

/****************************************************************************
 * WebRtcNetEQ_ClearInCallStats(...)
 *
 * Reset in-call statistics variables on DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_ClearInCallStats(DSPInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_ClearPostCallStats(...)
 *
 * Reset post-call statistics variables on DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_ClearPostCallStats(DSPInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_ClearActivityStats(...)
 *
 * Reset processing activity statistics.
 *
 * Input:
 *    - inst          : NetEQ DSP instance
 *
 * Output:
 *    - inst          : Updated instance
 *
 */

void WebRtcNetEQ_ClearActivityStats(DSPInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_RecOutInternal(...)
 *
 * This function asks NetEQ for more speech/audio data.
 *
 * Input:
 *    - inst      : NetEQ instance, i.e. the user that requests more
 *                  speech/audio data.
 *    - outdata   : Pointer to a memory space where the output data
 *                  should be stored.
 *    - BGNonly   : If non-zero, RecOut will only produce background
 *                  noise. It will still draw packets from the packet
 *                  buffer, but they will never be decoded.
 *    - av_sync   : 1 if NetEQ is in AV-sync, 0 otherwise.
 *
 * Output:
 *    - inst      : Updated user information
 *    - len       : Number of samples that were outputted from NetEq
 *
 * Return value   : 0 - Ok
 *                 -1 - Error
 */

int WebRtcNetEQ_RecOutInternal(DSPInst_t *inst, int16_t *pw16_outData,
                               int16_t *pw16_len, int16_t BGNonly, int av_sync);

/****************************************************************************
 * WebRtcNetEQ_Normal(...)
 *
 * This function has the possibility to modify data that is played out in Normal
 * mode, for example adjust the gain of the signal. The length of the signal 
 * can not be changed.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector
 *		- decoded		: Pointer to vector of new data from decoder
 *      - len           : Number of input samples
 *
 * Output:
 *		- inst			: Updated user information
 *		- pw16_len		: Pointer to varibale where the number of samples 
 *                        produced will be written
 *
 * Return value			: >=0 - Number of samples written to outData
 *						   -1 - Error
 */

int WebRtcNetEQ_Normal(DSPInst_t *inst,
#ifdef SCRATCH
                       int16_t *pw16_scratchPtr,
#endif
                       int16_t *pw16_decoded, int16_t len,
                       int16_t *pw16_outData, int16_t *pw16_len);

/****************************************************************************
 * WebRtcNetEQ_Expand(...)
 *
 * This function produces one "chunk" of expansion data (PLC audio). The
 * lenght of the produced audio depends on the speech history.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector
 *      - BGNonly       : If non-zero, Expand will only produce background
 *                        noise.
 *      - pw16_len      : Desired number of samples (only for BGN mode).
 *
 * Output:
 *		- inst			: Updated user information
 *		- outdata		: Pointer to a memory space where the output data
 *						  should be stored
 *		- pw16_len		: Number of samples that were outputted from NetEq
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_Expand(DSPInst_t *inst,
#ifdef SCRATCH
                       int16_t *pw16_scratchPtr,
#endif
                       int16_t *pw16_outData, int16_t *pw16_len,
                       int16_t BGNonly);

/****************************************************************************
 * WebRtcNetEQ_GenerateBGN(...)
 *
 * This function generates and writes len samples of background noise to the
 * output vector. The Expand function will be called repeteadly until the
 * correct number of samples is produced.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector
 *      - len           : Desired length of produced BGN.
 *						  
 *
 * Output:
 *		- pw16_outData	: Pointer to a memory space where the output data
 *						  should be stored
 *
 * Return value			: >=0 - Number of noise samples produced and written
 *                              to output
 *						  -1  - Error
 */

int WebRtcNetEQ_GenerateBGN(DSPInst_t *inst,
#ifdef SCRATCH
                            int16_t *pw16_scratchPtr,
#endif
                            int16_t *pw16_outData, int16_t len);

/****************************************************************************
 * WebRtcNetEQ_PreEmptiveExpand(...)
 *
 * This function tries to extend the audio data by repeating one or several
 * pitch periods. The operation is only carried out if the correlation is
 * strong or if the signal energy is very low. The algorithm is the
 * reciprocal of the Accelerate algorithm.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *		- decoded	    : Pointer to newly decoded speech.
 *		- len           : Length of decoded speech.
 *      - oldDataLen    : Length of the part of decoded that has already been played out.
 *      - BGNonly       : If non-zero, Pre-emptive Expand will only copy 
 *                        the first DEFAULT_TIME_ADJUST seconds of the
 *                        input and append to the end. No signal matching is
 *                        done.
 *
 * Output:
 *		- inst			: Updated instance
 *		- outData		: Pointer to a memory space where the output data
 *						  should be stored. The vector must be at least
 *						  min(len + 120*fs/8000, NETEQ_MAX_OUTPUT_SIZE)
 *						  elements long.
 *		- pw16_len		: Number of samples written to outData.
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_PreEmptiveExpand(DSPInst_t *inst,
#ifdef SCRATCH
                                 int16_t *pw16_scratchPtr,
#endif
                                 const int16_t *pw16_decoded, int len, int oldDataLen,
                                 int16_t *pw16_outData, int16_t *pw16_len,
                                 int16_t BGNonly);

/****************************************************************************
 * WebRtcNetEQ_Accelerate(...)
 *
 * This function tries to shorten the audio data by removing one or several
 * pitch periods. The operation is only carried out if the correlation is
 * strong or if the signal energy is very low.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *		- decoded	    : Pointer to newly decoded speech.
 *		- len           : Length of decoded speech.
 *      - BGNonly       : If non-zero, Accelerate will only remove the last 
 *                        DEFAULT_TIME_ADJUST seconds of the intput.
 *                        No signal matching is done.
 *
 *
 * Output:
 *		- inst			: Updated instance
 *		- outData		: Pointer to a memory space where the output data
 *						  should be stored
 *		- pw16_len		: Number of samples written to outData.
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_Accelerate(DSPInst_t *inst,
#ifdef SCRATCH
                           int16_t *pw16_scratchPtr,
#endif
                           const int16_t *pw16_decoded, int len,
                           int16_t *pw16_outData, int16_t *pw16_len,
                           int16_t BGNonly);

/****************************************************************************
 * WebRtcNetEQ_Merge(...)
 *
 * This function is used to merge new data from the decoder to the exisiting
 * stream in the synchronization buffer. The merge operation is typically
 * done after a packet loss, where the end of the expanded data does not
 * fit naturally with the new decoded data.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *		- decoded	    : Pointer to new decoded speech.
 *      - len           : Number of samples in pw16_decoded.
 *
 *
 * Output:
 *		- inst			: Updated user information
 *		- outData	    : Pointer to a memory space where the output data
 *						  should be stored
 *		- pw16_len		: Number of samples written to pw16_outData
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_Merge(DSPInst_t *inst,
#ifdef SCRATCH
                      int16_t *pw16_scratchPtr,
#endif
                      int16_t *pw16_decoded, int len, int16_t *pw16_outData,
                      int16_t *pw16_len);

/****************************************************************************
 * WebRtcNetEQ_Cng(...)
 *
 * This function produces CNG according to RFC 3389
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *		- len			: Number of samples to produce
 *
 * Output:
 *		- pw16_outData	: Output CNG
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

#ifdef NETEQ_CNG_CODEC
/* Must compile NetEQ with CNG support to enable this function */

int WebRtcNetEQ_Cng(DSPInst_t *inst, int16_t *pw16_outData, int len);

#endif /* NETEQ_CNG_CODEC */

/****************************************************************************
 * WebRtcNetEQ_BGNUpdate(...)
 *
 * This function updates the background noise parameter estimates.
 *
 * Input:
 *		- inst			: NetEQ instance, where the speech history is stored.
 *      - scratchPtr    : Pointer to scratch vector.
 *
 * Output:
 *		- inst			: Updated information about the BGN characteristics.
 *
 * Return value			: No return value
 */

void WebRtcNetEQ_BGNUpdate(
#ifdef SCRATCH
                           DSPInst_t *inst, int16_t *pw16_scratchPtr
#else
                           DSPInst_t *inst
#endif
                );

#ifdef NETEQ_VAD
/* Functions used by post-decode VAD */

/****************************************************************************
 * WebRtcNetEQ_InitVAD(...)
 *
 * Initializes post-decode VAD instance.
 *
 * Input:
 *		- VADinst		: PostDecodeVAD instance
 *      - fs            : Initial sample rate
 *
 * Output:
 *		- VADinst		: Updated instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_InitVAD(PostDecodeVAD_t *VADInst, uint16_t fs);

/****************************************************************************
 * WebRtcNetEQ_SetVADModeInternal(...)
 *
 * Set the VAD mode in the VAD struct, and communicate it to the VAD instance 
 * if it exists.
 *
 * Input:
 *		- VADinst		: PostDecodeVAD instance
 *      - mode          : Mode number passed on to the VAD function
 *
 * Output:
 *		- VADinst		: Updated instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_SetVADModeInternal(PostDecodeVAD_t *VADInst, int mode);

#endif /* NETEQ_VAD */

/****************************************************************************
 * WebRtcNetEQ_FlushSpeechBuffer(...)
 *
 * Flush the speech buffer.
 *
 * Input:
 *		- inst			: NetEq DSP instance 
 *
 * Output:
 *		- inst			: Updated instance
 *
 * Return value			: 0 - ok
 *                      : non-zero - error
 */

int WebRtcNetEQ_FlushSpeechBuffer(DSPInst_t *inst);

#ifndef WEBRTC_NETEQ_40BITACC_TEST

#include "signal_processing_library.h"
/* Map to regular SPL functions */
#define WebRtcNetEQ_CrossCorr   WebRtcSpl_CrossCorrelation
#define WebRtcNetEQ_DotW16W16   WebRtcSpl_DotProductWithScale

#else /* WEBRTC_NETEQ_40BITACC_TEST defined */
/* Run NetEQ with simulated 40-bit accumulator to run bit-exact to a DSP 
 implementation where the main (splib and NetEQ) functions have been
 40-bit optimized. */

/* Map to special 40-bit optimized functions, defined below */
#define WebRtcNetEQ_CrossCorr		WebRtcNetEQ_40BitAccCrossCorr
#define WebRtcNetEQ_DotW16W16	    WebRtcNetEQ_40BitAccDotW16W16

/****************************************************************************
 * WebRtcNetEQ_40BitAccCrossCorr(...)
 *
 * Calculates the Cross correlation between two sequences seq1 and seq2. Seq1
 * is fixed and seq2 slides as the pointer is increased with step
 *
 * Input:
 *		- seq1			: First sequence (fixed throughout the correlation)
 *		- seq2			: Second sequence (slided step_seq2 for each 
 *						  new correlation)
 *		- dimSeq		: Number of samples to use in the cross correlation.
 *                        Should be no larger than 1024 to avoid overflow.
 *		- dimCrossCorr	: Number of CrossCorrelations to calculate (start 
 *						  position for seq2 is updated for each new one)
 *		- rShift			: Number of right shifts to use
 *		- step_seq2		: How many (positive or negative) steps the seq2 
 *						  pointer should be updated for each new cross 
 *						  correlation value
 *
 * Output:
 *		- crossCorr		: The cross correlation in Q-rShift
 */

void WebRtcNetEQ_40BitAccCrossCorr(int32_t *crossCorr, int16_t *seq1,
                                   int16_t *seq2, int16_t dimSeq,
                                   int16_t dimCrossCorr, int16_t rShift,
                                   int16_t step_seq2);

/****************************************************************************
 * WebRtcNetEQ_40BitAccDotW16W16(...)
 *
 * Calculates the dot product between two vectors (int16_t)
 *
 * Input:
 *		- vector1		: Vector 1
 *		- vector2		: Vector 2
 *		- len			: Number of samples in vector
 *                        Should be no larger than 1024 to avoid overflow.
 *		- scaling		: The number of right shifts (after multiplication)
 *                        required to avoid overflow in the dot product.
 * Return value			: The dot product
 */

int32_t WebRtcNetEQ_40BitAccDotW16W16(int16_t *vector1, int16_t *vector2,
                                      int len, int scaling);

#endif /* WEBRTC_NETEQ_40BITACC_TEST */

#endif /* DSP_H */
