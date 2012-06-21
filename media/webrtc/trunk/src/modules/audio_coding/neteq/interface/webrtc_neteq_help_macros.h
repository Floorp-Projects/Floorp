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
 * This file contains some helper macros that can be used when loading the
 * NetEQ codec database.
 */

#ifndef WEBRTC_NETEQ_HELP_MACROS_H
#define WEBRTC_NETEQ_HELP_MACROS_H

#ifndef NULL
#define NULL 0
#endif

/**********************************************************
 * Help macros for NetEQ initialization
 */

#define SET_CODEC_PAR(inst,decoder,pt,state,fs) \
                    inst.codec=decoder; \
                    inst.payloadType=pt; \
                    inst.codec_state=state; \
                    inst.codec_fs=fs;

#define SET_PCMU_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG711_DecodeU; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_PCMA_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG711_DecodeA; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_ILBC_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcIlbcfix_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcIlbcfix_NetEqPlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcIlbcfix_Decoderinit30Ms; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_ISAC_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcIsac_Decode; \
                    inst.funcDecodeRCU=(WebRtcNetEQ_FuncDecode)WebRtcIsac_DecodeRcu; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcIsac_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=(WebRtcNetEQ_FuncUpdBWEst)WebRtcIsac_UpdateBwEstimate; \
                    inst.funcGetErrorCode=(WebRtcNetEQ_FuncGetErrorCode)WebRtcIsac_GetErrorCode;

#define SET_ISACfix_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcIsacfix_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcIsacfix_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=(WebRtcNetEQ_FuncUpdBWEst)WebRtcIsacfix_UpdateBwEstimate; \
                    inst.funcGetErrorCode=(WebRtcNetEQ_FuncGetErrorCode)WebRtcIsacfix_GetErrorCode;

#define SET_ISACSWB_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcIsac_Decode; \
                    inst.funcDecodeRCU=(WebRtcNetEQ_FuncDecode)WebRtcIsac_DecodeRcu; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcIsac_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=(WebRtcNetEQ_FuncUpdBWEst)WebRtcIsac_UpdateBwEstimate; \
                    inst.funcGetErrorCode=(WebRtcNetEQ_FuncGetErrorCode)WebRtcIsac_GetErrorCode;

#define SET_G729_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG729_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG729_DecodePlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG729_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G729_1_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7291_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7291_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=(WebRtcNetEQ_FuncUpdBWEst)WebRtcG7291_DecodeBwe; \
                    inst.funcGetErrorCode=NULL;

#define SET_PCM16B_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcPcm16b_DecodeW16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_PCM16B_WB_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcPcm16b_DecodeW16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_PCM16B_SWB32_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcPcm16b_DecodeW16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;


#define SET_PCM16B_SWB48_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcPcm16b_DecodeW16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG722_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG722_DecoderInit;\
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1_16_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221_Decode16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221_DecodePlc16; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221_DecoderInit16; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1_24_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221_Decode24; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221_DecodePlc24; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221_DecoderInit24; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1_32_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221_Decode32; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221_DecodePlc32; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221_DecoderInit32; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1C_24_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221C_Decode24; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221C_DecodePlc24; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221C_DecoderInit24; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1C_32_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221C_Decode32; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221C_DecodePlc32; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221C_DecoderInit32; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G722_1C_48_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG7221C_Decode48; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcG7221C_DecodePlc48; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG7221C_DecoderInit48; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_AMR_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcAmr_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcAmr_DecodePlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcAmr_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_AMRWB_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcAmrWb_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcAmrWb_DecodePlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcAmrWb_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_GSMFR_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcGSMFR_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcGSMFR_DecodePlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcGSMFR_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G726_16_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG726_decode16; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG726_decoderinit16; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G726_24_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG726_decode24; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG726_decoderinit24; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G726_32_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG726_decode32; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG726_decoderinit32; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_G726_40_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcG726_decode40; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcG726_decoderinit40; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_SPEEX_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcSpeex_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=(WebRtcNetEQ_FuncDecodePLC)WebRtcSpeex_DecodePlc; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcSpeex_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_CELT_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcCelt_Decode; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcCelt_DecoderInit; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_CELTSLAVE_FUNCTIONS(inst) \
                    inst.funcDecode=(WebRtcNetEQ_FuncDecode)WebRtcCelt_DecodeSlave; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=(WebRtcNetEQ_FuncDecodeInit)WebRtcCelt_DecoderInitSlave; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_RED_FUNCTIONS(inst) \
                    inst.funcDecode=NULL; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_AVT_FUNCTIONS(inst) \
                    inst.funcDecode=NULL; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#define SET_CNG_FUNCTIONS(inst) \
                    inst.funcDecode=NULL; \
                    inst.funcDecodeRCU=NULL; \
                    inst.funcDecodePLC=NULL; \
                    inst.funcDecodeInit=NULL; \
                    inst.funcAddLatePkt=NULL; \
                    inst.funcGetMDinfo=NULL; \
                    inst.funcGetPitch=NULL; \
                    inst.funcUpdBWEst=NULL; \
                    inst.funcGetErrorCode=NULL;

#endif /* WEBRTC_NETEQ_HELP_MACROS_H */

