/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*****************************************************************************************
 *
 * Compilation flags in NetEQ:
 *
 *****************************************************************************************
 *
 ***** Platform flags ******
 *
 * SCRATCH                        Run NetEQ with "Scratch memory" to save some stack memory.
 *                                Definition can be used on all platforms
 *
 ***** Summary flags ******
 *
 * NETEQ_ALL_SPECIAL_CODECS       Add support for special codecs (CN/RED/DTMF)
 *
 * NETEQ_ALL_NB_CODECS            Add support for all NB codecs (except CN/RED/DTMF)
 *
 * NETEQ_ALL_WB_CODECS            Add support for all WB codecs (except CN/RED/DTMF)
 *
 * NETEQ_VOICEENGINE_CODECS       Support for all NB, WB and SWB32 codecs and CN, RED and DTMF
 *
 * NETEQ_ALL_CODECS               Support for all NB, WB, SWB 32kHz and SWB 48kHz as well as
 *                                CN, RED and DTMF
 *
 ***** Sampling frequency ****** 
 * (Note: usually not needed when Summary flags are used)
 *
 * NETEQ_WIDEBAND                 Wideband enabled
 *
 * NETEQ_32KHZ_WIDEBAND           Super wideband @ 32kHz enabled
 *
 * NETEQ_48KHZ_WIDEBAND           Super wideband @ 48kHz enabled
 *
 ***** Special Codec ****** 
 * (Note: not needed if NETEQ_ALL_CODECS is used)
 *
 * NETEQ_RED_CODEC                With this flag you enable NetEQ to understand redundancy in
 *                                the RTP. NetEQ will use the redundancy if it's the same
 *                                codec
 *
 * NETEQ_CNG_CODEC                Enable DTX with the CN payload
 *
 * NETEQ_ATEVENT_DECODE           Enable AVT event and play out the corresponding DTMF tone
 *
 ***** Speech Codecs *****
 * (Note: Not needed if Summary flags are used)
 *
 * NETEQ_G711_CODEC               Enable G.711 u- and A-law
 *
 * NETEQ_PCM16B_CODEC             Enable uncompressed 16-bit
 *
 * NETEQ_ILBC_CODEC               Enable iLBC
 *
 * NETEQ_ISAC_CODEC               Enable iSAC
 *
 * NETEQ_ISAC_SWB_CODEC           Enable iSAC-SWB
 *
 * NETEQ_G722_CODEC               Enable G.722
 *
 * NETEQ_G729_CODEC               Enable G.729
 *
 * NETEQ_G729_1_CODEC             Enable G.729.1
 *
 * NETEQ_G726_CODEC               Enable G.726
 *
 * NETEQ_G722_1_CODEC             Enable G722.1
 *
 * NETEQ_G722_1C_CODEC            Enable G722.1 Annex C
 *
 * NETEQ_SPEEX_CODEC              Enable Speex (at 8 and 16 kHz sample rate)
 *
 * NETEQ_CELT_CODEC               Enable Celt (at 32 kHz sample rate)
 *
 * NETEQ_GSMFR_CODEC              Enable GSM-FR
 *
 * NETEQ_AMR_CODEC                Enable AMR (narrowband)
 *
 * NETEQ_AMRWB_CODEC              Enable AMR-WB
 *
 * NETEQ_CNG_CODEC                Enable DTX with the CNG payload
 *
 * NETEQ_ATEVENT_DECODE           Enable AVT event and play out the corresponding DTMF tone
 *
 ***** Test flags ******
 *
 * WEBRTC_NETEQ_40BITACC_TEST     Run NetEQ with simulated 40-bit accumulator to run
 *                                bit-exact to a DSP implementation where the main (splib
 *                                and NetEQ) functions have been 40-bit optimized
 *
 *****************************************************************************************
 */

#if !defined NETEQ_DEFINES_H
#define NETEQ_DEFINES_H

/* Data block structure for MCU to DSP communication:
 *
 *
 *  First 3 16-bit words are pre-header that contains instructions and timestamp update
 *  Fourth 16-bit word is length of data block 1
 *  Rest is payload data
 *
 *  0               48          64          80
 *  -------------...----------------------------------------------------------------------
 *  |  PreHeader ... | Length 1 |  Payload data 1 ...... | Lenght 2| Data block 2....    | ...
 *  -------------...----------------------------------------------------------------------
 *
 *
 *  Preheader:
 *  4 MSB can be either of:
 */

#define DSP_INSTR_NORMAL                         0x1000
/* Payload data will contain the encoded frames */

#define DSP_INSTR_MERGE                          0x2000
/* Payload data block 1 will contain the encoded frame */
/* Info block will contain the number of missing samples */

#define DSP_INSTR_EXPAND                         0x3000
/* Payload data will be empty */

#define DSP_INSTR_ACCELERATE                     0x4000
/* Payload data will contain the encoded frame */

#define DSP_INSTR_DO_RFC3389CNG                  0x5000
/* Payload data will contain the SID frame if there is one*/

#define DSP_INSTR_DTMF_GENERATE                  0x6000
/* Payload data will be one WebRtc_Word16 with the current DTMF value and one
 * WebRtc_Word16 with the current volume value
 */
#define DSP_INSTR_NORMAL_ONE_DESC                0x7000
/* No encoded frames */

#define DSP_INSTR_DO_CODEC_INTERNAL_CNG          0x8000
/* Codec has a built-in VAD/DTX scheme (use the above for "no transmission") */

#define DSP_INSTR_PREEMPTIVE_EXPAND              0x9000
/* Payload data will contain the encoded frames, if any */

#define DSP_INSTR_DO_ALTERNATIVE_PLC             0xB000
/* NetEQ switched off and packet missing... */

#define DSP_INSTR_DO_ALTERNATIVE_PLC_INC_TS      0xC000
/* NetEQ switched off and packet missing... */

#define DSP_INSTR_DO_AUDIO_REPETITION            0xD000
/* NetEQ switched off and packet missing... */

#define DSP_INSTR_DO_AUDIO_REPETITION_INC_TS     0xE000
/* NetEQ switched off and packet missing... */

#define DSP_INSTR_FADE_TO_BGN                    0xF000
/* Exception handling: fade out to BGN (expand) */

/*
 * Next 4 bits signal additional data that needs to be transmitted
 */

#define DSP_CODEC_NO_CHANGE                      0x0100
#define DSP_CODEC_NEW_CODEC                      0x0200
#define DSP_CODEC_ADD_LATE_PKT                   0x0300
#define DSP_CODEC_RESET                          0x0400
#define DSP_DTMF_PAYLOAD                         0x0010

/*
 * The most significant bit of the payload-length
 * is used to flag whether the associated payload
 * is redundant payload. This currently useful only for
 * iSAC, where redundant payloads have to be treated 
 * differently. Every time the length is read it must be
 * masked by DSP_CODEC_MASK_RED_FLAG to ignore the flag.
 * Use DSP_CODEC_RED_FLAG to set or retrieve the flag.
 */
#define DSP_CODEC_MASK_RED_FLAG                  0x7FFF
#define DSP_CODEC_RED_FLAG                       0x8000

/*
 * The first block of payload data consist of decode function pointers,
 * and then the speech blocks.
 *
 */


/*
 * The playout modes that NetEq produced (i.e. gives more info about if the 
 * Accelerate was successful or not)
 */

#define MODE_NORMAL                    0x0000
#define MODE_EXPAND                    0x0001
#define MODE_MERGE                     0x0002
#define MODE_SUCCESS_ACCELERATE        0x0003
#define MODE_UNSUCCESS_ACCELERATE      0x0004
#define MODE_RFC3389CNG                0x0005
#define MODE_LOWEN_ACCELERATE          0x0006
#define MODE_DTMF                      0x0007
#define MODE_ONE_DESCRIPTOR            0x0008
#define MODE_CODEC_INTERNAL_CNG        0x0009
#define MODE_SUCCESS_PREEMPTIVE        0x000A
#define MODE_UNSUCCESS_PREEMPTIVE      0x000B
#define MODE_LOWEN_PREEMPTIVE          0x000C
#define MODE_FADE_TO_BGN               0x000D

#define MODE_ERROR                     0x0010

#define MODE_AWAITING_CODEC_PTR        0x0100

#define MODE_BGN_ONLY                  0x0200

#define MODE_MASTER_DTMF_SIGNAL        0x0400

#define MODE_USING_STEREO              0x0800



/***********************/
/* Group codec defines */
/***********************/

#if (defined(NETEQ_ALL_SPECIAL_CODECS))
    #define NETEQ_CNG_CODEC
    #define NETEQ_ATEVENT_DECODE
    #define NETEQ_RED_CODEC
    #define NETEQ_VAD
    #define NETEQ_ARBITRARY_CODEC
#endif

#if (defined(NETEQ_ALL_NB_CODECS))        /* Except RED, DTMF and CNG */
    #define NETEQ_PCM16B_CODEC
    #define NETEQ_G711_CODEC
    #define NETEQ_ILBC_CODEC
    #define NETEQ_G729_CODEC
    #define NETEQ_G726_CODEC
    #define NETEQ_GSMFR_CODEC
    #define NETEQ_AMR_CODEC
#endif

#if (defined(NETEQ_ALL_WB_CODECS))        /* Except RED, DTMF and CNG */
    #define NETEQ_ISAC_CODEC
    #define NETEQ_G722_CODEC
    #define NETEQ_G722_1_CODEC
    #define NETEQ_G729_1_CODEC
    #define NETEQ_SPEEX_CODEC
    #define NETEQ_AMRWB_CODEC
    #define NETEQ_WIDEBAND
#endif

#if (defined(NETEQ_ALL_WB32_CODECS))        /* AAC, RED, DTMF and CNG */
    #define NETEQ_ISAC_SWB_CODEC
    #define NETEQ_32KHZ_WIDEBAND
    #define NETEQ_G722_1C_CODEC
    #define NETEQ_CELT_CODEC
#endif

#if (defined(NETEQ_VOICEENGINE_CODECS))
    /* Special codecs */
    #define NETEQ_CNG_CODEC
    #define NETEQ_ATEVENT_DECODE
    #define NETEQ_RED_CODEC
    #define NETEQ_VAD
    #define NETEQ_ARBITRARY_CODEC

    /* Narrowband codecs */
    #define NETEQ_PCM16B_CODEC
    #define NETEQ_G711_CODEC
    #define NETEQ_ILBC_CODEC
    #define NETEQ_AMR_CODEC
    #define NETEQ_G729_CODEC
    #define NETEQ_GSMFR_CODEC

    /* Wideband codecs */
    #define NETEQ_WIDEBAND
    #define NETEQ_ISAC_CODEC
    #define NETEQ_G722_CODEC
    #define NETEQ_G722_1_CODEC
    #define NETEQ_G729_1_CODEC
    #define NETEQ_AMRWB_CODEC
    #define NETEQ_SPEEX_CODEC

    /* Super wideband 32kHz codecs */
    #define NETEQ_ISAC_SWB_CODEC
    #define NETEQ_32KHZ_WIDEBAND
    #define NETEQ_G722_1C_CODEC
    #define NETEQ_CELT_CODEC

#endif 

#if (defined(NETEQ_ALL_CODECS))
    /* Special codecs */
    #define NETEQ_CNG_CODEC
    #define NETEQ_ATEVENT_DECODE
    #define NETEQ_RED_CODEC
    #define NETEQ_VAD
    #define NETEQ_ARBITRARY_CODEC

    /* Narrowband codecs */
    #define NETEQ_PCM16B_CODEC
    #define NETEQ_G711_CODEC
    #define NETEQ_ILBC_CODEC
    #define NETEQ_G729_CODEC
    #define NETEQ_G726_CODEC
    #define NETEQ_GSMFR_CODEC
    #define NETEQ_AMR_CODEC

    /* Wideband codecs */
    #define NETEQ_WIDEBAND
    #define NETEQ_ISAC_CODEC
    #define NETEQ_G722_CODEC
    #define NETEQ_G722_1_CODEC
    #define NETEQ_G729_1_CODEC
    #define NETEQ_SPEEX_CODEC
    #define NETEQ_AMRWB_CODEC

    /* Super wideband 32kHz codecs */
    #define NETEQ_ISAC_SWB_CODEC
    #define NETEQ_32KHZ_WIDEBAND
    #define NETEQ_G722_1C_CODEC
    #define NETEQ_CELT_CODEC

    /* Super wideband 48kHz codecs */
    #define NETEQ_48KHZ_WIDEBAND
#endif

/* Max output size from decoding one frame */
#if defined(NETEQ_48KHZ_WIDEBAND)
    #define NETEQ_MAX_FRAME_SIZE     2880    /* 60 ms super wideband */
    #define NETEQ_MAX_OUTPUT_SIZE    3600    /* 60+15 ms super wideband (60 ms decoded + 15 ms for merge overlap) */
#elif defined(NETEQ_32KHZ_WIDEBAND)
    #define NETEQ_MAX_FRAME_SIZE     1920    /* 60 ms super wideband */
    #define NETEQ_MAX_OUTPUT_SIZE    2400    /* 60+15 ms super wideband (60 ms decoded + 15 ms for merge overlap) */
#elif defined(NETEQ_WIDEBAND)
    #define NETEQ_MAX_FRAME_SIZE     960        /* 60 ms wideband */
    #define NETEQ_MAX_OUTPUT_SIZE    1200    /* 60+15 ms wideband (60 ms decoded + 10 ms for merge overlap) */
#else
    #define NETEQ_MAX_FRAME_SIZE     480        /* 60 ms narrowband */
    #define NETEQ_MAX_OUTPUT_SIZE    600        /* 60+15 ms narrowband (60 ms decoded + 10 ms for merge overlap) */
#endif


/* Enable stereo */
#define NETEQ_STEREO

#endif /* #if !defined NETEQ_DEFINES_H */

