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
 * Implementation of the RecIn function, which is the main function for inserting RTP
 * packets into NetEQ.
 */

#include "mcu.h"

#include <string.h>

#include "signal_processing_library.h"

#include "automode.h"
#include "dtmf_buffer.h"
#include "neteq_defines.h"
#include "neteq_error_codes.h"


int WebRtcNetEQ_RecInInternal(MCUInst_t *MCU_inst, RTPPacket_t *RTPpacketInput,
                              WebRtc_UWord32 uw32_timeRec)
{
    RTPPacket_t RTPpacket[2];
    int i_k;
    int i_ok = 0, i_No_Of_Payloads = 1;
    WebRtc_Word16 flushed = 0;
    WebRtc_Word16 codecPos;
    int curr_Codec;
    WebRtc_Word16 isREDPayload = 0;
    WebRtc_Word32 temp_bufsize;
#ifdef NETEQ_RED_CODEC
    RTPPacket_t* RTPpacketPtr[2]; /* Support for redundancy up to 2 payloads */
    RTPpacketPtr[0] = &RTPpacket[0];
    RTPpacketPtr[1] = &RTPpacket[1];
#endif

    temp_bufsize = WebRtcNetEQ_PacketBufferGetSize(&MCU_inst->PacketBuffer_inst,
                                                   &MCU_inst->codec_DB_inst);
    /*
     * Copy from input RTP packet to local copy
     * (mainly to enable multiple payloads using RED)
     */

    WEBRTC_SPL_MEMCPY_W8(&RTPpacket[0], RTPpacketInput, sizeof(RTPPacket_t));

    /* Reinitialize NetEq if it's needed (changed SSRC or first call) */

    if ((RTPpacket[0].ssrc != MCU_inst->ssrc) || (MCU_inst->first_packet == 1))
    {
        WebRtcNetEQ_RTCPInit(&MCU_inst->RTCP_inst, RTPpacket[0].seqNumber);
        MCU_inst->first_packet = 0;

        /* Flush the buffer */
        WebRtcNetEQ_PacketBufferFlush(&MCU_inst->PacketBuffer_inst);

        /* Store new SSRC */
        MCU_inst->ssrc = RTPpacket[0].ssrc;

        /* Update codecs */
        MCU_inst->timeStamp = RTPpacket[0].timeStamp;
        MCU_inst->current_Payload = RTPpacket[0].payloadType;

        /*Set MCU to update codec on next SignalMCU call */
        MCU_inst->new_codec = 1;

        /* Reset timestamp scaling */
        MCU_inst->TSscalingInitialized = 0;

    }

    /* Call RTCP statistics */
    i_ok |= WebRtcNetEQ_RTCPUpdate(&(MCU_inst->RTCP_inst), RTPpacket[0].seqNumber,
        RTPpacket[0].timeStamp, uw32_timeRec);

    /* If Redundancy is supported and this is the redundancy payload, separate the payloads */
#ifdef NETEQ_RED_CODEC
    if (RTPpacket[0].payloadType == WebRtcNetEQ_DbGetPayload(&MCU_inst->codec_DB_inst,
        kDecoderRED))
    {

        /* Split the payload into a main and a redundancy payloads */
        i_ok = WebRtcNetEQ_RedundancySplit(RTPpacketPtr, 2, &i_No_Of_Payloads);
        if (i_ok < 0)
        {
            /* error returned */
            return i_ok;
        }

        /*
         * Only accept a few redundancies of the same type as the main data,
         * AVT events and CNG.
         */
        if ((i_No_Of_Payloads > 1) && (RTPpacket[0].payloadType != RTPpacket[1].payloadType)
            && (RTPpacket[0].payloadType != WebRtcNetEQ_DbGetPayload(&MCU_inst->codec_DB_inst,
                kDecoderAVT)) && (RTPpacket[1].payloadType != WebRtcNetEQ_DbGetPayload(
            &MCU_inst->codec_DB_inst, kDecoderAVT)) && (!WebRtcNetEQ_DbIsCNGPayload(
            &MCU_inst->codec_DB_inst, RTPpacket[0].payloadType))
            && (!WebRtcNetEQ_DbIsCNGPayload(&MCU_inst->codec_DB_inst, RTPpacket[1].payloadType)))
        {
            i_No_Of_Payloads = 1;
        }
        isREDPayload = 1;
    }
#endif

    /* loop over the number of payloads */
    for (i_k = 0; i_k < i_No_Of_Payloads; i_k++)
    {

        if (isREDPayload == 1)
        {
            RTPpacket[i_k].rcuPlCntr = i_k;
        }
        else
        {
            RTPpacket[i_k].rcuPlCntr = 0;
        }

        /* Force update of SplitInfo if it's iLBC because of potential change between 20/30ms */
        if (RTPpacket[i_k].payloadType == WebRtcNetEQ_DbGetPayload(&MCU_inst->codec_DB_inst,
            kDecoderILBC))
        {
            i_ok = WebRtcNetEQ_DbGetSplitInfo(
                &MCU_inst->PayloadSplit_inst,
                (enum WebRtcNetEQDecoder) WebRtcNetEQ_DbGetCodec(&MCU_inst->codec_DB_inst,
                    RTPpacket[i_k].payloadType), RTPpacket[i_k].payloadLen);
            if (i_ok < 0)
            {
                /* error returned */
                return i_ok;
            }
        }

        /* Get information about timestamp scaling for this payload type */
        i_ok = WebRtcNetEQ_GetTimestampScaling(MCU_inst, RTPpacket[i_k].payloadType);
        if (i_ok < 0)
        {
            /* error returned */
            return i_ok;
        }

        if (MCU_inst->TSscalingInitialized == 0 && MCU_inst->scalingFactor != kTSnoScaling)
        {
            /* Must initialize scaling with current timestamps */
            MCU_inst->externalTS = RTPpacket[i_k].timeStamp;
            MCU_inst->internalTS = RTPpacket[i_k].timeStamp;
            MCU_inst->TSscalingInitialized = 1;
        }

        /* Adjust timestamp if timestamp scaling is needed (e.g. SILK or G.722) */
        if (MCU_inst->TSscalingInitialized == 1)
        {
            WebRtc_UWord32 newTS = WebRtcNetEQ_ScaleTimestampExternalToInternal(MCU_inst,
                RTPpacket[i_k].timeStamp);

            /* save the incoming timestamp for next time */
            MCU_inst->externalTS = RTPpacket[i_k].timeStamp;

            /* add the scaled difference to last scaled timestamp and save ... */
            MCU_inst->internalTS = newTS;

            RTPpacket[i_k].timeStamp = newTS;
        }

        /* Is this a DTMF packet?*/
        if (RTPpacket[i_k].payloadType == WebRtcNetEQ_DbGetPayload(&MCU_inst->codec_DB_inst,
            kDecoderAVT))
        {
#ifdef NETEQ_ATEVENT_DECODE
            if (MCU_inst->AVT_PlayoutOn)
            {
                i_ok = WebRtcNetEQ_DtmfInsertEvent(&MCU_inst->DTMF_inst,
                    RTPpacket[i_k].payload, RTPpacket[i_k].payloadLen,
                    RTPpacket[i_k].timeStamp);
                if (i_ok != 0)
                {
                    return i_ok;
                }
            }
#endif
#ifdef NETEQ_STEREO
            if (MCU_inst->usingStereo == 0)
            {
                /* do not set this for DTMF packets when using stereo mode */
                MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF = 1;
            }
#else
            MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF = 1;
#endif
        }
        else if (WebRtcNetEQ_DbIsCNGPayload(&MCU_inst->codec_DB_inst,
            RTPpacket[i_k].payloadType))
        {
            /* Is this a CNG packet? how should we handle this?*/
#ifdef NETEQ_CNG_CODEC
            /* Get CNG sample rate */
            WebRtc_UWord16 fsCng = WebRtcNetEQ_DbGetSampleRate(&MCU_inst->codec_DB_inst,
                RTPpacket[i_k].payloadType);

            /* Force sampling frequency to 32000 Hz CNG 48000 Hz. */
            /* TODO(tlegrand): remove limitation once ACM has full 48 kHz
             * support. */
            if (fsCng > 32000) {
                fsCng = 32000;
            }
            if ((fsCng != MCU_inst->fs) && (fsCng > 8000))
            {
                /*
                 * We have received CNG with a different sample rate from what we are using
                 * now (must be > 8000, since we may use only one CNG type (default) for all
                 * frequencies). Flush buffer and signal new codec.
                 */
                WebRtcNetEQ_PacketBufferFlush(&MCU_inst->PacketBuffer_inst);
                MCU_inst->new_codec = 1;
                MCU_inst->current_Codec = -1;
            }
            i_ok = WebRtcNetEQ_PacketBufferInsert(&MCU_inst->PacketBuffer_inst,
                &RTPpacket[i_k], &flushed);
            if (i_ok < 0)
            {
                return RECIN_CNG_ERROR;
            }
            MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF = 1;
#else /* NETEQ_CNG_CODEC not defined */
            return RECIN_UNKNOWNPAYLOAD;
#endif /* NETEQ_CNG_CODEC */
        }
        else
        {
            /* Reinitialize the splitting if the payload and/or the payload length has changed */
            curr_Codec = WebRtcNetEQ_DbGetCodec(&MCU_inst->codec_DB_inst,
                RTPpacket[i_k].payloadType);
            if (curr_Codec != MCU_inst->current_Codec)
            {
                if (curr_Codec < 0)
                {
                    return RECIN_UNKNOWNPAYLOAD;
                }
                MCU_inst->current_Codec = curr_Codec;
                MCU_inst->current_Payload = RTPpacket[i_k].payloadType;
                i_ok = WebRtcNetEQ_DbGetSplitInfo(&MCU_inst->PayloadSplit_inst,
                    (enum WebRtcNetEQDecoder) MCU_inst->current_Codec,
                    RTPpacket[i_k].payloadLen);
                if (i_ok < 0)
                { /* error returned */
                    return i_ok;
                }
                WebRtcNetEQ_PacketBufferFlush(&MCU_inst->PacketBuffer_inst);
                MCU_inst->new_codec = 1;
            }

            /* Parse the payload and insert it into the buffer */
            i_ok = WebRtcNetEQ_SplitAndInsertPayload(&RTPpacket[i_k],
                &MCU_inst->PacketBuffer_inst, &MCU_inst->PayloadSplit_inst, &flushed);
            if (i_ok < 0)
            {
                return i_ok;
            }
            if (MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF != 0)
            {
                /* first normal packet after CNG or DTMF */
                MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF = -1;
            }
        }
        /* Reset DSP timestamp etc. if packet buffer flushed */
        if (flushed)
        {
            MCU_inst->new_codec = 1;
        }
    }

    /*
     * Update Bandwidth Estimate
     * Only send the main payload to BWE
     */
    if ((curr_Codec = WebRtcNetEQ_DbGetCodec(&MCU_inst->codec_DB_inst,
        RTPpacket[0].payloadType)) >= 0)
    {
        codecPos = MCU_inst->codec_DB_inst.position[curr_Codec];
        if (MCU_inst->codec_DB_inst.funcUpdBWEst[codecPos] != NULL) /* codec has BWE function */
        {
            if (RTPpacket[0].starts_byte1) /* check for shifted byte alignment */
            {
                /* re-align to 16-bit alignment */
                for (i_k = 0; i_k < RTPpacket[0].payloadLen; i_k++)
                {
                    WEBRTC_SPL_SET_BYTE(RTPpacket[0].payload,
                        WEBRTC_SPL_GET_BYTE(RTPpacket[0].payload, i_k+1),
                        i_k);
                }
                RTPpacket[0].starts_byte1 = 0;
            }

            MCU_inst->codec_DB_inst.funcUpdBWEst[codecPos](
                MCU_inst->codec_DB_inst.codec_state[codecPos],
                (G_CONST WebRtc_UWord16 *) RTPpacket[0].payload,
                (WebRtc_Word32) RTPpacket[0].payloadLen, RTPpacket[0].seqNumber,
                (WebRtc_UWord32) RTPpacket[0].timeStamp, (WebRtc_UWord32) uw32_timeRec);
        }
    }

    if (MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF == 0)
    {
        /* Calculate the total speech length carried in each packet */
        temp_bufsize = WebRtcNetEQ_PacketBufferGetSize(
            &MCU_inst->PacketBuffer_inst, &MCU_inst->codec_DB_inst)
            - temp_bufsize;

        if ((temp_bufsize > 0) && (MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF
            == 0) && (temp_bufsize
            != MCU_inst->BufferStat_inst.Automode_inst.packetSpeechLenSamp))
        {
            /* Change the auto-mode parameters if packet length has changed */
            WebRtcNetEQ_SetPacketSpeechLen(&(MCU_inst->BufferStat_inst.Automode_inst),
                (WebRtc_Word16) temp_bufsize, MCU_inst->fs);
        }

        /* update statistics */
        if ((WebRtc_Word32) (RTPpacket[0].timeStamp - MCU_inst->timeStamp) >= 0
            && !MCU_inst->new_codec)
        {
            /*
             * Only update statistics if incoming packet is not older than last played out
             * packet, and if new codec flag is not set.
             */
            WebRtcNetEQ_UpdateIatStatistics(&MCU_inst->BufferStat_inst.Automode_inst,
                MCU_inst->PacketBuffer_inst.maxInsertPositions, RTPpacket[0].seqNumber,
                RTPpacket[0].timeStamp, MCU_inst->fs,
                WebRtcNetEQ_DbIsMDCodec((enum WebRtcNetEQDecoder) MCU_inst->current_Codec),
                (MCU_inst->NetEqPlayoutMode == kPlayoutStreaming));
        }
    }
    else if (MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF == -1)
    {
        /*
         * This is first "normal" packet after CNG or DTMF.
         * Reset packet time counter and measure time until next packet,
         * but don't update statistics.
         */
        MCU_inst->BufferStat_inst.Automode_inst.lastPackCNGorDTMF = 0;
        MCU_inst->BufferStat_inst.Automode_inst.packetIatCountSamp = 0;
    }
    return 0;

}

int WebRtcNetEQ_GetTimestampScaling(MCUInst_t *MCU_inst, int rtpPayloadType)
{
    enum WebRtcNetEQDecoder codec;
    int codecNumber;

    codecNumber = WebRtcNetEQ_DbGetCodec(&MCU_inst->codec_DB_inst, rtpPayloadType);
    if (codecNumber < 0)
    {
        /* error */
        return codecNumber;
    }

    /* cast to enumerator */
    codec = (enum WebRtcNetEQDecoder) codecNumber;

    /*
     * The factor obtained below is the number with which the RTP timestamp must be
     * multiplied to get the true sample count.
     */
    switch (codec)
    {
        case kDecoderG722:
        case kDecoderG722_2ch:
        {
            /* Use timestamp scaling with factor 2 (two output samples per RTP timestamp) */
            MCU_inst->scalingFactor = kTSscalingTwo;
            break;
        }
        case kDecoderISACfb:
        case kDecoderOpus:
        {
            /* We resample Opus internally to 32 kHz, and isac-fb decodes at
             * 32 kHz, but timestamps are counted at 48 kHz. So there are two
             * output samples per three RTP timestamp ticks. */
            MCU_inst->scalingFactor = kTSscalingTwoThirds;
            break;
        }

        case kDecoderAVT:
        case kDecoderCNG:
        {
            /* TODO(tlegrand): remove scaling once ACM has full 48 kHz
             * support. */
            WebRtc_UWord16 sample_freq =
                WebRtcNetEQ_DbGetSampleRate(&MCU_inst->codec_DB_inst,
                                            rtpPayloadType);
            if (sample_freq == 48000) {
              MCU_inst->scalingFactor = kTSscalingTwoThirds;
            }

            /* For sample_freq <= 32 kHz, do not change the timestamp scaling
             * settings. */
            break;
        }
        default:
        {
            /* do not use timestamp scaling */
            MCU_inst->scalingFactor = kTSnoScaling;
            break;
        }
    }
    return 0;
}

WebRtc_UWord32 WebRtcNetEQ_ScaleTimestampExternalToInternal(const MCUInst_t *MCU_inst,
                                                            WebRtc_UWord32 externalTS)
{
    WebRtc_Word32 timestampDiff;
    WebRtc_UWord32 internalTS;

    /* difference between this and last incoming timestamp */
    timestampDiff = externalTS - MCU_inst->externalTS;

    switch (MCU_inst->scalingFactor)
    {
        case kTSscalingTwo:
        {
            /* multiply with 2 */
            timestampDiff = WEBRTC_SPL_LSHIFT_W32(timestampDiff, 1);
            break;
        }
        case kTSscalingTwoThirds:
        {
            /* multiply with 2/3 */
            timestampDiff = WEBRTC_SPL_LSHIFT_W32(timestampDiff, 1);
            timestampDiff = WebRtcSpl_DivW32W16(timestampDiff, 3);
            break;
        }
        case kTSscalingFourThirds:
        {
            /* multiply with 4/3 */
            timestampDiff = WEBRTC_SPL_LSHIFT_W32(timestampDiff, 2);
            timestampDiff = WebRtcSpl_DivW32W16(timestampDiff, 3);
            break;
        }
        default:
        {
            /* no scaling */
        }
    }

    /* add the scaled difference to last scaled timestamp and save ... */
    internalTS = MCU_inst->internalTS + timestampDiff;

    return internalTS;
}

WebRtc_UWord32 WebRtcNetEQ_ScaleTimestampInternalToExternal(const MCUInst_t *MCU_inst,
                                                            WebRtc_UWord32 internalTS)
{
    WebRtc_Word32 timestampDiff;
    WebRtc_UWord32 externalTS;

    /* difference between this and last incoming timestamp */
    timestampDiff = (WebRtc_Word32) internalTS - MCU_inst->internalTS;

    switch (MCU_inst->scalingFactor)
    {
        case kTSscalingTwo:
        {
            /* divide by 2 */
            timestampDiff = WEBRTC_SPL_RSHIFT_W32(timestampDiff, 1);
            break;
        }
        case kTSscalingTwoThirds:
        {
            /* multiply with 3/2 */
            timestampDiff = WEBRTC_SPL_MUL_32_16(timestampDiff, 3);
            timestampDiff = WEBRTC_SPL_RSHIFT_W32(timestampDiff, 1);
            break;
        }
        case kTSscalingFourThirds:
        {
            /* multiply with 3/4 */
            timestampDiff = WEBRTC_SPL_MUL_32_16(timestampDiff, 3);
            timestampDiff = WEBRTC_SPL_RSHIFT_W32(timestampDiff, 2);
            break;
        }
        default:
        {
            /* no scaling */
        }
    }

    /* add the scaled difference to last scaled timestamp and save ... */
    externalTS = MCU_inst->externalTS + timestampDiff;

    return externalTS;
}
