/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "NETEQTEST_CodecClass.h"

#include <stdlib.h>  // exit

#include "webrtc_neteq_help_macros.h"

NETEQTEST_Decoder::NETEQTEST_Decoder(enum WebRtcNetEQDecoder type, WebRtc_UWord16 fs, const char * name, WebRtc_UWord8 pt)
:
_decoder(NULL),
_decoderType(type),
_pt(pt),
_fs(fs),
_name(name)
{
}

int NETEQTEST_Decoder::loadToNetEQ(NETEQTEST_NetEQClass & neteq, WebRtcNetEQ_CodecDef & codecInst)
{
    SET_CODEC_PAR(codecInst, _decoderType, _pt, _decoder, _fs);
    int err = neteq.loadCodec(codecInst);
    
    if (err)
    {
        printf("Error loading codec %s into NetEQ database\n", _name.c_str());
    }

    return(err);
}


// iSAC
#ifdef CODEC_ISAC
#include "isac.h"

decoder_iSAC::decoder_iSAC(WebRtc_UWord8 pt) 
:
NETEQTEST_Decoder(kDecoderISAC, 16000, "iSAC", pt)
{
    WebRtc_Word16 err = WebRtcIsac_Create((ISACStruct **) &_decoder);
    if (err)
    {
        exit(EXIT_FAILURE);
    }

    WebRtcIsac_EncoderInit((ISACStruct *) _decoder, 0);
    WebRtcIsac_SetDecSampRate((ISACStruct *) _decoder, kIsacWideband);
}


decoder_iSAC::~decoder_iSAC()
{
    if (_decoder)
    {
        WebRtcIsac_Free((ISACStruct *) _decoder);
        _decoder = NULL;
    }
}


int decoder_iSAC::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_ISAC_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));

}
#endif

#ifdef CODEC_ISAC_SWB
decoder_iSACSWB::decoder_iSACSWB(WebRtc_UWord8 pt) 
:
NETEQTEST_Decoder(kDecoderISACswb, 32000, "iSAC swb", pt)
{
    WebRtc_Word16 err = WebRtcIsac_Create((ISACStruct **) &_decoder);
    if (err)
    {
        exit(EXIT_FAILURE);
    }

    WebRtcIsac_EncoderInit((ISACStruct *) _decoder, 0);
    WebRtcIsac_SetDecSampRate((ISACStruct *) _decoder, kIsacSuperWideband);
}

decoder_iSACSWB::~decoder_iSACSWB()
{
    if (_decoder)
    {
        WebRtcIsac_Free((ISACStruct *) _decoder);
        _decoder = NULL;
    }
}

int decoder_iSACSWB::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_ISACSWB_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));

}
#endif

// PCM u/A
#ifdef CODEC_G711
#include "g711_interface.h"

decoder_PCMU::decoder_PCMU(WebRtc_UWord8 pt) 
:
NETEQTEST_Decoder(kDecoderPCMu, 8000, "G.711-u", pt)
{
    // no state to crate or init
}

int decoder_PCMU::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCMU_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));

}

decoder_PCMA::decoder_PCMA(WebRtc_UWord8 pt) 
:
NETEQTEST_Decoder(kDecoderPCMa, 8000, "G.711-A", pt)
{
    // no state to crate or init
}

int decoder_PCMA::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCMA_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

// Linear PCM16b
#if (defined(CODEC_PCM16B) || defined(CODEC_PCM16B_WB) || \
    defined(CODEC_PCM16B_32KHZ) || defined(CODEC_PCM16B_48KHZ))
#include "pcm16b.h"
#endif

#ifdef CODEC_PCM16B
int decoder_PCM16B_NB::loadToNetEQ(NETEQTEST_NetEQClass &neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCM16B_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_PCM16B_WB
int decoder_PCM16B_WB::loadToNetEQ(NETEQTEST_NetEQClass &neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCM16B_WB_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_PCM16B_32KHZ
int decoder_PCM16B_SWB32::loadToNetEQ(NETEQTEST_NetEQClass &neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCM16B_SWB32_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_PCM16B_48KHZ
int decoder_PCM16B_SWB48::loadToNetEQ(NETEQTEST_NetEQClass &neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_PCM16B_SWB48_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_ILBC
#include "ilbc.h"
decoder_ILBC::decoder_ILBC(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderILBC, 8000, "iLBC", pt)
{
    WebRtc_Word16 err = WebRtcIlbcfix_DecoderCreate((iLBC_decinst_t **) &_decoder);
    if (err)
    {
        exit(EXIT_FAILURE);
    }
}

decoder_ILBC::~decoder_ILBC()
{
    WebRtcIlbcfix_DecoderFree((iLBC_decinst_t *) _decoder);
}

int decoder_ILBC::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_ILBC_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G729
#include "G729Interface.h"
decoder_G729::decoder_G729(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG729, 8000, "G.729", pt)
{
    WebRtc_Word16 err = WebRtcG729_CreateDec((G729_decinst_t **) &_decoder);
    if (err)
    {
        exit(EXIT_FAILURE);
    }
}

decoder_G729::~decoder_G729()
{
    WebRtcG729_FreeDec((G729_decinst_t *) _decoder);
}

int decoder_G729::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G729_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G729_1
#include "G729_1Interface.h"
decoder_G729_1::decoder_G729_1(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG729_1, 16000, "G.729.1", pt)
{
    WebRtc_Word16 err = WebRtcG7291_Create((G729_1_inst_t **) &_decoder);
    if (err)
    {
        exit(EXIT_FAILURE);
    }
}

decoder_G729_1::~decoder_G729_1()
{
    WebRtcG7291_Free((G729_1_inst_t *) _decoder);
}

int decoder_G729_1::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G729_1_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722
#include "g722_interface.h"
decoder_G722::decoder_G722(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722, 16000, "G.722", pt)
{
     WebRtc_Word16 err = WebRtcG722_CreateDecoder((G722DecInst **) &_decoder);
     if (err)
     {
         exit(EXIT_FAILURE);
     }
}

decoder_G722::~decoder_G722()
{
    WebRtcG722_FreeDecoder((G722DecInst *) _decoder);
}

int decoder_G722::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#if (defined(CODEC_G722_1_16) || defined(CODEC_G722_1_24) || \
    defined(CODEC_G722_1_32) || defined(CODEC_G722_1C_24) || \
    defined(CODEC_G722_1C_32) || defined(CODEC_G722_1C_48))
#include "G722_1Interface.h"
#endif

#ifdef CODEC_G722_1_16
decoder_G722_1_16::decoder_G722_1_16(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1_16, 16000, "G.722.1 (16 kbps)", pt)
{
    if (WebRtcG7221_CreateDec16((G722_1_16_decinst_t **) &_decoder))
    {
        exit(EXIT_FAILURE);
    }
}

decoder_G722_1_16::~decoder_G722_1_16()
{
    WebRtcG7221_FreeDec16((G722_1_16_decinst_t *) _decoder);
}

int decoder_G722_1_16::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1_16_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722_1_24
decoder_G722_1_24::decoder_G722_1_24(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1_24, 16000, "G.722.1 (24 kbps)", pt)
{
    if (WebRtcG7221_CreateDec24((G722_1_24_decinst_t **) &_decoder))
    {
        exit(EXIT_FAILURE);
    }
}

decoder_G722_1_24::~decoder_G722_1_24()
{
    WebRtcG7221_FreeDec24((G722_1_24_decinst_t *) _decoder);
}

int decoder_G722_1_24::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1_24_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722_1_32
decoder_G722_1_32::decoder_G722_1_32(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1_32, 16000, "G.722.1 (32 kbps)", pt)
{
    if (WebRtcG7221_CreateDec32((G722_1_32_decinst_t **) &_decoder))
    {
        exit(EXIT_FAILURE);
    }
}

decoder_G722_1_32::~decoder_G722_1_32()
{
    WebRtcG7221_FreeDec32((G722_1_32_decinst_t *) _decoder);
}

int decoder_G722_1_32::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1_32_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722_1C_24
decoder_G722_1C_24::decoder_G722_1C_24(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1C_24, 32000, "G.722.1C (24 kbps)", pt)
{
    if (WebRtcG7221C_CreateDec24((G722_1C_24_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);
}

decoder_G722_1C_24::~decoder_G722_1C_24()
{
    WebRtcG7221C_FreeDec24((G722_1C_24_decinst_t *) _decoder);
}

int decoder_G722_1C_24::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1C_24_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722_1C_32
decoder_G722_1C_32::decoder_G722_1C_32(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1C_32, 32000, "G.722.1C (32 kbps)", pt)
{
    if (WebRtcG7221C_CreateDec32((G722_1C_32_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);
}

decoder_G722_1C_32::~decoder_G722_1C_32()
{
    WebRtcG7221C_FreeDec32((G722_1C_32_decinst_t *) _decoder);
}

int decoder_G722_1C_32::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1C_32_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_G722_1C_48
decoder_G722_1C_48::decoder_G722_1C_48(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderG722_1C_48, 32000, "G.722.1C (48 kbps)", pt)
{
    if (WebRtcG7221C_CreateDec48((G722_1C_48_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);
}

decoder_G722_1C_48::~decoder_G722_1C_48()
{
    WebRtcG7221C_FreeDec48((G722_1C_48_decinst_t *) _decoder);
}

int decoder_G722_1C_48::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_G722_1C_48_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_AMR
#include "AMRInterface.h"
#include "AMRCreation.h"
decoder_AMR::decoder_AMR(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderAMR, 8000, "AMR", pt)
{
    if (WebRtcAmr_CreateDec((AMR_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);

    WebRtcAmr_DecodeBitmode((AMR_decinst_t *) _decoder, AMRBandwidthEfficient);
}

decoder_AMR::~decoder_AMR()
{
    WebRtcAmr_FreeDec((AMR_decinst_t *) _decoder);
}

int decoder_AMR::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_AMR_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_AMRWB
#include "AMRWBInterface.h"
#include "AMRWBCreation.h"
decoder_AMRWB::decoder_AMRWB(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderAMRWB, 16000, "AMR wb", pt)
{
    if (WebRtcAmrWb_CreateDec((AMRWB_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);

    WebRtcAmrWb_DecodeBitmode((AMRWB_decinst_t *) _decoder, AMRBandwidthEfficient);
}

decoder_AMRWB::~decoder_AMRWB()
{
    WebRtcAmrWb_FreeDec((AMRWB_decinst_t *) _decoder);
}

int decoder_AMRWB::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_AMRWB_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_GSMFR
#include "GSMFRInterface.h"
#include "GSMFRCreation.h"
decoder_GSMFR::decoder_GSMFR(WebRtc_UWord8 pt)
:
NETEQTEST_Decoder(kDecoderGSMFR, 8000, "GSM-FR", pt)
{
    if (WebRtcGSMFR_CreateDec((GSMFR_decinst_t **) &_decoder))
        exit(EXIT_FAILURE);
}

decoder_GSMFR::~decoder_GSMFR()
{
    WebRtcGSMFR_FreeDec((GSMFR_decinst_t *) _decoder);
}

int decoder_GSMFR::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_GSMFR_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#if (defined(CODEC_SPEEX_8) || defined (CODEC_SPEEX_16))
#include "SpeexInterface.h"
decoder_SPEEX::decoder_SPEEX(WebRtc_UWord8 pt, WebRtc_UWord16 fs)
:
NETEQTEST_Decoder(fs == 8000 ? kDecoderSPEEX_8 : kDecoderSPEEX_16, 
                  fs, "SPEEX", pt)
{
    if (fs != 8000 && fs != 16000)
        throw std::exception("Wrong sample rate for SPEEX");

    if (WebRtcSpeex_CreateDec((SPEEX_decinst_t **) &_decoder, fs, 1))
        exit(EXIT_FAILURE);
}

decoder_SPEEX::~decoder_SPEEX()
{
    WebRtcSpeex_FreeDec((SPEEX_decinst_t *) _decoder);
}

int decoder_SPEEX::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_SPEEX_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_CELT_32
#include "celt_interface.h"
decoder_CELT::decoder_CELT(WebRtc_UWord8 pt, WebRtc_UWord16 fs)
:
NETEQTEST_Decoder(kDecoderCELT_32, fs, "CELT", pt)
{
   if (WebRtcCelt_CreateDec((CELT_decinst_t **) &_decoder, 2))
        exit(EXIT_FAILURE);
}

decoder_CELT::~decoder_CELT()
{
    WebRtcCelt_FreeDec((CELT_decinst_t *) _decoder);
}

int decoder_CELT::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_CELT_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}

decoder_CELTslave::decoder_CELTslave(WebRtc_UWord8 pt, WebRtc_UWord16 fs)
:
NETEQTEST_Decoder(kDecoderCELT_32, fs, "CELT", pt)
{
   if (WebRtcCelt_CreateDec((CELT_decinst_t **) &_decoder, 2))
        exit(EXIT_FAILURE);
}

decoder_CELTslave::~decoder_CELTslave()
{
    WebRtcCelt_FreeDec((CELT_decinst_t *) _decoder);
}

int decoder_CELTslave::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_CELTSLAVE_FUNCTIONS(codecInst);
    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_RED
int decoder_RED::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_RED_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#ifdef CODEC_ATEVENT_DECODE
int decoder_AVT::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_AVT_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif

#if (defined(CODEC_CNGCODEC8) || defined(CODEC_CNGCODEC16) || \
    defined(CODEC_CNGCODEC32) || defined(CODEC_CNGCODEC48))
#include "webrtc_cng.h"
decoder_CNG::decoder_CNG(WebRtc_UWord8 pt, WebRtc_UWord16 fs)
:
NETEQTEST_Decoder(kDecoderCNG, fs, "CNG", pt)
{
    if (fs != 8000 && fs != 16000 && fs != 32000 && fs != 48000)
        exit(EXIT_FAILURE);

    if (WebRtcCng_CreateDec((CNG_dec_inst **) &_decoder))
        exit(EXIT_FAILURE);
}

decoder_CNG::~decoder_CNG()
{
    WebRtcCng_FreeDec((CNG_dec_inst *) _decoder);
}

int decoder_CNG::loadToNetEQ(NETEQTEST_NetEQClass & neteq)
{
    WebRtcNetEQ_CodecDef codecInst;

    SET_CNG_FUNCTIONS(codecInst);

    return(NETEQTEST_Decoder::loadToNetEQ(neteq, codecInst));
}
#endif
