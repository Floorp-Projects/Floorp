/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ASN1_H
#define ASN1_H

#ifdef DEBUG
static const char ASN1_CVS_ID[] = "@(#) $RCSfile: asn1.h,v $ $Revision: 1.3 $ $Date: 2005/01/20 02:25:44 $";
#endif /* DEBUG */

/*
 * asn1.h
 *
 * This file contains the ASN.1 encoder/decoder routines available
 * internally within NSS.  It's not clear right now if this file
 * will be folded into base.h or something, I just needed to get this
 * going.  At the moment, most of these routines wrap the old SEC_ASN1
 * calls.
 */

#ifndef ASN1T_H
#include "asn1t.h"
#endif /* ASN1T_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

PR_BEGIN_EXTERN_C

/*
 * nssASN1Decoder
 *
 * ... description here ...
 *
 *  nssASN1Decoder_Create (Factory/Constructor)
 *  nssASN1Decoder_Update
 *  nssASN1Decoder_Finish (Destructor)
 *  nssASN1Decoder_SetFilter
 *  nssASN1Decoder_GetFilter
 *  nssASN1Decoder_SetNotify
 *  nssASN1Decoder_GetNotify
 *
 * Debug builds only:
 *
 *  nssASN1Decoder_verify
 *
 * Related functions that aren't type methods:
 *
 *  nssASN1_Decode
 *  nssASN1_DecodeBER
 */

/*
 * nssASN1Decoder_Create
 *
 * This routine creates an ASN.1 Decoder, which will use the specified
 * template to decode a datastream into the specified destination
 * structure.  If the optional arena argument is non-NULL, blah blah 
 * blah.  XXX fgmr Should we include an nssASN1EncodingType argument, 
 * as a hint?  Or is each encoding distinctive?  This routine may 
 * return NULL upon error, in which case an error will have been 
 * placed upon the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  ...
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an ASN.1 Decoder upon success.
 */

NSS_EXTERN nssASN1Decoder *
nssASN1Decoder_Create
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[]
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;

/*
 * nssASN1Decoder_Update
 *
 * This routine feeds data to the decoder.  In the event of an error, 
 * it will place an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_INVALID_ASN1DECODER
 *  NSS_ERROR_INVALID_BER
 *  ...
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success.
 */

NSS_EXTERN PRStatus
nssASN1Decoder_Update
(
  nssASN1Decoder *decoder,
  const void *data,
  PRUint32 amount
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;
extern const NSSError NSS_ERROR_INVALID_BER;

/*
 * nssASN1Decoder_Finish
 *
 * This routine finishes the decoding and destroys the decoder.
 * In the event of an error, it will place an error on the error
 * stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Decoder_Finish
(
  nssASN1Decoder *decoder
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;

/*
 * nssASN1Decoder_SetFilter
 *
 * This routine registers a callback filter routine with the decoder,
 * which will be called blah blah blah.  The specified argument will
 * be passed as-is to the filter routine.  The routine pointer may
 * be NULL, in which case no filter callback will be called.  If the
 * noStore boolean is PR_TRUE, then decoded fields will not be stored
 * in the destination structure specified when the decoder was 
 * created.  This routine returns a PRStatus value; in the event of
 * an error, it will place an error on the error stack and return
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Decoder_SetFilter
(
  nssASN1Decoder *decoder,
  nssASN1DecoderFilterFunction *callback,
  void *argument,
  PRBool noStore
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;

/*
 * nssASN1Decoder_GetFilter
 *
 * If the optional pCallbackOpt argument to this routine is non-null,
 * then the pointer to any callback function established for this
 * decoder with nssASN1Decoder_SetFilter will be stored at the 
 * location indicated by it.  If the optional pArgumentOpt
 * pointer is non-null, the filter's closure argument will be stored
 * there.  If the optional pNoStoreOpt pointer is non-null, the
 * noStore value specified when setting the filter will be stored
 * there.  This routine returns a PRStatus value; in the event of
 * an error it will place an error on the error stack and return
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Decoder_GetFilter
(
  nssASN1Decoder *decoder,
  nssASN1DecoderFilterFunction **pCallbackOpt,
  void **pArgumentOpt,
  PRBool *pNoStoreOpt
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;

/*
 * nssASN1Decoder_SetNotify
 *
 * This routine registers a callback notify routine with the decoder,
 * which will be called whenever.. The specified argument will be
 * passed as-is to the notify routine.  The routine pointer may be
 * NULL, in which case no notify routine will be called.  This routine
 * returns a PRStatus value; in the event of an error it will place
 * an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Decoder_SetNotify
(
  nssASN1Decoder *decoder,
  nssASN1NotifyFunction *callback,
  void *argument
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;

/*
 * nssASN1Decoder_GetNotify
 *
 * If the optional pCallbackOpt argument to this routine is non-null,
 * then the pointer to any callback function established for this
 * decoder with nssASN1Decoder_SetNotify will be stored at the 
 * location indicated by it.  If the optional pArgumentOpt pointer is
 * non-null, the filter's closure argument will be stored there.
 * This routine returns a PRStatus value; in the event of an error it
 * will place an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Decoder_GetNotify
(
  nssASN1Decoder *decoder,
  nssASN1NotifyFunction **pCallbackOpt,
  void **pArgumentOpt
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;

/*
 * nssASN1Decoder_verify
 *
 * This routine is only available in debug builds.
 *
 * If the specified pointer is a valid pointer to an nssASN1Decoder
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

#ifdef DEBUG
NSS_EXTERN PRStatus
nssASN1Decoder_verify
(
  nssASN1Decoder *decoder
);

extern const NSSError NSS_ERROR_INVALID_ASN1DECODER;
#endif /* DEBUG */

/*
 * nssASN1_Decode
 *
 * This routine will decode the specified data into the specified
 * destination structure, as specified by the specified template.
 * This routine returns a PRStatus value; in the event of an error
 * it will place an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_INVALID_BER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1_Decode
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[],
  const void *berData,
  PRUint32 amount
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;
extern const NSSError NSS_ERROR_INVALID_BER;

/*
 * nssASN1_DecodeBER
 *
 * This routine will decode the data in the specified NSSBER
 * into the destination structure, as specified by the template.
 * This routine returns a PRStatus value; in the event of an error
 * it will place an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_INVALID_NSSBER
 *  NSS_ERROR_INVALID_BER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1_DecodeBER
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[],
  const NSSBER *data
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;
extern const NSSError NSS_ERROR_INVALID_BER;

/*
 * nssASN1Encoder
 *
 * ... description here ...
 *
 *  nssASN1Encoder_Create (Factory/Constructor)
 *  nssASN1Encoder_Update
 *  nssASN1Encoder_Finish (Destructor)
 *  nssASN1Encoder_SetNotify
 *  nssASN1Encoder_GetNotify
 *  nssASN1Encoder_SetStreaming
 *  nssASN1Encoder_GetStreaming
 *  nssASN1Encoder_SetTakeFromBuffer
 *  nssASN1Encoder_GetTakeFromBuffer
 *
 * Debug builds only:
 *
 *  nssASN1Encoder_verify
 *
 * Related functions that aren't type methods:
 *
 *  nssASN1_Encode
 *  nssASN1_EncodeItem
 */

/*
 * nssASN1Encoder_Create
 *
 * This routine creates an ASN.1 Encoder, blah blah blah.  This 
 * may return NULL upon error, in which case an error will have been
 * placed on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_ENCODING_NOT_SUPPORTED
 *  ...
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an ASN.1 Encoder upon success
 */

NSS_EXTERN nssASN1Encoder *
nssASN1Encoder_Create
(
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding,
  nssASN1EncoderWriteFunction *sink,
  void *argument
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;
extern const NSSError NSS_ERROR_ENCODING_NOT_SUPPORTED;

/*
 * nssASN1Encoder_Update
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_Update
(
  nssASN1Encoder *encoder,
  const void *data,
  PRUint32 length
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;
extern const NSSError NSS_ERROR_INVALID_POINTER;

/*
 * nssASN1Encoder_Finish
 *
 * Destructor.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_Finish
(
  nssASN1Encoder *encoder
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;

/*
 * nssASN1Encoder_SetNotify
 *
 * This routine registers a callback notify routine with the encoder,
 * which will be called whenever.. The specified argument will be
 * passed as-is to the notify routine.  The routine pointer may be
 * NULL, in which case no notify routine will be called.  This routine
 * returns a PRStatus value; in the event of an error it will place
 * an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1DECODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_SetNotify
(
  nssASN1Encoder *encoder,
  nssASN1NotifyFunction *callback,
  void *argument
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;

/*
 * nssASN1Encoder_GetNotify
 *
 * If the optional pCallbackOpt argument to this routine is non-null,
 * then the pointer to any callback function established for this
 * decoder with nssASN1Encoder_SetNotify will be stored at the 
 * location indicated by it.  If the optional pArgumentOpt pointer is
 * non-null, the filter's closure argument will be stored there.
 * This routine returns a PRStatus value; in the event of an error it
 * will place an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_GetNotify
(
  nssASN1Encoder *encoder,
  nssASN1NotifyFunction **pCallbackOpt,
  void **pArgumentOpt
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;

/*
 * nssASN1Encoder_SetStreaming
 *
 * 
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_SetStreaming
(
  nssASN1Encoder *encoder,
  PRBool streaming
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;

/*
 * nssASN1Encoder_GetStreaming
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_GetStreaming
(
  nssASN1Encoder *encoder,
  PRBool *pStreaming
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;
extern const NSSError NSS_ERROR_INVALID_POINTER;

/*
 * nssASN1Encoder_SetTakeFromBuffer
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_SetTakeFromBuffer
(
  nssASN1Encoder *encoder,
  PRBool takeFromBuffer
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;

/*
 * nssASN1Encoder_GetTakeFromBuffer
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1Encoder_GetTakeFromBuffer
(
  nssASN1Encoder *encoder,
  PRBool *pTakeFromBuffer
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;
extern const NSSError NSS_ERROR_INVALID_POINTER;

/*
 * nssASN1Encoder_verify
 *
 * This routine is only available in debug builds.
 *
 * If the specified pointer is a valid pointer to an nssASN1Encoder
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ASN1ENCODER
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

#ifdef DEBUG
NSS_EXTERN PRStatus
nssASN1Encoder_verify
(
  nssASN1Encoder *encoder
);

extern const NSSError NSS_ERROR_INVALID_ASN1ENCODER;
#endif /* DEBUG */

/*
 * nssASN1_Encode
 *
 * 
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_ENCODING_NOT_SUPPORTED
 *  ...
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssASN1_Encode
(
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding,
  nssASN1EncoderWriteFunction *sink,
  void *argument
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;
extern const NSSError NSS_ERROR_ENCODING_NOT_SUPPORTED;

/*
 * nssASN1_EncodeItem
 *
 * There must be a better name.  If the optional arena argument is
 * non-null, it'll be used for the space.  If the optional rvOpt is
 * non-null, it'll be the return value-- if it is null, a new one
 * will be allocated.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_ENCODING_NOT_SUPPORTED
 *
 * Return value:
 *  NULL upon error
 *  A valid pointer to an NSSDER upon success
 */

NSS_EXTERN NSSDER *
nssASN1_EncodeItem
(
  NSSArena *arenaOpt,
  NSSDER *rvOpt,
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding
);

extern const NSSError NSS_ERROR_NO_MEMORY;
extern const NSSError NSS_ERROR_INVALID_ARENA;
extern const NSSError NSS_ERROR_INVALID_POINTER;
extern const NSSError NSS_ERROR_ENCODING_NOT_SUPPORTED;

/*
 * Other basic types' encoding and decoding helper functions:
 *
 *  nssASN1_CreatePRUint32FromBER
 *  nssASN1_GetDERFromPRUint32
 *  nssASN1_CreatePRInt32FromBER
 *  nssASN1_GetDERFromPRInt32
 * ..etc..
 */

/*
 * nssASN1_CreatePRUint32FromBER
 *
 */

NSS_EXTERN PRStatus
nssASN1_CreatePRUint32FromBER
(
  NSSBER *encoded,
  PRUint32 *pResult
);

/*
 * nssASN1_GetDERFromPRUint32
 *
 */

NSS_EXTERN NSSDER *
nssASN1_GetDERFromPRUint32
(
  NSSArena *arenaOpt,
  NSSDER *rvOpt,
  PRUint32 value
);

/*
 * nssASN1_CreatePRInt32FromBER
 *
 */

NSS_EXTERN PRStatus
nssASN1_CreatePRInt32FromBER
(
  NSSBER *encoded,
  PRInt32 *pResult
);

/*
 * nssASN1_GetDERFromPRInt32
 *
 */

NSS_EXTERN NSSDER *
nssASN1_GetDERFromPRInt32
(
  NSSArena *arenaOpt,
  NSSDER *rvOpt,
  PRInt32 value
);

/*
 * Builtin templates
 */

/*
 * Generic Templates
 * One for each of the simple types, plus a special one for ANY, plus:
 *	- a pointer to each one of those
 *	- a set of each one of those
 *
 * Note that these are alphabetical (case insensitive); please add new
 * ones in the appropriate place.
 */

extern const nssASN1Template *nssASN1Template_Any;
extern const nssASN1Template *nssASN1Template_BitString;
extern const nssASN1Template *nssASN1Template_BMPString;
extern const nssASN1Template *nssASN1Template_Boolean;
extern const nssASN1Template *nssASN1Template_Enumerated;
extern const nssASN1Template *nssASN1Template_GeneralizedTime;
extern const nssASN1Template *nssASN1Template_IA5String;
extern const nssASN1Template *nssASN1Template_Integer;
extern const nssASN1Template *nssASN1Template_Null;
extern const nssASN1Template *nssASN1Template_ObjectID;
extern const nssASN1Template *nssASN1Template_OctetString;
extern const nssASN1Template *nssASN1Template_PrintableString;
extern const nssASN1Template *nssASN1Template_T61String;
extern const nssASN1Template *nssASN1Template_UniversalString;
extern const nssASN1Template *nssASN1Template_UTCTime;
extern const nssASN1Template *nssASN1Template_UTF8String;
extern const nssASN1Template *nssASN1Template_VisibleString;

extern const nssASN1Template *nssASN1Template_PointerToAny;
extern const nssASN1Template *nssASN1Template_PointerToBitString;
extern const nssASN1Template *nssASN1Template_PointerToBMPString;
extern const nssASN1Template *nssASN1Template_PointerToBoolean;
extern const nssASN1Template *nssASN1Template_PointerToEnumerated;
extern const nssASN1Template *nssASN1Template_PointerToGeneralizedTime;
extern const nssASN1Template *nssASN1Template_PointerToIA5String;
extern const nssASN1Template *nssASN1Template_PointerToInteger;
extern const nssASN1Template *nssASN1Template_PointerToNull;
extern const nssASN1Template *nssASN1Template_PointerToObjectID;
extern const nssASN1Template *nssASN1Template_PointerToOctetString;
extern const nssASN1Template *nssASN1Template_PointerToPrintableString;
extern const nssASN1Template *nssASN1Template_PointerToT61String;
extern const nssASN1Template *nssASN1Template_PointerToUniversalString;
extern const nssASN1Template *nssASN1Template_PointerToUTCTime;
extern const nssASN1Template *nssASN1Template_PointerToUTF8String;
extern const nssASN1Template *nssASN1Template_PointerToVisibleString;

extern const nssASN1Template *nssASN1Template_SetOfAny;
extern const nssASN1Template *nssASN1Template_SetOfBitString;
extern const nssASN1Template *nssASN1Template_SetOfBMPString;
extern const nssASN1Template *nssASN1Template_SetOfBoolean;
extern const nssASN1Template *nssASN1Template_SetOfEnumerated;
extern const nssASN1Template *nssASN1Template_SetOfGeneralizedTime;
extern const nssASN1Template *nssASN1Template_SetOfIA5String;
extern const nssASN1Template *nssASN1Template_SetOfInteger;
extern const nssASN1Template *nssASN1Template_SetOfNull;
extern const nssASN1Template *nssASN1Template_SetOfObjectID;
extern const nssASN1Template *nssASN1Template_SetOfOctetString;
extern const nssASN1Template *nssASN1Template_SetOfPrintableString;
extern const nssASN1Template *nssASN1Template_SetOfT61String;
extern const nssASN1Template *nssASN1Template_SetOfUniversalString;
extern const nssASN1Template *nssASN1Template_SetOfUTCTime;
extern const nssASN1Template *nssASN1Template_SetOfUTF8String;
extern const nssASN1Template *nssASN1Template_SetOfVisibleString;

/*
 *
 */

NSS_EXTERN NSSUTF8 *
nssUTF8_CreateFromBER
(
  NSSArena *arenaOpt,
  nssStringType type,
  NSSBER *berData
);

NSS_EXTERN NSSDER *
nssUTF8_GetDEREncoding
(
  NSSArena *arenaOpt,
  /* Should have an NSSDER *rvOpt */
  nssStringType type,
  const NSSUTF8 *string
);

PR_END_EXTERN_C

#endif /* ASN1_H */
