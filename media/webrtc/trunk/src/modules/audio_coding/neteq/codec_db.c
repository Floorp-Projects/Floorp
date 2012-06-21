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
 * Implementation of the codec database.
 */

#include "codec_db.h"

#include <string.h> /* to define NULL */

#include "signal_processing_library.h"

#include "neteq_error_codes.h"

/*
 * Resets the codec database.
 */

int WebRtcNetEQ_DbReset(CodecDbInst_t *inst)
{
    int i;

    WebRtcSpl_MemSetW16((WebRtc_Word16*) inst, 0,
        sizeof(CodecDbInst_t) / sizeof(WebRtc_Word16));

    for (i = 0; i < NUM_TOTAL_CODECS; i++)
    {
        inst->position[i] = -1;
    }

    for (i = 0; i < NUM_CODECS; i++)
    {
        inst->payloadType[i] = -1;
    }

    for (i = 0; i < NUM_CNG_CODECS; i++)
    {
        inst->CNGpayloadType[i] = -1;
    }

    return 0;
}

/*
 * Adds a new codec to the database.
 */

int WebRtcNetEQ_DbAdd(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codec,
                      WebRtc_Word16 payloadType, FuncDecode funcDecode,
                      FuncDecode funcDecodeRCU, FuncDecodePLC funcDecodePLC,
                      FuncDecodeInit funcDecodeInit, FuncAddLatePkt funcAddLatePkt,
                      FuncGetMDinfo funcGetMDinfo, FuncGetPitchInfo funcGetPitch,
                      FuncUpdBWEst funcUpdBWEst, FuncGetErrorCode funcGetErrorCode,
                      void* codec_state, WebRtc_UWord16 codec_fs)
{

    int temp;
    int insertCNGcodec = 0, overwriteCNGcodec = 0, CNGpos = -1;

#ifndef NETEQ_RED_CODEC
    if (codec == kDecoderRED)
    {
        return CODEC_DB_UNSUPPORTED_CODEC;
    }
#endif
    if (((int) codec <= (int) kDecoderReservedStart) || ((int) codec
        >= (int) kDecoderReservedEnd))
    {
        return CODEC_DB_UNSUPPORTED_CODEC;
    }

    if ((codec_fs != 8000)
#ifdef NETEQ_WIDEBAND
    &&(codec_fs!=16000)
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
    &&(codec_fs!=32000)
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
    &&(codec_fs!=48000)
#endif
    )
    {
        return CODEC_DB_UNSUPPORTED_FS;
    }

    /* Ensure that the codec type is supported */
    switch (codec)
    {
#ifdef NETEQ_PCM16B_CODEC
        case kDecoderPCM16B :
#endif
#ifdef NETEQ_G711_CODEC
        case kDecoderPCMu :
        case kDecoderPCMa :
#endif
#ifdef NETEQ_ILBC_CODEC
        case kDecoderILBC :
#endif
#ifdef NETEQ_ISAC_CODEC
        case kDecoderISAC :
#endif
#ifdef NETEQ_ISAC_SWB_CODEC
        case kDecoderISACswb :
#endif
#ifdef NETEQ_G722_CODEC
        case kDecoderG722 :
#endif
#ifdef NETEQ_WIDEBAND
        case kDecoderPCM16Bwb :
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
        case kDecoderPCM16Bswb32kHz :
#endif
#ifdef NETEQ_CNG_CODEC
        case kDecoderCNG :
#endif
#ifdef NETEQ_ATEVENT_DECODE
        case kDecoderAVT :
#endif
#ifdef NETEQ_RED_CODEC
        case kDecoderRED :
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
        case kDecoderPCM16Bswb48kHz :
#endif
#ifdef NETEQ_ARBITRARY_CODEC
        case kDecoderArbitrary:
#endif
#ifdef NETEQ_G729_CODEC
        case kDecoderG729:
#endif
#ifdef NETEQ_G729_1_CODEC
        case kDecoderG729_1 :
#endif
#ifdef NETEQ_G726_CODEC
        case kDecoderG726_16 :
        case kDecoderG726_24 :
        case kDecoderG726_32 :
        case kDecoderG726_40 :
#endif
#ifdef NETEQ_G722_1_CODEC
        case kDecoderG722_1_16 :
        case kDecoderG722_1_24 :
        case kDecoderG722_1_32 :
#endif
#ifdef NETEQ_G722_1C_CODEC
        case kDecoderG722_1C_24 :
        case kDecoderG722_1C_32 :
        case kDecoderG722_1C_48 :
#endif
#ifdef NETEQ_SPEEX_CODEC
        case kDecoderSPEEX_8 :
        case kDecoderSPEEX_16 :
#endif
#ifdef NETEQ_CELT_CODEC
        case kDecoderCELT_32 :
#endif
#ifdef NETEQ_GSMFR_CODEC
        case kDecoderGSMFR :
#endif
#ifdef NETEQ_AMR_CODEC
        case kDecoderAMR :
#endif
#ifdef NETEQ_AMRWB_CODEC
        case kDecoderAMRWB :
#endif
        {
            /* If we end up here, the inserted codec is supported => Do nothing */
            break;
        }
    default:
    {
        /* If we get to this point, the inserted codec is not supported */
        return CODEC_DB_UNSUPPORTED_CODEC;
    }
    }

    /* Check to see if payload type is taken */
    if (WebRtcNetEQ_DbGetCodec(inst, payloadType) > 0)
    {
        return CODEC_DB_PAYLOAD_TAKEN;
    }

    /* Special case for CNG codecs */
    if (codec == kDecoderCNG)
    {
        /* check if this is first CNG codec to be registered */
        if (WebRtcNetEQ_DbGetPayload(inst, codec) == CODEC_DB_NOT_EXIST2)
        {
            /* no other CNG codec found */
            insertCNGcodec = 1;
        }

        /* find the appropriate insert position in CNG payload vector */
        switch (codec_fs)
        {
#ifdef NETEQ_WIDEBAND
            case 16000:
            CNGpos = 1;
            break;
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
            case 32000:
            CNGpos = 2;
            break;
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
            case 48000:
            CNGpos = 3;
            break;
#endif
            default: /* 8000 Hz case */
                CNGpos = 0;
                /*
                 * The 8 kHz CNG payload type is the one associated with the regular codec DB
                 * should override any other setting.
                 * Overwrite if this isn't the first CNG
                 */
                overwriteCNGcodec = !insertCNGcodec;
                break;
        }

        /* insert CNG payload type */
        inst->CNGpayloadType[CNGpos] = payloadType;

    }

    if ((codec != kDecoderCNG) || (insertCNGcodec == 1) || (overwriteCNGcodec == 1))
    {
        /* Check if we have reached the maximum numbers of simultaneous codecs */
        if (inst->nrOfCodecs == NUM_CODECS) return CODEC_DB_FULL;

        /* Check that codec has not already been initialized to DB =>
         remove it and reinitialize according to new spec */
        if ((inst->position[codec] != -1) && (overwriteCNGcodec != 1))
        { /* if registering multiple CNG codecs, don't remove, just overwrite */
            WebRtcNetEQ_DbRemove(inst, codec);
        }

        if (overwriteCNGcodec == 1)
        {
            temp = inst->position[codec];
        }
        else
        {
            temp = inst->nrOfCodecs; /* Store this codecs position */
            inst->position[codec] = temp;
            inst->nrOfCodecs++;
        }

        inst->payloadType[temp] = payloadType;

        /* Copy to database */
        inst->codec_state[temp] = codec_state;
        inst->funcDecode[temp] = funcDecode;
        inst->funcDecodeRCU[temp] = funcDecodeRCU;
        inst->funcAddLatePkt[temp] = funcAddLatePkt;
        inst->funcDecodeInit[temp] = funcDecodeInit;
        inst->funcDecodePLC[temp] = funcDecodePLC;
        inst->funcGetMDinfo[temp] = funcGetMDinfo;
        inst->funcGetPitch[temp] = funcGetPitch;
        inst->funcUpdBWEst[temp] = funcUpdBWEst;
        inst->funcGetErrorCode[temp] = funcGetErrorCode;
        inst->codec_fs[temp] = codec_fs;

    }

    return 0;
}

/*
 * Removes a codec from the database.
 */

int WebRtcNetEQ_DbRemove(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codec)
{
    int i;
    int pos = -1;

#ifndef NETEQ_RED_CODEC
    if (codec == kDecoderRED)
    {
        return CODEC_DB_UNSUPPORTED_CODEC;
    }
#endif
    if (((int) codec <= (int) kDecoderReservedStart) || ((int) codec
        >= (int) kDecoderReservedEnd))
    {
        return CODEC_DB_UNSUPPORTED_CODEC;
    }

    pos = inst->position[codec];
    if (pos == -1)
    {
        return CODEC_DB_NOT_EXIST4;
    }
    else
    {
        /* Remove this codec */
        inst->position[codec] = -1;
        for (i = pos; i < (inst->nrOfCodecs - 1); i++)
        {
            inst->payloadType[i] = inst->payloadType[i + 1];
            inst->codec_state[i] = inst->codec_state[i + 1];
            inst->funcDecode[i] = inst->funcDecode[i + 1];
            inst->funcDecodeRCU[i] = inst->funcDecodeRCU[i + 1];
            inst->funcAddLatePkt[i] = inst->funcAddLatePkt[i + 1];
            inst->funcDecodeInit[i] = inst->funcDecodeInit[i + 1];
            inst->funcDecodePLC[i] = inst->funcDecodePLC[i + 1];
            inst->funcGetMDinfo[i] = inst->funcGetMDinfo[i + 1];
            inst->funcGetPitch[i] = inst->funcGetPitch[i + 1];
            inst->funcUpdBWEst[i] = inst->funcUpdBWEst[i + 1];
            inst->funcGetErrorCode[i] = inst->funcGetErrorCode[i + 1];
            inst->codec_fs[i] = inst->codec_fs[i + 1];
        }
        inst->payloadType[i] = -1;
        inst->codec_state[i] = NULL;
        inst->funcDecode[i] = NULL;
        inst->funcDecodeRCU[i] = NULL;
        inst->funcAddLatePkt[i] = NULL;
        inst->funcDecodeInit[i] = NULL;
        inst->funcDecodePLC[i] = NULL;
        inst->funcGetMDinfo[i] = NULL;
        inst->funcGetPitch[i] = NULL;
        inst->funcUpdBWEst[i] = NULL;
        inst->funcGetErrorCode[i] = NULL;
        inst->codec_fs[i] = 0;
        /* Move down all the codecs above this one */
        for (i = 0; i < NUM_TOTAL_CODECS; i++)
        {
            if (inst->position[i] >= pos)
            {
                inst->position[i] = inst->position[i] - 1;
            }
        }
        inst->nrOfCodecs--;

        if (codec == kDecoderCNG)
        {
            /* also remove all registered CNG payload types */
            for (i = 0; i < NUM_CNG_CODECS; i++)
            {
                inst->CNGpayloadType[i] = -1;
            }
        }
    }
    return 0;
}

/*
 * Get the decoder function pointers for a codec.
 */

int WebRtcNetEQ_DbGetPtrs(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codec,
                          CodecFuncInst_t *ptr_inst)
{

    int pos = inst->position[codec];
    if ((codec <= kDecoderReservedStart) || (codec >= kDecoderReservedEnd) || (codec
        > NUM_TOTAL_CODECS))
    {
        /* ERROR */
        pos = -1;
    }
    if (pos >= 0)
    {
        ptr_inst->codec_state = inst->codec_state[pos];
        ptr_inst->funcAddLatePkt = inst->funcAddLatePkt[pos];
        ptr_inst->funcDecode = inst->funcDecode[pos];
        ptr_inst->funcDecodeRCU = inst->funcDecodeRCU[pos];
        ptr_inst->funcDecodeInit = inst->funcDecodeInit[pos];
        ptr_inst->funcDecodePLC = inst->funcDecodePLC[pos];
        ptr_inst->funcGetMDinfo = inst->funcGetMDinfo[pos];
        ptr_inst->funcUpdBWEst = inst->funcUpdBWEst[pos];
        ptr_inst->funcGetErrorCode = inst->funcGetErrorCode[pos];
        ptr_inst->codec_fs = inst->codec_fs[pos];
        return 0;
    }
    else
    {
        WebRtcSpl_MemSetW16((WebRtc_Word16*) ptr_inst, 0,
            sizeof(CodecFuncInst_t) / sizeof(WebRtc_Word16));
        return CODEC_DB_NOT_EXIST1;
    }
}

/*
 * Returns payload number given a codec identifier.
 */

int WebRtcNetEQ_DbGetPayload(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codecID)
{
    if (inst->position[codecID] == -1)
        return CODEC_DB_NOT_EXIST2;
    else
        return (inst->payloadType[inst->position[codecID]]);

}

/*
 * Returns codec identifier given a payload number.
 * Returns -1 if the payload type does not exist.
 */

int WebRtcNetEQ_DbGetCodec(CodecDbInst_t *inst, int payloadType)
{
    int i, pos;

    for (i = 0; i < NUM_TOTAL_CODECS; i++)
    {
        pos = inst->position[i];
        if (pos != -1)
        {
            if (inst->payloadType[pos] == payloadType) return i;
        }
    }

    /* did not find payload type */
    /* check if it's a CNG codec */
    if (WebRtcNetEQ_DbIsCNGPayload(inst, payloadType))
    {
        return kDecoderCNG;
    }

    /* found no match */
    return CODEC_DB_NOT_EXIST3;
}

/*
 * Extracts the Payload Split information of the codec with the specified payloadType.
 */

int WebRtcNetEQ_DbGetSplitInfo(SplitInfo_t *inst, enum WebRtcNetEQDecoder codecID,
                               int codedsize)
{

    switch (codecID)
    {
#ifdef NETEQ_ISAC_CODEC
        case kDecoderISAC:
#endif
#ifdef NETEQ_ISAC_SWB_CODEC
        case kDecoderISACswb:
#endif
#ifdef NETEQ_ARBITRARY_CODEC
        case kDecoderArbitrary:
#endif
#ifdef NETEQ_AMR_CODEC
        case kDecoderAMR:
#endif
#ifdef NETEQ_AMRWB_CODEC
        case kDecoderAMRWB:
#endif
#ifdef NETEQ_G726_CODEC
            /* Treat G726 as non-splittable to simplify the implementation */
        case kDecoderG726_16:
        case kDecoderG726_24:
        case kDecoderG726_32:
        case kDecoderG726_40:
#endif
#ifdef NETEQ_SPEEX_CODEC
        case kDecoderSPEEX_8:
        case kDecoderSPEEX_16:
#endif
#ifdef NETEQ_CELT_CODEC
        case kDecoderCELT_32 :
#endif
#ifdef NETEQ_G729_1_CODEC
        case kDecoderG729_1:
#endif
        {
            /* These codecs' payloads are not splittable */
            inst->deltaBytes = NO_SPLIT;
            return 0;
        }

            /*
             * Sample based coders are a special case.
             * In this case, deltaTime signals the number of bytes per timestamp unit times 2
             * in log2 domain.
             */
#if (defined NETEQ_G711_CODEC)
        case kDecoderPCMu:
        case kDecoderPCMa:
        {
            inst->deltaBytes = -12;
            inst->deltaTime = 1;
            return 0;
        }
#endif
#if (defined NETEQ_G722_CODEC)
        case kDecoderG722:
        {
            inst->deltaBytes = -14;
            inst->deltaTime = 0;
            return 0;
        }
#endif
#if (defined NETEQ_PCM16B_CODEC)
        case kDecoderPCM16B:
        {
            inst->deltaBytes = -12;
            inst->deltaTime = 2;
            return 0;
        }
#endif
#if ((defined NETEQ_PCM16B_CODEC)&&(defined NETEQ_WIDEBAND))
        case kDecoderPCM16Bwb:
        {
            inst->deltaBytes = -14;
            inst->deltaTime = 2;
            return 0;
        }
#endif
#if ((defined NETEQ_PCM16B_CODEC)&&(defined NETEQ_32KHZ_WIDEBAND))
        case kDecoderPCM16Bswb32kHz:
        {
            inst->deltaBytes = -18;
            inst->deltaTime = 2;
            return 0;
        }
#endif
#if ((defined NETEQ_PCM16B_CODEC)&&(defined NETEQ_48KHZ_WIDEBAND))
        case kDecoderPCM16Bswb48kHz:
        {
            inst->deltaBytes = -22;
            inst->deltaTime = 2;
            return 0;
        }
#endif

            /* Splittable payloads */
#ifdef NETEQ_G722_1_CODEC
        case kDecoderG722_1_16:
        {
            inst->deltaBytes = 40;
            inst->deltaTime = 320;
            return 0;
        }
        case kDecoderG722_1_24:
        {
            inst->deltaBytes = 60;
            inst->deltaTime = 320;
            return 0;
        }
        case kDecoderG722_1_32:
        {
            inst->deltaBytes = 80;
            inst->deltaTime = 320;
            return 0;
        }
#endif
#ifdef NETEQ_G722_1C_CODEC
        case kDecoderG722_1C_24:
        {
            inst->deltaBytes = 60;
            inst->deltaTime = 640;
            return 0;
        }
        case kDecoderG722_1C_32:
        {
            inst->deltaBytes = 80;
            inst->deltaTime = 640;
            return 0;
        }
        case kDecoderG722_1C_48:
        {
            inst->deltaBytes = 120;
            inst->deltaTime = 640;
            return 0;
        }
#endif
#ifdef NETEQ_G729_CODEC
        case kDecoderG729:
        {
            inst->deltaBytes = 10;
            inst->deltaTime = 80;
            return 0;
        }
#endif
#ifdef NETEQ_ILBC_CODEC
        case kDecoderILBC:
        {
            /* Check for splitting of iLBC packets.
             * If payload size is a multiple of 50 bytes it should be split into 30ms frames.
             * If payload size is a multiple of 38 bytes it should be split into 20ms frames.
             * Least common multiplier between 38 and 50 is 950, so the payload size must be less than
             * 950 bytes in order to resolve the frames unambiguously.
             * Currently max 12 frames in one bundle.
             */
            switch (codedsize)
            {
                case 50:
                case 100:
                case 150:
                case 200:
                case 250:
                case 300:
                case 350:
                case 400:
                case 450:
                case 500:
                case 550:
                case 600:
                {
                    inst->deltaBytes = 50;
                    inst->deltaTime = 240;
                    break;
                }
                case 38:
                case 76:
                case 114:
                case 152:
                case 190:
                case 228:
                case 266:
                case 304:
                case 342:
                case 380:
                case 418:
                case 456:
                {
                    inst->deltaBytes = 38;
                    inst->deltaTime = 160;
                    break;
                }
                default:
                {
                    return AMBIGUOUS_ILBC_FRAME_SIZE; /* Something not supported... */
                }
            }
            return 0;
        }
#endif
#ifdef NETEQ_GSMFR_CODEC
        case kDecoderGSMFR:
        {
            inst->deltaBytes = 33;
            inst->deltaTime = 160;
            return 0;
        }
#endif
        default:
        { /*Unknown codec */
            inst->deltaBytes = NO_SPLIT;
            return CODEC_DB_UNKNOWN_CODEC;
        }
    } /* end of switch */
}

/*
 * Returns 1 if codec is multiple description, 0 otherwise.
 * NOTE: This function is a stub, since there currently are no MD codecs.
 */
int WebRtcNetEQ_DbIsMDCodec(enum WebRtcNetEQDecoder codecID)
{
    if (0) /* Add test for MD codecs here */
        return 1;
    else
        return 0;
}

/*
 * Returns 1 if payload type is registered as a CNG codec, 0 otherwise
 */
int WebRtcNetEQ_DbIsCNGPayload(CodecDbInst_t *inst, int payloadType)
{
#ifdef NETEQ_CNG_CODEC
    int i;

    for(i=0; i<NUM_CNG_CODECS; i++)
    {
        if( (inst->CNGpayloadType[i] != -1) && (inst->CNGpayloadType[i] == payloadType) )
        {
            return 1;
        }
    }
#endif

    return 0;

}

/*
 * Return the sample rate for the codec with the given payload type, 0 if error
 */
WebRtc_UWord16 WebRtcNetEQ_DbGetSampleRate(CodecDbInst_t *inst, int payloadType)
{
    int i;
    CodecFuncInst_t codecInst;

    /* Sanity */
    if (inst == NULL)
    {
        /* return 0 Hz */
        return 0;
    }

    /* Check among CNG payloads */
    for (i = 0; i < NUM_CNG_CODECS; i++)
    {
        if ((inst->CNGpayloadType[i] != -1) && (inst->CNGpayloadType[i] == payloadType))
        {
            switch (i)
            {
                case 1:
                    return 16000;
                case 2:
                    return 32000;
                case 3:
                    return 48000;
                default:
                    return 8000;
            }
        }
    }

    /* Not a CNG payload, check the other payloads */
    i = WebRtcNetEQ_DbGetCodec(inst, payloadType);
    if (i >= 0)
    {
        if (WebRtcNetEQ_DbGetPtrs(inst, (enum WebRtcNetEQDecoder) i, &codecInst) != 0)
        {
            /* Unexpected error, return 0 Hz */
            return 0;
        }
        return codecInst.codec_fs;
    }

    /* If we end up here, we got an error, return 0 Hz */
    return 0;

}

