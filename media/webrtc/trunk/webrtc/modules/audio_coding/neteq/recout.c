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
 * Implementation of RecOut function, which is the main function for the audio output
 * process. This function must be called (through the NetEQ API) once every 10 ms.
 */

#include "dsp.h"

#include <assert.h>
#include <string.h> /* to define NULL */

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"
#include "neteq_defines.h"
#include "mcu_dsp_common.h"

/* Audio types */
#define TYPE_SPEECH 1
#define TYPE_CNG 2

#ifdef NETEQ_DELAY_LOGGING
#include "delay_logging.h"
#include <stdio.h>
#pragma message("*******************************************************************")
#pragma message("You have specified to use NETEQ_DELAY_LOGGING in the NetEQ library.")
#pragma message("Make sure that your test application supports this.")
#pragma message("*******************************************************************")
#endif

/* Scratch usage:

 Type           Name                            size             startpos      endpos
 int16_t  pw16_NetEqAlgorithm_buffer      1080*fs/8000     0             1080*fs/8000-1
 struct         dspInfo                         6                1080*fs/8000  1085*fs/8000

 func           WebRtcNetEQ_Normal              40+495*fs/8000   0             39+495*fs/8000
 func           WebRtcNetEQ_Merge               40+496*fs/8000   0             39+496*fs/8000
 func           WebRtcNetEQ_Expand              40+370*fs/8000   126*fs/800    39+496*fs/8000
 func           WebRtcNetEQ_Accelerate          210              240*fs/8000   209+240*fs/8000
 func           WebRtcNetEQ_BGNUpdate           69               480*fs/8000   68+480*fs/8000

 Total:  1086*fs/8000
 */

#define SCRATCH_ALGORITHM_BUFFER            0
#define SCRATCH_NETEQ_NORMAL                0
#define SCRATCH_NETEQ_MERGE                 0

#if (defined(NETEQ_48KHZ_WIDEBAND)) 
#define SCRATCH_DSP_INFO                     6480
#define SCRATCH_NETEQ_ACCELERATE            1440
#define SCRATCH_NETEQ_BGN_UPDATE            2880
#define SCRATCH_NETEQ_EXPAND                756
#elif (defined(NETEQ_32KHZ_WIDEBAND)) 
#define SCRATCH_DSP_INFO                     4320
#define SCRATCH_NETEQ_ACCELERATE            960
#define SCRATCH_NETEQ_BGN_UPDATE            1920
#define SCRATCH_NETEQ_EXPAND                504
#elif (defined(NETEQ_WIDEBAND)) 
#define SCRATCH_DSP_INFO                     2160
#define SCRATCH_NETEQ_ACCELERATE            480
#define SCRATCH_NETEQ_BGN_UPDATE            960
#define SCRATCH_NETEQ_EXPAND                252
#else    /* NB */
#define SCRATCH_DSP_INFO                     1080
#define SCRATCH_NETEQ_ACCELERATE            240
#define SCRATCH_NETEQ_BGN_UPDATE            480
#define SCRATCH_NETEQ_EXPAND                126
#endif

#if (defined(NETEQ_48KHZ_WIDEBAND)) 
#define SIZE_SCRATCH_BUFFER                 6516
#elif (defined(NETEQ_32KHZ_WIDEBAND)) 
#define SIZE_SCRATCH_BUFFER                 4344
#elif (defined(NETEQ_WIDEBAND)) 
#define SIZE_SCRATCH_BUFFER                 2172
#else    /* NB */
#define SIZE_SCRATCH_BUFFER                 1086
#endif

#ifdef NETEQ_DELAY_LOGGING
extern FILE *delay_fid2; /* file pointer to delay log file */
extern uint32_t tot_received_packets;
#endif


int WebRtcNetEQ_RecOutInternal(DSPInst_t *inst, int16_t *pw16_outData,
                               int16_t *pw16_len, int16_t BGNonly,
                               int av_sync)
{

    int16_t blockLen, payloadLen, len = 0, pos;
    int16_t w16_tmp1, w16_tmp2, w16_tmp3, DataEnough;
    int16_t *blockPtr;
    int16_t MD = 0;

    int16_t speechType = TYPE_SPEECH;
    uint16_t instr;
    uint16_t uw16_tmp;
#ifdef SCRATCH
    char pw8_ScratchBuffer[((SIZE_SCRATCH_BUFFER + 1) * 2)];
    int16_t *pw16_scratchPtr = (int16_t*) pw8_ScratchBuffer;
    /* pad with 240*fs_mult to match the overflow guard below */
    int16_t pw16_decoded_buffer[NETEQ_MAX_FRAME_SIZE+240*6];
    int16_t *pw16_NetEqAlgorithm_buffer = pw16_scratchPtr
        + SCRATCH_ALGORITHM_BUFFER;
    DSP2MCU_info_t *dspInfo = (DSP2MCU_info_t*) (pw16_scratchPtr + SCRATCH_DSP_INFO);
#else
    /* pad with 240*fs_mult to match the overflow guard below */
    int16_t pw16_decoded_buffer[NETEQ_MAX_FRAME_SIZE+240*6];
    int16_t pw16_NetEqAlgorithm_buffer[NETEQ_MAX_OUTPUT_SIZE+240*6];
    DSP2MCU_info_t dspInfoStruct;
    DSP2MCU_info_t *dspInfo = &dspInfoStruct;
#endif
    int16_t fs_mult;
    int borrowedSamples;
    int oldBorrowedSamples;
    int return_value = 0;
    int16_t lastModeBGNonly = (inst->w16_mode & MODE_BGN_ONLY) != 0; /* check BGN flag */
    void *mainInstBackup = inst->main_inst;

#ifdef NETEQ_DELAY_LOGGING
    int temp_var;
#endif
    int16_t dtmfValue = -1;
    int16_t dtmfVolume = -1;
    int playDtmf = 0;
#ifdef NETEQ_ATEVENT_DECODE
    int dtmfSwitch = 0;
#endif
#ifdef NETEQ_STEREO
    MasterSlaveInfo *msInfo = inst->msInfo;
#endif
    int16_t *sharedMem = pw16_NetEqAlgorithm_buffer; /* Reuse memory SHARED_MEM_SIZE size */
    inst->pw16_readAddress = sharedMem;
    inst->pw16_writeAddress = sharedMem;

    /* Get information about if there is one descriptor left */
    if (inst->codec_ptr_inst.funcGetMDinfo != NULL)
    {
        MD = inst->codec_ptr_inst.funcGetMDinfo(inst->codec_ptr_inst.codec_state);
        if (MD > 0)
            MD = 1;
        else
            MD = 0;
    }

#ifdef NETEQ_STEREO
    if ((msInfo->msMode == NETEQ_SLAVE) && (inst->codec_ptr_inst.funcDecode != NULL))
    {
        /*
         * Valid function pointers indicate that we have decoded something,
         * and that the timestamp information is correct.
         */

        /* Get the information from master to correct synchronization */
        uint32_t currentMasterTimestamp;
        uint32_t currentSlaveTimestamp;

        currentMasterTimestamp = msInfo->endTimestamp - msInfo->samplesLeftWithOverlap;
        currentSlaveTimestamp = inst->endTimestamp - (inst->endPosition - inst->curPosition);

        /* Partition the uint32_t space in three: [0 0.25) [0.25 0.75] (0.75 1]
         * We consider a wrap to have occurred if the timestamps are in
         * different edge partitions.
         */
        if (currentSlaveTimestamp < 0x40000000 &&
            currentMasterTimestamp > 0xc0000000) {
          // Slave has wrapped.
          currentSlaveTimestamp += (0xffffffff - currentMasterTimestamp) + 1;
          currentMasterTimestamp = 0;
        } else if (currentMasterTimestamp < 0x40000000 &&
            currentSlaveTimestamp > 0xc0000000) {
          // Master has wrapped.
          currentMasterTimestamp += (0xffffffff - currentSlaveTimestamp) + 1;
          currentSlaveTimestamp = 0;
        }

        if (currentSlaveTimestamp < currentMasterTimestamp)
        {
            /* brute-force discard a number of samples to catch up */
            inst->curPosition += currentMasterTimestamp - currentSlaveTimestamp;

        }
        else if (currentSlaveTimestamp > currentMasterTimestamp)
        {
            /* back off current position to slow down */
            inst->curPosition -= currentSlaveTimestamp - currentMasterTimestamp;
        }

        /* make sure we have at least "overlap" samples left */
        inst->curPosition = WEBRTC_SPL_MIN(inst->curPosition,
            inst->endPosition - inst->ExpandInst.w16_overlap);

        /* make sure we do not end up outside the speech history */
        inst->curPosition = WEBRTC_SPL_MAX(inst->curPosition, 0);
    }
#endif

    /* Write status data to shared memory */
    dspInfo->playedOutTS = inst->endTimestamp;
    dspInfo->samplesLeft = inst->endPosition - inst->curPosition
        - inst->ExpandInst.w16_overlap;
    dspInfo->MD = MD;
    dspInfo->lastMode = inst->w16_mode;
    dspInfo->frameLen = inst->w16_frameLen;

    /* Force update of codec if codec function is NULL */
    if (inst->codec_ptr_inst.funcDecode == NULL)
    {
        dspInfo->lastMode |= MODE_AWAITING_CODEC_PTR;
    }

#ifdef NETEQ_STEREO
    if (msInfo->msMode == NETEQ_SLAVE && (msInfo->extraInfo == DTMF_OVERDUB
        || msInfo->extraInfo == DTMF_ONLY))
    {
        /* Signal that the master instance generated DTMF tones */
        dspInfo->lastMode |= MODE_MASTER_DTMF_SIGNAL;
    }

    if (msInfo->msMode != NETEQ_MONO)
    {
        /* We are using stereo mode; signal this to MCU side */
        dspInfo->lastMode |= MODE_USING_STEREO;
    }
#endif

    WEBRTC_SPL_MEMCPY_W8(inst->pw16_writeAddress,dspInfo,sizeof(DSP2MCU_info_t));

    /* Signal MCU with "interrupt" call to main inst*/
#ifdef NETEQ_STEREO
    assert(msInfo != NULL);
    if (msInfo->msMode == NETEQ_MASTER)
    {
        /* clear info to slave */
        WebRtcSpl_MemSetW16((int16_t *) msInfo, 0,
            sizeof(MasterSlaveInfo) / sizeof(int16_t));
        /* re-set mode */
        msInfo->msMode = NETEQ_MASTER;

        /* Store some information to slave */
        msInfo->endTimestamp = inst->endTimestamp;
        msInfo->samplesLeftWithOverlap = inst->endPosition - inst->curPosition;
    }
#endif

    /*
     * This call will trigger the MCU side to make a decision based on buffer contents and
     * decision history. Instructions, encoded data and function pointers will be written
     * to the shared memory.
     */
    return_value = WebRtcNetEQ_DSP2MCUinterrupt((MainInst_t *) inst->main_inst, sharedMem);

    /* Read MCU data and instructions */
    instr = (uint16_t) (inst->pw16_readAddress[0] & 0xf000);

#ifdef NETEQ_STEREO
    if (msInfo->msMode == NETEQ_MASTER)
    {
        msInfo->instruction = instr;
    }
    else if (msInfo->msMode == NETEQ_SLAVE)
    {
        /* Nothing to do */
    }
#endif

    /* check for error returned from MCU side, if so, return error */
    if (return_value < 0)
    {
        inst->w16_mode = MODE_ERROR;
        dspInfo->lastMode = MODE_ERROR;
        return return_value;
    }

    blockPtr = &((inst->pw16_readAddress)[3]);

    /* Check for DTMF payload flag */
    if ((inst->pw16_readAddress[0] & DSP_DTMF_PAYLOAD) != 0)
    {
        playDtmf = 1;
        dtmfValue = blockPtr[1];
        dtmfVolume = blockPtr[2];
        blockPtr += 3;

#ifdef NETEQ_STEREO
        if (msInfo->msMode == NETEQ_MASTER)
        {
            /* signal to slave that master is using DTMF */
            msInfo->extraInfo = DTMF_OVERDUB;
        }
#endif
    }

    blockLen = (((*blockPtr) & DSP_CODEC_MASK_RED_FLAG) + 1) >> 1; /* In # of int16_t */
    payloadLen = ((*blockPtr) & DSP_CODEC_MASK_RED_FLAG);
    blockPtr++;

    /* Do we have to change our decoder? */
    if ((inst->pw16_readAddress[0] & 0x0f00) == DSP_CODEC_NEW_CODEC)
    {
        WEBRTC_SPL_MEMCPY_W16(&inst->codec_ptr_inst,blockPtr,(payloadLen+1)>>1);
        if (inst->codec_ptr_inst.codec_fs != 0)
        {
            return_value = WebRtcNetEQ_DSPInit(inst, inst->codec_ptr_inst.codec_fs);
            if (return_value != 0)
            { /* error returned */
                instr = DSP_INSTR_FADE_TO_BGN; /* emergency instruction */
            }
#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_CHANGE_FS;
            if ((fwrite(&temp_var, sizeof(int),
                        1, delay_fid2) != 1) ||
                (fwrite(&inst->fs, sizeof(uint16_t),
                        1, delay_fid2) != 1)) {
              return -1;
            }
#endif
        }

        /* Copy it again since the init destroys this part */

        WEBRTC_SPL_MEMCPY_W16(&inst->codec_ptr_inst,blockPtr,(payloadLen+1)>>1);
        inst->endTimestamp = inst->codec_ptr_inst.timeStamp;
        inst->videoSyncTimestamp = inst->codec_ptr_inst.timeStamp;
        blockPtr += blockLen;
        blockLen = (((*blockPtr) & DSP_CODEC_MASK_RED_FLAG) + 1) >> 1;
        payloadLen = ((*blockPtr) & DSP_CODEC_MASK_RED_FLAG);
        blockPtr++;
        if (inst->codec_ptr_inst.funcDecodeInit != NULL)
        {
            inst->codec_ptr_inst.funcDecodeInit(inst->codec_ptr_inst.codec_state);
        }

#ifdef NETEQ_CNG_CODEC

        /* Also update the CNG state as this might be uninitialized */

        WEBRTC_SPL_MEMCPY_W16(&inst->CNG_Codec_inst,blockPtr,(payloadLen+1)>>1);
        blockPtr += blockLen;
        blockLen = (((*blockPtr) & DSP_CODEC_MASK_RED_FLAG) + 1) >> 1;
        payloadLen = ((*blockPtr) & DSP_CODEC_MASK_RED_FLAG);
        blockPtr++;
        if (inst->CNG_Codec_inst != NULL)
        {
            WebRtcCng_InitDec(inst->CNG_Codec_inst);
        }
#endif
    }
    else if ((inst->pw16_readAddress[0] & 0x0f00) == DSP_CODEC_RESET)
    {
        /* Reset the current codec (but not DSP struct) */
        if (inst->codec_ptr_inst.funcDecodeInit != NULL)
        {
            inst->codec_ptr_inst.funcDecodeInit(inst->codec_ptr_inst.codec_state);
        }

#ifdef NETEQ_CNG_CODEC
        /* And reset CNG */
        if (inst->CNG_Codec_inst != NULL)
        {
            WebRtcCng_InitDec(inst->CNG_Codec_inst);
        }
#endif /*NETEQ_CNG_CODEC*/
    }

    fs_mult = WebRtcNetEQ_CalcFsMult(inst->fs);

    /* Add late packet? */
    if ((inst->pw16_readAddress[0] & 0x0f00) == DSP_CODEC_ADD_LATE_PKT)
    {
        if (inst->codec_ptr_inst.funcAddLatePkt != NULL)
        {
            /* Only do this if the codec has support for Add Late Pkt */
            inst->codec_ptr_inst.funcAddLatePkt(inst->codec_ptr_inst.codec_state, blockPtr,
                payloadLen);
        }
        blockPtr += blockLen;
        blockLen = (((*blockPtr) & DSP_CODEC_MASK_RED_FLAG) + 1) >> 1; /* In # of Word16 */
        payloadLen = ((*blockPtr) & DSP_CODEC_MASK_RED_FLAG);
        blockPtr++;
    }

    /* Do we have to decode data? */
    if ((instr == DSP_INSTR_NORMAL) || (instr == DSP_INSTR_ACCELERATE) || (instr
        == DSP_INSTR_MERGE) || (instr == DSP_INSTR_PREEMPTIVE_EXPAND))
    {
        /* Do we need to update codec-internal PLC state? */
        if ((instr == DSP_INSTR_MERGE) && (inst->codec_ptr_inst.funcDecodePLC != NULL))
        {
            len = 0;
            len = inst->codec_ptr_inst.funcDecodePLC(inst->codec_ptr_inst.codec_state,
                &pw16_decoded_buffer[len], 1);
        }
        len = 0;

        /* Do decoding */
        while ((blockLen > 0) && (len < (240 * fs_mult))) /* Guard somewhat against overflow */
        {
            if (inst->codec_ptr_inst.funcDecode != NULL)
            {
                int16_t dec_Len;
                if (!BGNonly)
                {
                  /* Check if this is a sync payload. */
                  if (av_sync && WebRtcNetEQ_IsSyncPayload(blockPtr,
                                                           payloadLen)) {
                    /* Zero-stuffing with same size as the last frame. */
                    dec_Len = inst->w16_frameLen;
                    memset(&pw16_decoded_buffer[len], 0, dec_Len *
                           sizeof(pw16_decoded_buffer[len]));
                  } else {
                    /* Do decoding as normal
                     *
                     * blockPtr is pointing to payload, at this point,
                     * the most significant bit of *(blockPtr - 1) is a flag if
                     * set to 1 indicates that the following payload is the
                     * redundant payload.
                     */
                    if (((*(blockPtr - 1) & DSP_CODEC_RED_FLAG) != 0)
                        && (inst->codec_ptr_inst.funcDecodeRCU != NULL))
                    {
                      dec_Len = inst->codec_ptr_inst.funcDecodeRCU(
                          inst->codec_ptr_inst.codec_state, blockPtr,
                          payloadLen, &pw16_decoded_buffer[len], &speechType);
                    }
                    else
                    {
                      /* Regular decoding. */
                      dec_Len = inst->codec_ptr_inst.funcDecode(
                          inst->codec_ptr_inst.codec_state, blockPtr,
                          payloadLen, &pw16_decoded_buffer[len], &speechType);
                    }
                  }
                }
                else
                {
                    /*
                     * Background noise mode: don't decode, just produce the same length BGN.
                     * Don't call Expand for BGN here, since Expand uses the memory where the
                     * bitstreams are stored (sharemem).
                     */
                    dec_Len = inst->w16_frameLen;
                }

                if (dec_Len > 0)
                {
                    len += dec_Len;
                    /* Update frameLen */
                    inst->w16_frameLen = dec_Len;
                }
                else if (dec_Len < 0)
                {
                    /* Error */
                    len = -1;
                    break;
                }
                /*
                 * Sanity check (although we might still write outside memory when this
                 * happens...)
                 */
                if (len > NETEQ_MAX_FRAME_SIZE)
                {
                    WebRtcSpl_MemSetW16(pw16_outData, 0, inst->timestampsPerCall);
                    *pw16_len = inst->timestampsPerCall;
                    inst->w16_mode = MODE_ERROR;
                    dspInfo->lastMode = MODE_ERROR;
                    return RECOUT_ERROR_DECODED_TOO_MUCH;
                }

                /* Verify that instance was not corrupted by decoder */
                if (mainInstBackup != inst->main_inst)
                {
                    /* Instance is corrupt */
                    return CORRUPT_INSTANCE;
                }

            }
            blockPtr += blockLen;
            blockLen = (((*blockPtr) & DSP_CODEC_MASK_RED_FLAG) + 1) >> 1; /* In # of Word16 */
            payloadLen = ((*blockPtr) & DSP_CODEC_MASK_RED_FLAG);
            blockPtr++;
        }

        if (len < 0)
        {
            len = 0;
            inst->endTimestamp += inst->w16_frameLen; /* advance one frame */
            if (inst->codec_ptr_inst.funcGetErrorCode != NULL)
            {
                return_value = -inst->codec_ptr_inst.funcGetErrorCode(
                    inst->codec_ptr_inst.codec_state);
            }
            else
            {
                return_value = RECOUT_ERROR_DECODING;
            }
            instr = DSP_INSTR_FADE_TO_BGN;
        }
        if (speechType != TYPE_CNG)
        {
            /*
             * Don't increment timestamp if codec returned CNG speech type
             * since in this case, the MCU side will increment the CNGplayedTS counter.
             */
            inst->endTimestamp += len;
        }
    }
    else if (instr == DSP_INSTR_NORMAL_ONE_DESC)
    {
        if (inst->codec_ptr_inst.funcDecode != NULL)
        {
            len = inst->codec_ptr_inst.funcDecode(inst->codec_ptr_inst.codec_state, NULL, 0,
                pw16_decoded_buffer, &speechType);
#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_DECODE_ONE_DESC;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
            if (fwrite(&inst->endTimestamp, sizeof(uint32_t),
                       1, delay_fid2) != 1) {
              return -1;
            }
            if (fwrite(&dspInfo->samplesLeft, sizeof(uint16_t),
                       1, delay_fid2) != 1) {
              return -1;
            }
            tot_received_packets++;
#endif
        }
        if (speechType != TYPE_CNG)
        {
            /*
             * Don't increment timestamp if codec returned CNG speech type
             * since in this case, the MCU side will increment the CNGplayedTS counter.
             */
            inst->endTimestamp += len;
        }

        /* Verify that instance was not corrupted by decoder */
        if (mainInstBackup != inst->main_inst)
        {
            /* Instance is corrupt */
            return CORRUPT_INSTANCE;
        }

        if (len <= 0)
        {
            len = 0;
            if (inst->codec_ptr_inst.funcGetErrorCode != NULL)
            {
                return_value = -inst->codec_ptr_inst.funcGetErrorCode(
                    inst->codec_ptr_inst.codec_state);
            }
            else
            {
                return_value = RECOUT_ERROR_DECODING;
            }
            if ((inst->codec_ptr_inst.funcDecodeInit != NULL)
                && (inst->codec_ptr_inst.codec_state != NULL))
            {
                /* Reinitialize codec state as something is obviously wrong */
                inst->codec_ptr_inst.funcDecodeInit(inst->codec_ptr_inst.codec_state);
            }
            inst->endTimestamp += inst->w16_frameLen; /* advance one frame */
            instr = DSP_INSTR_FADE_TO_BGN;
        }
    }

    if (len == 0 && lastModeBGNonly) /* no new data */
    {
        BGNonly = 1; /* force BGN this time too */
    }

#ifdef NETEQ_VAD
    if ((speechType == TYPE_CNG) /* decoder responded with codec-internal CNG */
    || ((instr == DSP_INSTR_DO_RFC3389CNG) && (blockLen > 0)) /* ... or, SID frame */
    || (inst->fs > 16000)) /* ... or, if not NB or WB */
    {
        /* disable post-decode VAD upon first sign of send-side DTX/VAD active, or if SWB */
        inst->VADInst.VADEnabled = 0;
        inst->VADInst.VADDecision = 1; /* set to always active, just to be on the safe side */
        inst->VADInst.SIDintervalCounter = 0; /* reset SID interval counter */
    }
    else if (!inst->VADInst.VADEnabled) /* VAD disabled and no SID/CNG data observed this time */
    {
        inst->VADInst.SIDintervalCounter++; /* increase counter */
    }

    /* check for re-enabling the VAD */
    if (inst->VADInst.SIDintervalCounter >= POST_DECODE_VAD_AUTO_ENABLE)
    {
        /*
         * It's been a while since the last CNG/SID frame was observed => re-enable VAD.
         * (Do not care to look for a VAD instance, since this is done inside the init
         * function)
         */
        WebRtcNetEQ_InitVAD(&inst->VADInst, inst->fs);
    }

    if (len > 0 /* if we decoded any data */
    && inst->VADInst.VADEnabled /* and VAD enabled */
    && inst->fs <= 16000) /* can only do VAD for NB and WB */
    {
        int VADframeSize; /* VAD frame size in ms */
        int VADSamplePtr = 0;

        inst->VADInst.VADDecision = 0;

        if (inst->VADInst.VADFunction != NULL) /* make sure that VAD function is provided */
        {
            /* divide the data into groups, as large as possible */
            for (VADframeSize = 30; VADframeSize >= 10; VADframeSize -= 10)
            {
                /* loop through 30, 20, 10 */

                while (inst->VADInst.VADDecision == 0
                    && len - VADSamplePtr >= VADframeSize * fs_mult * 8)
                {
                    /*
                     * Only continue until first active speech found, and as long as there is
                     * one VADframeSize left.
                     */

                    /* call VAD with new decoded data */
                    inst->VADInst.VADDecision |= inst->VADInst.VADFunction(
                        inst->VADInst.VADState, (int) inst->fs,
                        (int16_t *) &pw16_decoded_buffer[VADSamplePtr],
                        (VADframeSize * fs_mult * 8));

                    VADSamplePtr += VADframeSize * fs_mult * 8; /* increment sample counter */
                }
            }
        }
        else
        { /* VAD function is NULL */
            inst->VADInst.VADDecision = 1; /* set decision to active */
            inst->VADInst.VADEnabled = 0; /* disable VAD since we have no VAD function */
        }

    }
#endif /* NETEQ_VAD */

    /* Adjust timestamp if needed */
    uw16_tmp = (uint16_t) inst->pw16_readAddress[1];
    inst->endTimestamp += (((uint32_t) uw16_tmp) << 16);
    uw16_tmp = (uint16_t) inst->pw16_readAddress[2];
    inst->endTimestamp += uw16_tmp;

    if (BGNonly && len > 0)
    {
        /*
         * If BGN mode, we did not produce any data at decoding.
         * Do it now instead.
         */

        WebRtcNetEQ_GenerateBGN(inst,
#ifdef SCRATCH
            pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
            pw16_decoded_buffer, len);
    }

    /* Switch on the instruction received from the MCU side. */
    switch (instr)
    {
        case DSP_INSTR_NORMAL:

            /* Allow for signal processing to apply gain-back etc */
            WebRtcNetEQ_Normal(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQ_NORMAL,
#endif
                pw16_decoded_buffer, len, pw16_NetEqAlgorithm_buffer, &len);

            /* If last packet was decoded as a inband CNG set mode to CNG instead */
            if ((speechType == TYPE_CNG) || ((inst->w16_mode == MODE_CODEC_INTERNAL_CNG)
                && (len == 0)))
            {
                inst->w16_mode = MODE_CODEC_INTERNAL_CNG;
            }

#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            break;
        case DSP_INSTR_NORMAL_ONE_DESC:

            /* Allow for signal processing to apply gain-back etc */
            WebRtcNetEQ_Normal(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQ_NORMAL,
#endif
                pw16_decoded_buffer, len, pw16_NetEqAlgorithm_buffer, &len);
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            inst->w16_mode = MODE_ONE_DESCRIPTOR;
            break;
        case DSP_INSTR_MERGE:
#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_MERGE_INFO;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
            temp_var = -len;
#endif
            /* Call Merge with history*/
            return_value = WebRtcNetEQ_Merge(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQ_MERGE,
#endif
                pw16_decoded_buffer, len, pw16_NetEqAlgorithm_buffer, &len);

            if (return_value < 0)
            {
                /* error */
                return return_value;
            }

#ifdef NETEQ_DELAY_LOGGING
            temp_var += len;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
#endif
            /* If last packet was decoded as a inband CNG set mode to CNG instead */
            if (speechType == TYPE_CNG) inst->w16_mode = MODE_CODEC_INTERNAL_CNG;
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            break;

        case DSP_INSTR_EXPAND:
            len = 0;
            pos = 0;
            while ((inst->endPosition - inst->curPosition - inst->ExpandInst.w16_overlap + pos)
                < (inst->timestampsPerCall))
            {
                return_value = WebRtcNetEQ_Expand(inst,
#ifdef SCRATCH
                    pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
                    pw16_NetEqAlgorithm_buffer, &len, BGNonly);
                if (return_value < 0)
                {
                    /* error */
                    return return_value;
                }

                /*
                 * Update buffer, but only end part (otherwise expand state is destroyed
                 * since it reuses speechBuffer[] memory
                 */

                WEBRTC_SPL_MEMMOVE_W16(inst->pw16_speechHistory,
                                       inst->pw16_speechHistory + len,
                                       (inst->w16_speechHistoryLen-len));
                WEBRTC_SPL_MEMCPY_W16(&inst->pw16_speechHistory[inst->w16_speechHistoryLen-len],
                                      pw16_NetEqAlgorithm_buffer, len);

                inst->curPosition -= len;

                /* Update variables for VQmon */
                inst->w16_concealedTS += len;
#ifdef NETEQ_DELAY_LOGGING
                temp_var = NETEQ_DELAY_LOGGING_SIGNAL_EXPAND_INFO;
                if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                  return -1;
                }
                temp_var = len;
                if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                  return -1;
                }
#endif
                len = 0; /* already written the data, so do not write it again further down. */
            }
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            break;

        case DSP_INSTR_ACCELERATE:
            if (len < 3 * 80 * fs_mult)
            {
                /* We need to move data from the speechBuffer[] in order to get 30 ms */
                borrowedSamples = 3 * 80 * fs_mult - len;

                WEBRTC_SPL_MEMMOVE_W16(&pw16_decoded_buffer[borrowedSamples],
                                       pw16_decoded_buffer, len);
                WEBRTC_SPL_MEMCPY_W16(pw16_decoded_buffer,
                                      &(inst->speechBuffer[inst->endPosition-borrowedSamples]),
                                      borrowedSamples);

                return_value = WebRtcNetEQ_Accelerate(inst,
#ifdef SCRATCH
                    pw16_scratchPtr + SCRATCH_NETEQ_ACCELERATE,
#endif
                    pw16_decoded_buffer, 3 * inst->timestampsPerCall,
                    pw16_NetEqAlgorithm_buffer, &len, BGNonly);

                if (return_value < 0)
                {
                    /* error */
                    return return_value;
                }

                /* Copy back samples to the buffer */
                if (len < borrowedSamples)
                {
                    /*
                     * This destroys the beginning of the buffer, but will not cause any
                     * problems
                     */

                    WEBRTC_SPL_MEMCPY_W16(&inst->speechBuffer[inst->endPosition-borrowedSamples],
                        pw16_NetEqAlgorithm_buffer, len);
                    WEBRTC_SPL_MEMMOVE_W16(&inst->speechBuffer[borrowedSamples-len],
                                           inst->speechBuffer,
                                           (inst->endPosition-(borrowedSamples-len)));

                    inst->curPosition += (borrowedSamples - len);
#ifdef NETEQ_DELAY_LOGGING
                    temp_var = NETEQ_DELAY_LOGGING_SIGNAL_ACCELERATE_INFO;
                    if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                      return -1;
                    }
                    temp_var = 3 * inst->timestampsPerCall - len;
                    if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                      return -1;
                    }
#endif
                    len = 0;
                }
                else
                {
                    WEBRTC_SPL_MEMCPY_W16(&inst->speechBuffer[inst->endPosition-borrowedSamples],
                        pw16_NetEqAlgorithm_buffer, borrowedSamples);
                    WEBRTC_SPL_MEMMOVE_W16(pw16_NetEqAlgorithm_buffer,
                                           &pw16_NetEqAlgorithm_buffer[borrowedSamples],
                                           (len-borrowedSamples));
#ifdef NETEQ_DELAY_LOGGING
                    temp_var = NETEQ_DELAY_LOGGING_SIGNAL_ACCELERATE_INFO;
                    if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                      return -1;
                    }
                    temp_var = 3 * inst->timestampsPerCall - len;
                    if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                      return -1;
                    }
#endif
                    len = len - borrowedSamples;
                }

            }
            else
            {
#ifdef NETEQ_DELAY_LOGGING
                temp_var = NETEQ_DELAY_LOGGING_SIGNAL_ACCELERATE_INFO;
                if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                  return -1;
                }
                temp_var = len;
#endif
                return_value = WebRtcNetEQ_Accelerate(inst,
#ifdef SCRATCH
                    pw16_scratchPtr + SCRATCH_NETEQ_ACCELERATE,
#endif
                    pw16_decoded_buffer, len, pw16_NetEqAlgorithm_buffer, &len, BGNonly);

                if (return_value < 0)
                {
                    /* error */
                    return return_value;
                }

#ifdef NETEQ_DELAY_LOGGING
                temp_var -= len;
                if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
                  return -1;
                }
#endif
            }
            /* If last packet was decoded as a inband CNG set mode to CNG instead */
            if (speechType == TYPE_CNG) inst->w16_mode = MODE_CODEC_INTERNAL_CNG;
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            break;

        case DSP_INSTR_DO_RFC3389CNG:
#ifdef NETEQ_CNG_CODEC
            if (blockLen > 0)
            {
                if (WebRtcCng_UpdateSid(inst->CNG_Codec_inst, (uint8_t*) blockPtr,
                    payloadLen) < 0)
                {
                    /* error returned from CNG function */
                    return_value = -WebRtcCng_GetErrorCodeDec(inst->CNG_Codec_inst);
                    len = inst->timestampsPerCall;
                    WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0, len);
                    break;
                }
            }

            if (BGNonly)
            {
                /* Get data from BGN function instead of CNG */
                len = WebRtcNetEQ_GenerateBGN(inst,
#ifdef SCRATCH
                    pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
                    pw16_NetEqAlgorithm_buffer, inst->timestampsPerCall);
                if (len != inst->timestampsPerCall)
                {
                    /* this is not good, treat this as an error */
                    return_value = -1;
                }
            }
            else
            {
                return_value = WebRtcNetEQ_Cng(inst, pw16_NetEqAlgorithm_buffer,
                    inst->timestampsPerCall);
            }
            len = inst->timestampsPerCall;
            inst->ExpandInst.w16_consecExp = 0;
            inst->w16_mode = MODE_RFC3389CNG;
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif

            if (return_value < 0)
            {
                /* error returned */
                WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0, len);
            }

            break;
#else
            return FAULTY_INSTRUCTION;
#endif
        case DSP_INSTR_DO_CODEC_INTERNAL_CNG:
            /*
             * This represents the case when there is no transmission and the decoder should
             * do internal CNG.
             */
            len = 0;
            if (inst->codec_ptr_inst.funcDecode != NULL && !BGNonly)
            {
                len = inst->codec_ptr_inst.funcDecode(inst->codec_ptr_inst.codec_state,
                    blockPtr, 0, pw16_decoded_buffer, &speechType);
            }
            else
            {
                /* get BGN data */
                len = WebRtcNetEQ_GenerateBGN(inst,
#ifdef SCRATCH
                    pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
                    pw16_decoded_buffer, inst->timestampsPerCall);
            }
            WebRtcNetEQ_Normal(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQ_NORMAL,
#endif
                pw16_decoded_buffer, len, pw16_NetEqAlgorithm_buffer, &len);
            inst->w16_mode = MODE_CODEC_INTERNAL_CNG;
            inst->ExpandInst.w16_consecExp = 0;
            break;

        case DSP_INSTR_DTMF_GENERATE:
#ifdef NETEQ_ATEVENT_DECODE
            dtmfSwitch = 0;
            if ((inst->w16_mode != MODE_DTMF) && (inst->DTMFInst.reinit == 0))
            {
                /* Special case; see below.
                 * We must catch this before calling DTMFGenerate,
                 * since reinit is set to 0 in that call.
                 */
                dtmfSwitch = 1;
            }

            len = WebRtcNetEQ_DTMFGenerate(&inst->DTMFInst, dtmfValue, dtmfVolume,
                pw16_NetEqAlgorithm_buffer, inst->fs, -1);
            if (len < 0)
            {
                /* error occurred */
                return_value = len;
                len = inst->timestampsPerCall;
                WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0, len);
            }

            if (dtmfSwitch == 1)
            {
                /*
                 * This is the special case where the previous operation was DTMF overdub.
                 * but the current instruction is "regular" DTMF. We must make sure that the
                 * DTMF does not have any discontinuities. The first DTMF sample that we
                 * generate now must be played out immediately, wherefore it must be copied to
                 * the speech buffer.
                 */

                /*
                 * Generate extra DTMF data to fill the space between
                 * curPosition and endPosition
                 */
                int16_t tempLen;

                tempLen = WebRtcNetEQ_DTMFGenerate(&inst->DTMFInst, dtmfValue, dtmfVolume,
                    &pw16_NetEqAlgorithm_buffer[len], inst->fs,
                    inst->endPosition - inst->curPosition);
                if (tempLen < 0)
                {
                    /* error occurred */
                    return_value = tempLen;
                    len = inst->endPosition - inst->curPosition;
                    WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0,
                        inst->endPosition - inst->curPosition);
                }

                /* Add to total length */
                len += tempLen;

                /* Overwrite the "future" part of the speech buffer with the new DTMF data */

                WEBRTC_SPL_MEMCPY_W16(&inst->speechBuffer[inst->curPosition],
                                      pw16_NetEqAlgorithm_buffer,
                                      inst->endPosition - inst->curPosition);

                /* Shuffle the remaining data to the beginning of algorithm buffer */
                len -= (inst->endPosition - inst->curPosition);
                WEBRTC_SPL_MEMMOVE_W16(pw16_NetEqAlgorithm_buffer,
                    &pw16_NetEqAlgorithm_buffer[inst->endPosition - inst->curPosition],
                    len);
            }

            inst->endTimestamp += inst->timestampsPerCall;
            inst->DTMFInst.reinit = 0;
            inst->ExpandInst.w16_consecExp = 0;
            inst->w16_mode = MODE_DTMF;
            BGNonly = 0; /* override BGN only and let DTMF through */

            playDtmf = 0; /* set to zero because the DTMF is already in the Algorithm buffer */
            /*
             * If playDtmf is 1, an extra DTMF vector will be generated and overdubbed
             * on the output.
             */

#ifdef NETEQ_STEREO
            if (msInfo->msMode == NETEQ_MASTER)
            {
                /* signal to slave that master is using DTMF only */
                msInfo->extraInfo = DTMF_ONLY;
            }
#endif

            break;
#else
            inst->w16_mode = MODE_ERROR;
            dspInfo->lastMode = MODE_ERROR;
            return FAULTY_INSTRUCTION;
#endif

        case DSP_INSTR_DO_ALTERNATIVE_PLC:
            if (inst->codec_ptr_inst.funcDecodePLC != 0)
            {
                len = inst->codec_ptr_inst.funcDecodePLC(inst->codec_ptr_inst.codec_state,
                    pw16_NetEqAlgorithm_buffer, 1);
            }
            else
            {
                len = inst->timestampsPerCall;
                /* ZeroStuffing... */
                WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0, len);
                /* By not advancing the timestamp, NetEq inserts samples. */
                inst->statInst.addedSamples += len;
            }
            inst->ExpandInst.w16_consecExp = 0;
            break;
        case DSP_INSTR_DO_ALTERNATIVE_PLC_INC_TS:
            if (inst->codec_ptr_inst.funcDecodePLC != 0)
            {
                len = inst->codec_ptr_inst.funcDecodePLC(inst->codec_ptr_inst.codec_state,
                    pw16_NetEqAlgorithm_buffer, 1);
            }
            else
            {
                len = inst->timestampsPerCall;
                /* ZeroStuffing... */
                WebRtcSpl_MemSetW16(pw16_NetEqAlgorithm_buffer, 0, len);
            }
            inst->ExpandInst.w16_consecExp = 0;
            inst->endTimestamp += len;
            break;
        case DSP_INSTR_DO_AUDIO_REPETITION:
            len = inst->timestampsPerCall;
            /* copy->paste... */
            WEBRTC_SPL_MEMCPY_W16(pw16_NetEqAlgorithm_buffer,
                                  &inst->speechBuffer[inst->endPosition-len], len);
            inst->ExpandInst.w16_consecExp = 0;
            break;
        case DSP_INSTR_DO_AUDIO_REPETITION_INC_TS:
            len = inst->timestampsPerCall;
            /* copy->paste... */
            WEBRTC_SPL_MEMCPY_W16(pw16_NetEqAlgorithm_buffer,
                                  &inst->speechBuffer[inst->endPosition-len], len);
            inst->ExpandInst.w16_consecExp = 0;
            inst->endTimestamp += len;
            break;

        case DSP_INSTR_PREEMPTIVE_EXPAND:
            if (len < 3 * inst->timestampsPerCall)
            {
                /* borrow samples from sync buffer if necessary */
                borrowedSamples = 3 * inst->timestampsPerCall - len; /* borrow this many samples */
                /* calculate how many of these are already played out */
                oldBorrowedSamples = WEBRTC_SPL_MAX(0,
                    borrowedSamples - (inst->endPosition - inst->curPosition));
                WEBRTC_SPL_MEMMOVE_W16(&pw16_decoded_buffer[borrowedSamples],
                                       pw16_decoded_buffer, len);
                WEBRTC_SPL_MEMCPY_W16(pw16_decoded_buffer,
                                      &(inst->speechBuffer[inst->endPosition-borrowedSamples]),
                                      borrowedSamples);
            }
            else
            {
                borrowedSamples = 0;
                oldBorrowedSamples = 0;
            }

#ifdef NETEQ_DELAY_LOGGING
            w16_tmp1 = len;
#endif
            /* do the expand */
            return_value = WebRtcNetEQ_PreEmptiveExpand(inst,
#ifdef SCRATCH
                /* use same scratch memory as Accelerate */
                pw16_scratchPtr + SCRATCH_NETEQ_ACCELERATE,
#endif
                pw16_decoded_buffer, len + borrowedSamples, oldBorrowedSamples,
                pw16_NetEqAlgorithm_buffer, &len, BGNonly);

            if (return_value < 0)
            {
                /* error */
                return return_value;
            }

            if (borrowedSamples > 0)
            {
                /* return borrowed samples */

                /* Copy back to last part of speechBuffer from beginning of output buffer */
                WEBRTC_SPL_MEMCPY_W16( &(inst->speechBuffer[inst->endPosition-borrowedSamples]),
                    pw16_NetEqAlgorithm_buffer,
                    borrowedSamples);

                len -= borrowedSamples; /* remove the borrowed samples from new total length */

                /* Move to beginning of output buffer from end of output buffer */
                WEBRTC_SPL_MEMMOVE_W16( pw16_NetEqAlgorithm_buffer,
                    &pw16_NetEqAlgorithm_buffer[borrowedSamples],
                    len);
            }

#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_PREEMPTIVE_INFO;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
            temp_var = len - w16_tmp1; /* number of samples added */
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
#endif
            /* If last packet was decoded as inband CNG, set mode to CNG instead */
            if (speechType == TYPE_CNG) inst->w16_mode = MODE_CODEC_INTERNAL_CNG;
#ifdef NETEQ_ATEVENT_DECODE
            if (playDtmf == 0)
            {
                inst->DTMFInst.reinit = 1;
            }
#endif
            break;

        case DSP_INSTR_FADE_TO_BGN:
        {
            int tempReturnValue;
            /* do not overwrite return_value, since it likely contains an error code */

            /* calculate interpolation length */
            w16_tmp3 = WEBRTC_SPL_MIN(inst->endPosition - inst->curPosition,
                    inst->timestampsPerCall);
            /* check that it will fit in pw16_NetEqAlgorithm_buffer */
            if (w16_tmp3 + inst->w16_frameLen > NETEQ_MAX_OUTPUT_SIZE)
            {
                w16_tmp3 = NETEQ_MAX_OUTPUT_SIZE - inst->w16_frameLen;
            }

            /* call Expand */
            len = inst->timestampsPerCall + inst->ExpandInst.w16_overlap;
            pos = 0;

            tempReturnValue = WebRtcNetEQ_Expand(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
                pw16_NetEqAlgorithm_buffer, &len, 1);

            if (tempReturnValue < 0)
            {
                /* error */
                /* this error value will override return_value */
                return tempReturnValue;
            }

            pos += len; /* got len samples from expand */

            /* copy to fill the demand */
            while (pos + len <= inst->w16_frameLen + w16_tmp3)
            {
                WEBRTC_SPL_MEMCPY_W16(&pw16_NetEqAlgorithm_buffer[pos],
                    pw16_NetEqAlgorithm_buffer, len);
                pos += len;
            }

            /* fill with fraction of the expand vector if needed */
            if (pos < inst->w16_frameLen + w16_tmp3)
            {
                WEBRTC_SPL_MEMCPY_W16(&pw16_NetEqAlgorithm_buffer[pos], pw16_NetEqAlgorithm_buffer,
                    inst->w16_frameLen + w16_tmp3 - pos);
            }

            len = inst->w16_frameLen + w16_tmp3; /* truncate any surplus samples since we don't want these */

            /*
             * Mix with contents in sync buffer. Find largest power of two that is less than
             * interpolate length divide 16384 with this number; result is in w16_tmp2.
             */
            w16_tmp1 = 2;
            w16_tmp2 = 16384;
            while (w16_tmp1 <= w16_tmp3)
            {
                w16_tmp2 >>= 1; /* divide with 2 */
                w16_tmp1 <<= 1; /* increase with a factor of 2 */
            }

            w16_tmp1 = 0;
            pos = 0;
            while (w16_tmp1 < 16384)
            {
                inst->speechBuffer[inst->curPosition + pos]
                    =
                    (int16_t) WEBRTC_SPL_RSHIFT_W32(
                        WEBRTC_SPL_MUL_16_16( inst->speechBuffer[inst->endPosition - w16_tmp3 + pos],
                            16384-w16_tmp1 ) +
                        WEBRTC_SPL_MUL_16_16( pw16_NetEqAlgorithm_buffer[pos], w16_tmp1 ),
                        14 );
                w16_tmp1 += w16_tmp2;
                pos++;
            }

            /* overwrite remainder of speech buffer */

            WEBRTC_SPL_MEMCPY_W16( &inst->speechBuffer[inst->endPosition - w16_tmp3 + pos],
                &pw16_NetEqAlgorithm_buffer[pos], w16_tmp3 - pos);

            len -= w16_tmp3;
            /* shift algorithm buffer */

            WEBRTC_SPL_MEMMOVE_W16( pw16_NetEqAlgorithm_buffer,
                &pw16_NetEqAlgorithm_buffer[w16_tmp3],
                len );

            /* Update variables for VQmon */
            inst->w16_concealedTS += len;

            inst->w16_mode = MODE_FADE_TO_BGN;
#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_EXPAND_INFO;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
            temp_var = len;
            if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
              return -1;
            }
#endif

            break;
        }

        default:
            inst->w16_mode = MODE_ERROR;
            dspInfo->lastMode = MODE_ERROR;
            return FAULTY_INSTRUCTION;
    } /* end of grand switch */

    /* Copy data directly to output buffer */

    w16_tmp2 = 0;
    if ((inst->endPosition + len - inst->curPosition - inst->ExpandInst.w16_overlap)
        >= inst->timestampsPerCall)
    {
        w16_tmp2 = inst->endPosition - inst->curPosition;
        w16_tmp2 = WEBRTC_SPL_MAX(w16_tmp2, 0); /* Additional error protection, just in case */
        w16_tmp1 = WEBRTC_SPL_MIN(w16_tmp2, inst->timestampsPerCall);
        w16_tmp2 = inst->timestampsPerCall - w16_tmp1;
        WEBRTC_SPL_MEMCPY_W16(pw16_outData, &inst->speechBuffer[inst->curPosition], w16_tmp1);
        WEBRTC_SPL_MEMCPY_W16(&pw16_outData[w16_tmp1], pw16_NetEqAlgorithm_buffer, w16_tmp2);
        DataEnough = 1;
    }
    else
    {
        DataEnough = 0;
    }

    if (playDtmf != 0)
    {
#ifdef NETEQ_ATEVENT_DECODE
        int16_t outDataIndex = 0;
        int16_t overdubLen = -1; /* default len */
        int16_t dtmfLen;

        /*
         * Overdub the output with DTMF. Note that this is not executed if the
         * DSP_INSTR_DTMF_GENERATE operation is performed above.
         */
        if (inst->DTMFInst.lastDtmfSample - inst->curPosition > 0)
        {
            /* special operation for transition from "DTMF only" to "DTMF overdub" */
            outDataIndex
                = WEBRTC_SPL_MIN(inst->DTMFInst.lastDtmfSample - inst->curPosition,
                    inst->timestampsPerCall);
            overdubLen = inst->timestampsPerCall - outDataIndex;
        }

        dtmfLen = WebRtcNetEQ_DTMFGenerate(&inst->DTMFInst, dtmfValue, dtmfVolume,
            &pw16_outData[outDataIndex], inst->fs, overdubLen);
        if (dtmfLen < 0)
        {
            /* error occurred */
            return_value = dtmfLen;
        }
        inst->DTMFInst.reinit = 0;
#else
        inst->w16_mode = MODE_ERROR;
        dspInfo->lastMode = MODE_ERROR;
        return FAULTY_INSTRUCTION;
#endif
    }

    /*
     * Shuffle speech buffer to allow more data. Move data from pw16_NetEqAlgorithm_buffer
     * to speechBuffer.
     */
    if (instr != DSP_INSTR_EXPAND)
    {
        w16_tmp1 = WEBRTC_SPL_MIN(inst->endPosition, len);
        WEBRTC_SPL_MEMMOVE_W16(inst->speechBuffer, inst->speechBuffer + w16_tmp1,
                               (inst->endPosition-w16_tmp1));
        WEBRTC_SPL_MEMCPY_W16(&inst->speechBuffer[inst->endPosition-w16_tmp1],
                              &pw16_NetEqAlgorithm_buffer[len-w16_tmp1], w16_tmp1);
#ifdef NETEQ_ATEVENT_DECODE
        /* Update index to end of DTMF data in speech buffer */
        if (instr == DSP_INSTR_DTMF_GENERATE)
        {
            /* We have written DTMF data to the end of speech buffer */
            inst->DTMFInst.lastDtmfSample = inst->endPosition;
        }
        else if (inst->DTMFInst.lastDtmfSample > 0)
        {
            /* The end of DTMF data in speech buffer has been shuffled */
            inst->DTMFInst.lastDtmfSample -= w16_tmp1;
        }
#endif
        /*
         * Update the BGN history if last operation was not expand (nor Merge, Accelerate
         * or Pre-emptive expand, to save complexity).
         */
        if ((inst->w16_mode != MODE_EXPAND) && (inst->w16_mode != MODE_MERGE)
            && (inst->w16_mode != MODE_SUCCESS_ACCELERATE) && (inst->w16_mode
            != MODE_LOWEN_ACCELERATE) && (inst->w16_mode != MODE_SUCCESS_PREEMPTIVE)
            && (inst->w16_mode != MODE_LOWEN_PREEMPTIVE) && (inst->w16_mode
            != MODE_FADE_TO_BGN) && (inst->w16_mode != MODE_DTMF) && (!BGNonly))
        {
            WebRtcNetEQ_BGNUpdate(inst
#ifdef SCRATCH
                , pw16_scratchPtr + SCRATCH_NETEQ_BGN_UPDATE
#endif
            );
        }
    }
    else /* instr == DSP_INSTR_EXPAND */
    {
        /* Nothing should be done since data is already copied to output. */
    }

    inst->curPosition -= len;

    /*
     * Extra protection in case something should go totally wrong in terms of sizes...
     * If everything is ok this should NEVER happen.
     */
    if (inst->curPosition < -inst->timestampsPerCall)
    {
        inst->curPosition = -inst->timestampsPerCall;
    }

    if ((instr != DSP_INSTR_EXPAND) && (instr != DSP_INSTR_MERGE) && (instr
        != DSP_INSTR_FADE_TO_BGN))
    {
        /* Reset concealed TS parameter if it does not seem to have been flushed */
        if (inst->w16_concealedTS > inst->timestampsPerCall)
        {
            inst->w16_concealedTS = 0;
        }
    }

    /*
     * Double-check that we actually have 10 ms to play. If we haven't, there has been a
     * serious error.The decoder might have returned way too few samples
     */
    if (!DataEnough)
    {
        /* This should not happen. Set outdata to zeros, and return error. */
        WebRtcSpl_MemSetW16(pw16_outData, 0, inst->timestampsPerCall);
        *pw16_len = inst->timestampsPerCall;
        inst->w16_mode = MODE_ERROR;
        dspInfo->lastMode = MODE_ERROR;
        return RECOUT_ERROR_SAMPLEUNDERRUN;
    }

    /*
     * Update Videosync timestamp (this special timestamp is needed since the endTimestamp
     * stops during CNG and Expand periods.
     */
    if ((inst->w16_mode != MODE_EXPAND) && (inst->w16_mode != MODE_RFC3389CNG))
    {
        uint32_t uw32_tmpTS;
        uw32_tmpTS = inst->endTimestamp - (inst->endPosition - inst->curPosition);
        if ((int32_t) (uw32_tmpTS - inst->videoSyncTimestamp) > 0)
        {
            inst->videoSyncTimestamp = uw32_tmpTS;
        }
    }
    else
    {
        inst->videoSyncTimestamp += inst->timestampsPerCall;
    }

    /* After this, regardless of what has happened, deliver 10 ms of future data */
    inst->curPosition += inst->timestampsPerCall;
    *pw16_len = inst->timestampsPerCall;

    /* Remember if BGNonly was used */
    if (BGNonly)
    {
        inst->w16_mode |= MODE_BGN_ONLY;
    }

    return return_value;
}

#undef    SCRATCH_ALGORITHM_BUFFER
#undef    SCRATCH_NETEQ_NORMAL
#undef    SCRATCH_NETEQ_MERGE
#undef    SCRATCH_NETEQ_BGN_UPDATE
#undef    SCRATCH_NETEQ_EXPAND
#undef    SCRATCH_DSP_INFO
#undef    SCRATCH_NETEQ_ACCELERATE
#undef    SIZE_SCRATCH_BUFFER
