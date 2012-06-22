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
 * Signal the MCU that data is available and ask for a RecOut decision.
 */

#include "mcu.h"

#include <string.h>

#include "signal_processing_library.h"

#include "automode.h"
#include "dtmf_buffer.h"
#include "mcu_dsp_common.h"
#include "neteq_error_codes.h"

#ifdef NETEQ_DELAY_LOGGING
#include "delay_logging.h"
#include <stdio.h>

extern FILE *delay_fid2; /* file pointer to delay log file */
#endif


/*
 * Signals the MCU that DSP status data is available.
 */
int WebRtcNetEQ_SignalMcu(MCUInst_t *inst)
{

    int i_bufferpos, i_res;
    WebRtc_UWord16 uw16_instr;
    DSP2MCU_info_t dspInfo;
    WebRtc_Word16 *blockPtr, blockLen;
    WebRtc_UWord32 uw32_availableTS;
    RTPPacket_t temp_pkt;
    WebRtc_Word32 w32_bufsize, w32_tmp;
    WebRtc_Word16 payloadType = -1;
    WebRtc_Word16 wantedNoOfTimeStamps;
    WebRtc_Word32 totalTS;
    WebRtc_Word16 oldPT, latePacketExist = 0;
    WebRtc_UWord32 oldTS, prevTS, uw32_tmp;
    WebRtc_UWord16 prevSeqNo;
    WebRtc_Word16 nextSeqNoAvail;
    WebRtc_Word16 fs_mult, w16_tmp;
    WebRtc_Word16 lastModeBGNonly = 0;
#ifdef NETEQ_DELAY_LOGGING
    int temp_var;
#endif
    int playDtmf = 0;

    fs_mult = WebRtcSpl_DivW32W16ResW16(inst->fs, 8000);

    /* Increment counter since last statistics report */
    inst->lastReportTS += inst->timestampsPerCall;

    /* Increment waiting time for all packets. */
    WebRtcNetEQ_IncrementWaitingTimes(&inst->PacketBuffer_inst);

    /* Read info from DSP so we now current status */

    WEBRTC_SPL_MEMCPY_W8(&dspInfo,inst->pw16_readAddress,sizeof(DSP2MCU_info_t));

    /* Set blockPtr to first payload block */
    blockPtr = &inst->pw16_writeAddress[3];

    /* Clear instruction word and number of lost samples (2*WebRtc_Word16) */
    inst->pw16_writeAddress[0] = 0;
    inst->pw16_writeAddress[1] = 0;
    inst->pw16_writeAddress[2] = 0;

    if ((dspInfo.lastMode & MODE_AWAITING_CODEC_PTR) != 0)
    {
        /*
         * Make sure state is adjusted so that a codec update is
         * performed when first packet arrives.
         */
        if (inst->new_codec != 1)
        {
            inst->current_Codec = -1;
        }
        dspInfo.lastMode = (dspInfo.lastMode ^ MODE_AWAITING_CODEC_PTR);
    }

#ifdef NETEQ_STEREO
    if ((dspInfo.lastMode & MODE_MASTER_DTMF_SIGNAL) != 0)
    {
        playDtmf = 1; /* force DTMF decision */
        dspInfo.lastMode = (dspInfo.lastMode ^ MODE_MASTER_DTMF_SIGNAL);
    }

    if ((dspInfo.lastMode & MODE_USING_STEREO) != 0)
    {
        if (inst->usingStereo == 0)
        {
            /* stereo mode changed; reset automode instance to re-synchronize statistics */
            WebRtcNetEQ_ResetAutomode(&(inst->BufferStat_inst.Automode_inst),
                inst->PacketBuffer_inst.maxInsertPositions);
        }
        inst->usingStereo = 1;
        dspInfo.lastMode = (dspInfo.lastMode ^ MODE_USING_STEREO);
    }
    else
    {
        inst->usingStereo = 0;
    }
#endif

    /* detect if BGN_ONLY flag is set in lastMode */
    if ((dspInfo.lastMode & MODE_BGN_ONLY) != 0)
    {
        lastModeBGNonly = 1; /* remember flag */
        dspInfo.lastMode ^= MODE_BGN_ONLY; /* clear the flag */
    }

    if ((dspInfo.lastMode == MODE_RFC3389CNG) || (dspInfo.lastMode == MODE_CODEC_INTERNAL_CNG)
        || (dspInfo.lastMode == MODE_EXPAND))
    {
        /*
         * If last mode was CNG (or Expand, since this could be covering up for a lost CNG
         * packet), increase the CNGplayedTS counter.
         */
        inst->BufferStat_inst.uw32_CNGplayedTS += inst->timestampsPerCall;

        if (dspInfo.lastMode == MODE_RFC3389CNG)
        {
            /* remember that RFC3389CNG is on (needed if CNG is interrupted by DTMF) */
            inst->BufferStat_inst.w16_cngOn = CNG_RFC3389_ON;
        }
        else if (dspInfo.lastMode == MODE_CODEC_INTERNAL_CNG)
        {
            /* remember that internal CNG is on (needed if CNG is interrupted by DTMF) */
            inst->BufferStat_inst.w16_cngOn = CNG_INTERNAL_ON;
        }

    }

    /* Update packet size from previously decoded packet */
    if (dspInfo.frameLen > 0)
    {
        inst->PacketBuffer_inst.packSizeSamples = dspInfo.frameLen;
    }

    /* Look for late packet (unless codec has changed) */
    if (inst->new_codec != 1)
    {
        if (WebRtcNetEQ_DbIsMDCodec((enum WebRtcNetEQDecoder) inst->current_Codec))
        {
            WebRtcNetEQ_PacketBufferFindLowestTimestamp(&inst->PacketBuffer_inst,
                inst->timeStamp, &uw32_availableTS, &i_bufferpos, 1, &payloadType);
            if ((inst->new_codec != 1) && (inst->timeStamp == uw32_availableTS)
                && (inst->timeStamp < dspInfo.playedOutTS) && (i_bufferpos != -1)
                && (WebRtcNetEQ_DbGetPayload(&(inst->codec_DB_inst),
                    (enum WebRtcNetEQDecoder) inst->current_Codec) == payloadType))
            {
                int waitingTime;
                temp_pkt.payload = blockPtr + 1;
                i_res = WebRtcNetEQ_PacketBufferExtract(&inst->PacketBuffer_inst, &temp_pkt,
                    i_bufferpos, &waitingTime);
                if (i_res < 0)
                { /* error returned */
                    return i_res;
                }
                WebRtcNetEQ_StoreWaitingTime(inst, waitingTime);
                *blockPtr = temp_pkt.payloadLen;
                /* set the flag if this is a redundant payload */
                if (temp_pkt.rcuPlCntr > 0)
                {
                    *blockPtr = (*blockPtr) | (DSP_CODEC_RED_FLAG);
                }
                blockPtr += ((temp_pkt.payloadLen + 1) >> 1) + 1;

                /*
                 * Close the data with a zero size block, in case we will not write any
                 * more data.
                 */
                *blockPtr = 0;
                inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0xf0ff)
                        | DSP_CODEC_ADD_LATE_PKT;
                latePacketExist = 1;
            }
        }
    }

    i_res = WebRtcNetEQ_PacketBufferFindLowestTimestamp(&inst->PacketBuffer_inst,
        dspInfo.playedOutTS, &uw32_availableTS, &i_bufferpos, (inst->new_codec == 0),
        &payloadType);
    if (i_res < 0)
    { /* error returned */
        return i_res;
    }

    if (inst->BufferStat_inst.w16_cngOn == CNG_RFC3389_ON)
    {
        /*
         * Because of timestamp peculiarities, we have to "manually" disallow using a CNG
         * packet with the same timestamp as the one that was last played. This can happen
         * when using redundancy and will cause the timing to shift.
         */
        while (i_bufferpos != -1 && WebRtcNetEQ_DbIsCNGPayload(&inst->codec_DB_inst,
            payloadType) && dspInfo.playedOutTS >= uw32_availableTS)
        {

            /* Don't use this packet, discard it */
            inst->PacketBuffer_inst.payloadType[i_bufferpos] = -1;
            inst->PacketBuffer_inst.payloadLengthBytes[i_bufferpos] = 0;
            inst->PacketBuffer_inst.numPacketsInBuffer--;

            /* Check buffer again */
            WebRtcNetEQ_PacketBufferFindLowestTimestamp(&inst->PacketBuffer_inst,
                dspInfo.playedOutTS, &uw32_availableTS, &i_bufferpos, (inst->new_codec == 0),
                &payloadType);
        }
    }

    /* Check packet buffer */
    w32_bufsize = WebRtcNetEQ_PacketBufferGetSize(&inst->PacketBuffer_inst);

    if (dspInfo.lastMode == MODE_SUCCESS_ACCELERATE || dspInfo.lastMode
        == MODE_LOWEN_ACCELERATE || dspInfo.lastMode == MODE_SUCCESS_PREEMPTIVE
        || dspInfo.lastMode == MODE_LOWEN_PREEMPTIVE)
    {
        /* Subtract (dspInfo.samplesLeft + inst->timestampsPerCall) from sampleMemory */
        inst->BufferStat_inst.Automode_inst.sampleMemory -= dspInfo.samplesLeft
            + inst->timestampsPerCall;
    }

    /* calculate total current buffer size (in ms*8), including sync buffer */
    w32_bufsize = WebRtcSpl_DivW32W16((w32_bufsize + dspInfo.samplesLeft), fs_mult);

#ifdef NETEQ_ATEVENT_DECODE
    /* DTMF data will affect the decision */
    if (WebRtcNetEQ_DtmfDecode(&inst->DTMF_inst, blockPtr + 1, blockPtr + 2,
        dspInfo.playedOutTS + inst->BufferStat_inst.uw32_CNGplayedTS) > 0)
    {
        playDtmf = 1;

        /* Flag DTMF payload */
        inst->pw16_writeAddress[0] = inst->pw16_writeAddress[0] | DSP_DTMF_PAYLOAD;

        /* Block Length in bytes */
        blockPtr[0] = 4;
        /* Advance to next payload position */
        blockPtr += 3;
    }
#endif

    /* Update statistics and make decision */
    uw16_instr = WebRtcNetEQ_BufstatsDecision(&inst->BufferStat_inst,
        inst->PacketBuffer_inst.packSizeSamples, w32_bufsize, dspInfo.playedOutTS,
        uw32_availableTS, i_bufferpos == -1,
        WebRtcNetEQ_DbIsCNGPayload(&inst->codec_DB_inst, payloadType), dspInfo.lastMode,
        inst->NetEqPlayoutMode, inst->timestampsPerCall, inst->NoOfExpandCalls, fs_mult,
        lastModeBGNonly, playDtmf);

    /* Check if time to reset loss counter */
    if (inst->lastReportTS > WEBRTC_SPL_UMUL(inst->fs, MAX_LOSS_REPORT_PERIOD))
    {
        /* reset loss counter */
        WebRtcNetEQ_ResetMcuInCallStats(inst);
    }

    /* Check sync buffer size */
    if ((dspInfo.samplesLeft >= inst->timestampsPerCall) && (uw16_instr
        != BUFSTATS_DO_ACCELERATE) && (uw16_instr != BUFSTATS_DO_MERGE) && (uw16_instr
            != BUFSTATS_DO_PREEMPTIVE_EXPAND))
    {
        *blockPtr = 0;
        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff) | DSP_INSTR_NORMAL;
        return 0;
    }

    if (uw16_instr == BUFSTATS_DO_EXPAND)
    {
        inst->NoOfExpandCalls++;
    }
    else
    {
        /* reset counter */
        inst->NoOfExpandCalls = 0;
    }

    /* New codec or big change in packet number? */
    if (((inst->new_codec) || (uw16_instr == BUFSTAT_REINIT)) && (uw16_instr
        != BUFSTATS_DO_EXPAND))
    {
        CodecFuncInst_t cinst;

        /* Clear other instructions */
        blockPtr = &inst->pw16_writeAddress[3];
        /* Clear instruction word */
        inst->pw16_writeAddress[0] = 0;

        inst->timeStamp = uw32_availableTS;
        dspInfo.playedOutTS = uw32_availableTS;
        if (inst->current_Codec != -1)
        {
            i_res = WebRtcNetEQ_DbGetPtrs(&inst->codec_DB_inst,
                (enum WebRtcNetEQDecoder) inst->current_Codec, &cinst);
            if (i_res < 0)
            { /* error returned */
                return i_res;
            }
        }
        else
        {
            /* The main codec has not been initialized yet (first packets are DTMF or CNG). */
            if (WebRtcNetEQ_DbIsCNGPayload(&inst->codec_DB_inst, payloadType))
            {
                /* The currently extracted packet is CNG; get CNG fs */
                WebRtc_UWord16 tempFs;

                tempFs = WebRtcNetEQ_DbGetSampleRate(&inst->codec_DB_inst, payloadType);
                if (tempFs > 0)
                {
                    inst->fs = tempFs;
                }
            }
            WebRtcSpl_MemSetW16((WebRtc_Word16*) &cinst, 0,
                                sizeof(CodecFuncInst_t) / sizeof(WebRtc_Word16));
            cinst.codec_fs = inst->fs;
        }
        cinst.timeStamp = inst->timeStamp;
        blockLen = (sizeof(CodecFuncInst_t)) >> (sizeof(WebRtc_Word16) - 1); /* in Word16 */
        *blockPtr = blockLen * 2;
        blockPtr++;
        WEBRTC_SPL_MEMCPY_W8(blockPtr,&cinst,sizeof(CodecFuncInst_t));
        blockPtr += blockLen;
        inst->new_codec = 0;

        /* Reinitialize the MCU fs */
        i_res = WebRtcNetEQ_McuSetFs(inst, cinst.codec_fs);
        if (i_res < 0)
        { /* error returned */
            return i_res;
        }

        /* Set the packet size by guessing */
        inst->PacketBuffer_inst.packSizeSamples = inst->timestampsPerCall * 3;

        WebRtcNetEQ_ResetAutomode(&(inst->BufferStat_inst.Automode_inst),
                                  inst->PacketBuffer_inst.maxInsertPositions);

#ifdef NETEQ_CNG_CODEC
        /* Also insert CNG state as this might be needed by DSP */
        i_res = WebRtcNetEQ_DbGetPtrs(&inst->codec_DB_inst, kDecoderCNG, &cinst);
        if ((i_res < 0) && (i_res != CODEC_DB_NOT_EXIST1))
        {
            /* other error returned */
            /* (CODEC_DB_NOT_EXIST1 simply indicates that CNG is not used */
            return i_res;
        }
        else
        {
            /* CNG exists */
            blockLen = (sizeof(cinst.codec_state)) >> (sizeof(WebRtc_Word16) - 1);
            *blockPtr = blockLen * 2;
            blockPtr++;
            WEBRTC_SPL_MEMCPY_W8(blockPtr,&cinst.codec_state,sizeof(cinst.codec_state));
            blockPtr += blockLen;
        }
#endif

        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0xf0ff)
                | DSP_CODEC_NEW_CODEC;

        if (uw16_instr == BUFSTATS_DO_RFC3389CNG_NOPACKET)
        {
            /*
             * Change decision to CNG packet, since we do have a CNG packet, but it was
             * considered too early to use. Now, use it anyway.
             */
            uw16_instr = BUFSTATS_DO_RFC3389CNG_PACKET;
        }
        else if (uw16_instr != BUFSTATS_DO_RFC3389CNG_PACKET)
        {
            uw16_instr = BUFSTATS_DO_NORMAL;
        }

        /* reset loss counter */
        WebRtcNetEQ_ResetMcuInCallStats(inst);
    }

    /* Should we just reset the decoder? */
    if (uw16_instr == BUFSTAT_REINIT_DECODER)
    {
        /* Change decision to normal and flag decoder reset */
        uw16_instr = BUFSTATS_DO_NORMAL;
        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0xf0ff) | DSP_CODEC_RESET;
    }

    /* Expand requires no new packet */
    if (uw16_instr == BUFSTATS_DO_EXPAND)
    {

        inst->timeStamp = dspInfo.playedOutTS;

        /* Have we got one descriptor left? */
        if (WebRtcNetEQ_DbIsMDCodec((enum WebRtcNetEQDecoder) inst->current_Codec)
            && (dspInfo.MD || latePacketExist))
        {

            if (dspInfo.lastMode != MODE_ONE_DESCRIPTOR)
            {
                /* this is the first "consecutive" one-descriptor decoding; reset counter */
                inst->one_desc = 0;
            }
            if (inst->one_desc < MAX_ONE_DESC)
            {
                /* use that one descriptor */
                inst->one_desc++; /* increase counter */
                inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                        | DSP_INSTR_NORMAL_ONE_DESC;

                /* decrease counter since we did no Expand */
                inst->NoOfExpandCalls = WEBRTC_SPL_MAX(inst->NoOfExpandCalls - 1, 0);
                return 0;
            }
            else
            {
                /* too many consecutive one-descriptor decodings; do expand instead */
                inst->one_desc = 0; /* reset counter */
            }

        }

        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff) | DSP_INSTR_EXPAND;
        return 0;
    }

    /* Merge is not needed if we still have a descriptor */
    if ((uw16_instr == BUFSTATS_DO_MERGE) && (dspInfo.MD != 0))
    {
        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                | DSP_INSTR_NORMAL_ONE_DESC;
        *blockPtr = 0;
        return 0;
    }

    /* Do CNG without trying to extract any packets from buffer */
    if (uw16_instr == BUFSTATS_DO_RFC3389CNG_NOPACKET)
    {
        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                | DSP_INSTR_DO_RFC3389CNG;
        *blockPtr = 0;
        return 0;
    }

    /* Do built-in CNG without extracting any new packets from buffer */
    if (uw16_instr == BUFSTATS_DO_INTERNAL_CNG_NOPACKET)
    {
        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                | DSP_INSTR_DO_CODEC_INTERNAL_CNG;
        *blockPtr = 0;
        return 0;
    }

    /* Do DTMF without extracting any new packets from buffer */
    if (uw16_instr == BUFSTATS_DO_DTMF_ONLY)
    {
        WebRtc_UWord32 timeStampJump = 0;

        /* Update timestamp */
        if ((inst->BufferStat_inst.uw32_CNGplayedTS > 0) && (dspInfo.lastMode != MODE_DTMF))
        {
            /* Jump in timestamps if needed */
            timeStampJump = inst->BufferStat_inst.uw32_CNGplayedTS;
            inst->pw16_writeAddress[1] = (WebRtc_UWord16) (timeStampJump >> 16);
            inst->pw16_writeAddress[2] = (WebRtc_UWord16) (timeStampJump & 0xFFFF);
        }

        inst->timeStamp = dspInfo.playedOutTS + timeStampJump;

        inst->BufferStat_inst.uw32_CNGplayedTS = 0;
        inst->NoOfExpandCalls = 0;

        inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                | DSP_INSTR_DTMF_GENERATE;
        *blockPtr = 0;
        return 0;
    }

    if (uw16_instr == BUFSTATS_DO_ACCELERATE)
    {
        /* In order to do a Accelerate we need at least 30 ms of data */
        if (dspInfo.samplesLeft >= (3 * 80 * fs_mult))
        {
            /* Already have enough data, so we do not need to extract any more */
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_ACCELERATE;
            *blockPtr = 0;
            inst->BufferStat_inst.Automode_inst.sampleMemory
            = (WebRtc_Word32) dspInfo.samplesLeft;
            inst->BufferStat_inst.Automode_inst.prevTimeScale = 1;
            return 0;
        }
        else if ((dspInfo.samplesLeft >= (1 * 80 * fs_mult))
            && (inst->PacketBuffer_inst.packSizeSamples >= (240 * fs_mult)))
        {
            /* Avoid decoding more data as it might overflow playout buffer */
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_NORMAL;
            *blockPtr = 0;
            return 0;
        }
        else if ((dspInfo.samplesLeft < (1 * 80 * fs_mult))
            && (inst->PacketBuffer_inst.packSizeSamples >= (240 * fs_mult)))
        {
            /* For >= 30ms allow Accelerate with a decoding to avoid overflow in playout buffer */
            wantedNoOfTimeStamps = inst->timestampsPerCall;
        }
        else if (dspInfo.samplesLeft >= (2 * 80 * fs_mult))
        {
            /* We need to decode another 10 ms in order to do an Accelerate */
            wantedNoOfTimeStamps = inst->timestampsPerCall;
        }
        else
        {
            /*
             * Build up decoded data by decoding at least 20 ms of data.
             * Do not perform Accelerate yet, but wait until we only need to do one decoding.
             */
            wantedNoOfTimeStamps = 2 * inst->timestampsPerCall;
            uw16_instr = BUFSTATS_DO_NORMAL;
        }
    }
    else if (uw16_instr == BUFSTATS_DO_PREEMPTIVE_EXPAND)
    {
        /* In order to do a Preemptive Expand we need at least 30 ms of data */
        if (dspInfo.samplesLeft >= (3 * 80 * fs_mult))
        {
            /* Already have enough data, so we do not need to extract any more */
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_PREEMPTIVE_EXPAND;
            *blockPtr = 0;
            inst->BufferStat_inst.Automode_inst.sampleMemory
            = (WebRtc_Word32) dspInfo.samplesLeft;
            inst->BufferStat_inst.Automode_inst.prevTimeScale = 1;
            return 0;
        }
        else if ((dspInfo.samplesLeft >= (1 * 80 * fs_mult))
            && (inst->PacketBuffer_inst.packSizeSamples >= (240 * fs_mult)))
        {
            /*
             * Avoid decoding more data as it might overflow playout buffer;
             * still try Preemptive Expand though.
             */
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_PREEMPTIVE_EXPAND;
            *blockPtr = 0;
            inst->BufferStat_inst.Automode_inst.sampleMemory
            = (WebRtc_Word32) dspInfo.samplesLeft;
            inst->BufferStat_inst.Automode_inst.prevTimeScale = 1;
            return 0;
        }
        else if ((dspInfo.samplesLeft < (1 * 80 * fs_mult))
            && (inst->PacketBuffer_inst.packSizeSamples >= (240 * fs_mult)))
        {
            /*
             * For >= 30ms allow Preemptive Expand with a decoding to avoid overflow in
             * playout buffer
             */
            wantedNoOfTimeStamps = inst->timestampsPerCall;
        }
        else if (dspInfo.samplesLeft >= (2 * 80 * fs_mult))
        {
            /* We need to decode another 10 ms in order to do an Preemptive Expand */
            wantedNoOfTimeStamps = inst->timestampsPerCall;
        }
        else
        {
            /*
             * Build up decoded data by decoding at least 20 ms of data,
             * Still try to perform Preemptive Expand.
             */
            wantedNoOfTimeStamps = 2 * inst->timestampsPerCall;
        }
    }
    else
    {
        wantedNoOfTimeStamps = inst->timestampsPerCall;
    }

    /* Otherwise get data from buffer, try to get at least 10ms */
    totalTS = 0;
    oldTS = uw32_availableTS;
    if ((i_bufferpos > -1) && (uw16_instr != BUFSTATS_DO_ALTERNATIVE_PLC) && (uw16_instr
        != BUFSTATS_DO_ALTERNATIVE_PLC_INC_TS) && (uw16_instr != BUFSTATS_DO_AUDIO_REPETITION)
        && (uw16_instr != BUFSTATS_DO_AUDIO_REPETITION_INC_TS))
    {
        uw32_tmp = (uw32_availableTS - dspInfo.playedOutTS);
        inst->pw16_writeAddress[1] = (WebRtc_UWord16) (uw32_tmp >> 16);
        inst->pw16_writeAddress[2] = (WebRtc_UWord16) (uw32_tmp & 0xFFFF);
        if (inst->BufferStat_inst.w16_cngOn == CNG_OFF)
        {
            /*
             * Adjustment of TS only corresponds to an actual packet loss
             * if comfort noise is not played. If comfort noise was just played,
             * this adjustment of TS is only done to get back in sync with the
             * stream TS; no loss to report.
             */
            inst->lostTS += uw32_tmp;
        }

        if (uw16_instr != BUFSTATS_DO_RFC3389CNG_PACKET)
        {
            /* We are about to decode and use a non-CNG packet => CNG period is ended */
            inst->BufferStat_inst.w16_cngOn = CNG_OFF;
        }

        /*
         * Reset CNG timestamp as a new packet will be delivered.
         * (Also if CNG packet, since playedOutTS is updated.)
         */
        inst->BufferStat_inst.uw32_CNGplayedTS = 0;

        prevSeqNo = inst->PacketBuffer_inst.seqNumber[i_bufferpos];
        prevTS = inst->PacketBuffer_inst.timeStamp[i_bufferpos];
        oldPT = inst->PacketBuffer_inst.payloadType[i_bufferpos];

        /* clear flag bits */
        inst->pw16_writeAddress[0] = inst->pw16_writeAddress[0] & 0xFF3F;
        do
        {
            int waitingTime;
            inst->timeStamp = uw32_availableTS;
            /* Write directly to shared memory */
            temp_pkt.payload = blockPtr + 1;
            i_res = WebRtcNetEQ_PacketBufferExtract(&inst->PacketBuffer_inst, &temp_pkt,
                i_bufferpos, &waitingTime);

            if (i_res < 0)
            {
                /* error returned */
                return i_res;
            }
            WebRtcNetEQ_StoreWaitingTime(inst, waitingTime);

#ifdef NETEQ_DELAY_LOGGING
            temp_var = NETEQ_DELAY_LOGGING_SIGNAL_DECODE;
            fwrite(&temp_var,sizeof(int),1,delay_fid2);
            fwrite(&temp_pkt.timeStamp,sizeof(WebRtc_UWord32),1,delay_fid2);
            fwrite(&dspInfo.samplesLeft, sizeof(WebRtc_UWord16), 1, delay_fid2);
#endif

            *blockPtr = temp_pkt.payloadLen;
            /* set the flag if this is a redundant payload */
            if (temp_pkt.rcuPlCntr > 0)
            {
                *blockPtr = (*blockPtr) | (DSP_CODEC_RED_FLAG);
            }
            blockPtr += ((temp_pkt.payloadLen + 1) >> 1) + 1;

            if (i_bufferpos > -1)
            {
                /*
                 * Store number of TS extracted (last extracted is assumed to be of
                 * packSizeSamples).
                 */
                totalTS = uw32_availableTS - oldTS + inst->PacketBuffer_inst.packSizeSamples;
            }
            /* Check what next packet is available */
            WebRtcNetEQ_PacketBufferFindLowestTimestamp(&inst->PacketBuffer_inst,
                inst->timeStamp, &uw32_availableTS, &i_bufferpos, 0, &payloadType);

            nextSeqNoAvail = 0;
            if ((i_bufferpos > -1) && (oldPT
                == inst->PacketBuffer_inst.payloadType[i_bufferpos]))
            {
                w16_tmp = inst->PacketBuffer_inst.seqNumber[i_bufferpos] - prevSeqNo;
                w32_tmp = inst->PacketBuffer_inst.timeStamp[i_bufferpos] - prevTS;
                if ((w16_tmp == 1) || /* Next packet */
                    ((w16_tmp == 0) && (w32_tmp == inst->PacketBuffer_inst.packSizeSamples)))
                { /* or packet split into frames */
                    nextSeqNoAvail = 1;
                }
                prevSeqNo = inst->PacketBuffer_inst.seqNumber[i_bufferpos];
            }

        }
        while ((totalTS < wantedNoOfTimeStamps) && (nextSeqNoAvail == 1));
    }

    if ((uw16_instr == BUFSTATS_DO_ACCELERATE)
        || (uw16_instr == BUFSTATS_DO_PREEMPTIVE_EXPAND))
    {
        /* Check that we have enough data (30ms) to do the Accelearate */
        if ((totalTS + dspInfo.samplesLeft) < WEBRTC_SPL_MUL(3,inst->timestampsPerCall)
            && (uw16_instr == BUFSTATS_DO_ACCELERATE))
        {
            /* Not enough, do normal operation instead */
            uw16_instr = BUFSTATS_DO_NORMAL;
        }
        else
        {
            inst->BufferStat_inst.Automode_inst.sampleMemory
            = (WebRtc_Word32) dspInfo.samplesLeft + totalTS;
            inst->BufferStat_inst.Automode_inst.prevTimeScale = 1;
        }
    }

    /* Close the data with a zero size block */
    *blockPtr = 0;

    /* Write data to DSP */
    switch (uw16_instr)
    {
        case BUFSTATS_DO_NORMAL:
            /* Normal with decoding included */
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
            | DSP_INSTR_NORMAL;
            break;
        case BUFSTATS_DO_ACCELERATE:
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
            | DSP_INSTR_ACCELERATE;
            break;
        case BUFSTATS_DO_MERGE:
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
            | DSP_INSTR_MERGE;
            break;
        case BUFSTATS_DO_RFC3389CNG_PACKET:
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
            | DSP_INSTR_DO_RFC3389CNG;
            break;
        case BUFSTATS_DO_ALTERNATIVE_PLC:
            inst->pw16_writeAddress[1] = 0;
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_DO_ALTERNATIVE_PLC;
            break;
        case BUFSTATS_DO_ALTERNATIVE_PLC_INC_TS:
            inst->pw16_writeAddress[1] = 0;
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_DO_ALTERNATIVE_PLC_INC_TS;
            break;
        case BUFSTATS_DO_AUDIO_REPETITION:
            inst->pw16_writeAddress[1] = 0;
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_DO_AUDIO_REPETITION;
            break;
        case BUFSTATS_DO_AUDIO_REPETITION_INC_TS:
            inst->pw16_writeAddress[1] = 0;
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
                    | DSP_INSTR_DO_AUDIO_REPETITION_INC_TS;
            break;
        case BUFSTATS_DO_PREEMPTIVE_EXPAND:
            inst->pw16_writeAddress[0] = (inst->pw16_writeAddress[0] & 0x0fff)
            | DSP_INSTR_PREEMPTIVE_EXPAND;
            break;
        default:
            return UNKNOWN_BUFSTAT_DECISION;
    }

    inst->timeStamp = dspInfo.playedOutTS;
    return 0;

}

