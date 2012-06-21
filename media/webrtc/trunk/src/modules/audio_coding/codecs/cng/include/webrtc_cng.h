/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_MAIN_INTERFACE_WEBRTC_CNG_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_MAIN_INTERFACE_WEBRTC_CNG_H_

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEBRTC_CNG_MAX_LPC_ORDER 12
#define WEBRTC_CNG_MAX_OUTSIZE_ORDER 640

/* Define Error codes */

/* 6100 Encoder */
#define CNG_ENCODER_MEMORY_ALLOCATION_FAILED    6110
#define CNG_ENCODER_NOT_INITIATED               6120
#define CNG_DISALLOWED_LPC_ORDER                6130
#define CNG_DISALLOWED_FRAME_SIZE               6140
#define CNG_DISALLOWED_SAMPLING_FREQUENCY       6150
/* 6200 Decoder */
#define CNG_DECODER_MEMORY_ALLOCATION_FAILED    6210
#define CNG_DECODER_NOT_INITIATED               6220


typedef struct WebRtcCngEncInst         CNG_enc_inst;
typedef struct WebRtcCngDecInst         CNG_dec_inst;


/****************************************************************************
 * WebRtcCng_Version(...)
 *
 * These functions returns the version name (string must be at least
 * 500 characters long)
 *
 * Output:
 *    - version    : Pointer to character string
 *
 * Return value    :  0 - Ok
 *                   -1 - Error
 */

WebRtc_Word16 WebRtcCng_Version(char *version);

/****************************************************************************
 * WebRtcCng_AssignSizeEnc/Dec(...)
 *
 * These functions get the size needed for storing the instance for encoder
 * and decoder, respectively
 *
 * Input/Output:
 *    - sizeinbytes     : Pointer to integer where the size is returned
 *
 * Return value         :  0
 */

WebRtc_Word16 WebRtcCng_AssignSizeEnc(int *sizeinbytes);
WebRtc_Word16 WebRtcCng_AssignSizeDec(int *sizeinbytes);


/****************************************************************************
 * WebRtcCng_AssignEnc/Dec(...)
 *
 * These functions Assignes memory for the instances.
 *
 * Input:
 *    - CNG_inst_Addr :  Adress to where to assign memory
 * Output:
 *    - inst          :  Pointer to the instance that should be created
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */

WebRtc_Word16 WebRtcCng_AssignEnc(CNG_enc_inst **inst, void *CNG_inst_Addr);
WebRtc_Word16 WebRtcCng_AssignDec(CNG_dec_inst **inst, void *CNG_inst_Addr);


/****************************************************************************
 * WebRtcCng_CreateEnc/Dec(...)
 *
 * These functions create an instance to the specified structure
 *
 * Input:
 *    - XXX_inst      : Pointer to created instance that should be created
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */

WebRtc_Word16 WebRtcCng_CreateEnc(CNG_enc_inst **cng_inst);
WebRtc_Word16 WebRtcCng_CreateDec(CNG_dec_inst **cng_inst);


/****************************************************************************
 * WebRtcCng_InitEnc/Dec(...)
 *
 * This function initializes a instance
 *
 * Input:
 *    - cng_inst      : Instance that should be initialized
 *
 *    - fs            : 8000 for narrowband and 16000 for wideband
 *    - interval      : generate SID data every interval ms
 *    - quality       : Number of refl. coefs, maximum allowed is 12
 *
 * Output:
 *    - cng_inst      : Initialized instance
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */

WebRtc_Word16 WebRtcCng_InitEnc(CNG_enc_inst *cng_inst,
                                WebRtc_Word16 fs,
                                WebRtc_Word16 interval,
                                WebRtc_Word16 quality);
WebRtc_Word16 WebRtcCng_InitDec(CNG_dec_inst *cng_dec_inst);

 
/****************************************************************************
 * WebRtcCng_FreeEnc/Dec(...)
 *
 * These functions frees the dynamic memory of a specified instance
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */


WebRtc_Word16 WebRtcCng_FreeEnc(CNG_enc_inst *cng_inst);
WebRtc_Word16 WebRtcCng_FreeDec(CNG_dec_inst *cng_inst);



/****************************************************************************
 * WebRtcCng_Encode(...)
 *
 * These functions analyzes background noise
 *
 * Input:
 *    - cng_inst      : Pointer to created instance
 *    - speech        : Signal to be analyzed
 *    - nrOfSamples   : Size of speech vector
 *    - forceSID      : not zero to force SID frame and reset
 *
 * Output:
 *    - bytesOut      : Nr of bytes to transmit, might be 0
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */

WebRtc_Word16 WebRtcCng_Encode(CNG_enc_inst *cng_inst,
                               WebRtc_Word16 *speech,
                               WebRtc_Word16 nrOfSamples,
                               WebRtc_UWord8* SIDdata,
                               WebRtc_Word16 *bytesOut,
                               WebRtc_Word16 forceSID);


/****************************************************************************
 * WebRtcCng_UpdateSid(...)
 *
 * These functions updates the CN state, when a new SID packet arrives
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *    - SID           : SID packet, all headers removed
 *    - length        : Length in bytes of SID packet
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */
WebRtc_Word16 WebRtcCng_UpdateSid(CNG_dec_inst *cng_inst,
                                  WebRtc_UWord8 *SID,
                                  WebRtc_Word16 length);


/****************************************************************************
 * WebRtcCng_Generate(...)
 *
 * These functions generates CN data when needed
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *    - outData       : pointer to area to write CN data
 *    - nrOfSamples   : How much data to generate
 *    - new_period    : >0 if a new period of CNG, will reset history
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */
WebRtc_Word16 WebRtcCng_Generate(CNG_dec_inst *cng_inst,
                                 WebRtc_Word16 * outData,
                                 WebRtc_Word16 nrOfSamples,
                                 WebRtc_Word16 new_period);


/*****************************************************************************
 * WebRtcCng_GetErrorCodeEnc/Dec(...)
 *
 * This functions can be used to check the error code of a CNG instance. When
 * a function returns -1 a error code will be set for that instance. The 
 * function below extract the code of the last error that occurred in the
 * specified instance.
 *
 * Input:
 *    - CNG_inst    : CNG enc/dec instance
 *
 * Return value     : Error code
 */

WebRtc_Word16 WebRtcCng_GetErrorCodeEnc(CNG_enc_inst *cng_inst);
WebRtc_Word16 WebRtcCng_GetErrorCodeDec(CNG_dec_inst *cng_inst);


#ifdef __cplusplus
}
#endif

#endif // WEBRTC_MODULES_AUDIO_CODING_CODECS_CNG_MAIN_INTERFACE_WEBRTC_CNG_H_
