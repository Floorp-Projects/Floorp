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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: asn1.c,v $ $Revision: 1.4 $ $Date: 2005/01/20 02:25:44 $";
#endif /* DEBUG */

/*
 * asn1.c
 *
 * At this point in time, this file contains the NSS wrappers for
 * the old "SEC" ASN.1 encoder/decoder stuff.
 */

#ifndef ASN1M_H
#include "asn1m.h"
#endif /* ASN1M_H */

#include "plarena.h"
#include "secasn1.h"

/*
 * The pointer-tracking stuff
 */

#ifdef DEBUG
extern const NSSError NSS_ERROR_INTERNAL_ERROR;

static nssPointerTracker decoder_pointer_tracker;

static PRStatus
decoder_add_pointer
(
  const nssASN1Decoder *decoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&decoder_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return rv;
  }

  rv = nssPointerTracker_add(&decoder_pointer_tracker, decoder);
  if( PR_SUCCESS != rv ) {
    NSSError e = NSS_GetError();
    if( NSS_ERROR_NO_MEMORY != e ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    }

    return rv;
  }

  return PR_SUCCESS;
}

static PRStatus
decoder_remove_pointer
(
  const nssASN1Decoder *decoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_remove(&decoder_pointer_tracker, decoder);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  }

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_verify
(
  nssASN1Decoder *decoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&decoder_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return PR_FAILURE;
  }

  rv = nssPointerTracker_verify(&decoder_pointer_tracker, decoder);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INVALID_ASN1DECODER);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

static nssPointerTracker encoder_pointer_tracker;

static PRStatus
encoder_add_pointer
(
  const nssASN1Encoder *encoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&encoder_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return rv;
  }

  rv = nssPointerTracker_add(&encoder_pointer_tracker, encoder);
  if( PR_SUCCESS != rv ) {
    NSSError e = NSS_GetError();
    if( NSS_ERROR_NO_MEMORY != e ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    }

    return rv;
  }

  return PR_SUCCESS;
}

static PRStatus
encoder_remove_pointer
(
  const nssASN1Encoder *encoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_remove(&encoder_pointer_tracker, encoder);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  }

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_verify
(
  nssASN1Encoder *encoder
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&encoder_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return PR_FAILURE;
  }

  rv = nssPointerTracker_verify(&encoder_pointer_tracker, encoder);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INVALID_ASN1ENCODER);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}
#endif /* DEBUG */

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

NSS_IMPLEMENT nssASN1Decoder *
nssASN1Decoder_Create
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[]
)
{
  SEC_ASN1DecoderContext *rv;
  PLArenaPool *hack = (PLArenaPool *)arenaOpt;

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (nssASN1Decoder *)NULL;
    }
  }

  /* 
   * May destination be NULL?  I'd think so, since one might
   * have only a filter proc.  But if not, check the pointer here.
   */

  if( (nssASN1Template *)NULL == template ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (nssASN1Decoder *)NULL;
  }
#endif /* DEBUG */

  rv = SEC_ASN1DecoderStart(hack, destination, template);
  if( (SEC_ASN1DecoderContext *)NULL == rv ) {
    nss_SetError(PORT_GetError()); /* also evil */
    return (nssASN1Decoder *)NULL;
  }

#ifdef DEBUG
  if( PR_SUCCESS != decoder_add_pointer(rv) ) {
    (void)SEC_ASN1DecoderFinish(rv);
    return (nssASN1Decoder *)NULL;
  }
#endif /* DEBUG */

  return (nssASN1Decoder *)rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_Update
(
  nssASN1Decoder *decoder,
  const void *data,
  PRUint32 amount
)
{
  SECStatus rv;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }

  if( (void *)NULL == data ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }
#endif /* DEBUG */

  rv = SEC_ASN1DecoderUpdate((SEC_ASN1DecoderContext *)decoder, 
                             (const char *)data,
                             (unsigned long)amount);
  if( SECSuccess != rv ) {
    nss_SetError(PORT_GetError()); /* ugly */
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_Finish
(
  nssASN1Decoder *decoder
)
{
  PRStatus rv = PR_SUCCESS;
  SECStatus srv;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  srv = SEC_ASN1DecoderFinish((SEC_ASN1DecoderContext *)decoder);

  if( SECSuccess != srv ) {
    nss_SetError(PORT_GetError()); /* ugly */
    rv = PR_FAILURE;
  }

#ifdef DEBUG
  {
    PRStatus rv2 = decoder_remove_pointer(decoder);
    if( PR_SUCCESS == rv ) {
      rv = rv2;
    }
  }
#endif /* DEBUG */

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_SetFilter
(
  nssASN1Decoder *decoder,
  nssASN1DecoderFilterFunction *callback,
  void *argument,
  PRBool noStore
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1DecoderFilterFunction *)NULL == callback ) {
     SEC_ASN1DecoderClearFilterProc((SEC_ASN1DecoderContext *)decoder);
  } else {
     SEC_ASN1DecoderSetFilterProc((SEC_ASN1DecoderContext *)decoder,
                                  (SEC_ASN1WriteProc)callback,
                                  argument, noStore);
  }

  /* No error returns defined for those routines */

  return PR_SUCCESS;
}

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

extern const NSSError NSS_ERROR_INTERNAL_ERROR;

NSS_IMPLEMENT PRStatus
nssASN1Decoder_GetFilter
(
  nssASN1Decoder *decoder,
  nssASN1DecoderFilterFunction **pCallbackOpt,
  void **pArgumentOpt,
  PRBool *pNoStoreOpt
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1DecoderFilterFunction **)NULL != pCallbackOpt ) {
    *pCallbackOpt = (nssASN1DecoderFilterFunction *)NULL;
  }

  if( (void **)NULL != pArgumentOpt ) {
    *pArgumentOpt = (void *)NULL;
  }

  if( (PRBool *)NULL != pNoStoreOpt ) {
    *pNoStoreOpt = PR_FALSE;
  }

  /* Error because it's unimplemented */
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FAILURE;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_SetNotify
(
  nssASN1Decoder *decoder,
  nssASN1NotifyFunction *callback,
  void *argument
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1NotifyFunction *)NULL == callback ) {
    SEC_ASN1DecoderClearNotifyProc((SEC_ASN1DecoderContext *)decoder);
  } else {
    SEC_ASN1DecoderSetNotifyProc((SEC_ASN1DecoderContext *)decoder,
                                 (SEC_ASN1NotifyProc)callback,
                                 argument);
  }

  /* No error returns defined for those routines */

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Decoder_GetNotify
(
  nssASN1Decoder *decoder,
  nssASN1NotifyFunction **pCallbackOpt,
  void **pArgumentOpt
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Decoder_verify(decoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1NotifyFunction **)NULL != pCallbackOpt ) {
    *pCallbackOpt = (nssASN1NotifyFunction *)NULL;
  }

  if( (void **)NULL != pArgumentOpt ) {
    *pArgumentOpt = (void *)NULL;
  }

  /* Error because it's unimplemented */
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FAILURE;
}

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

NSS_IMPLEMENT PRStatus
nssASN1_Decode
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[],
  const void *berData,
  PRUint32 amount
)
{
  PRStatus rv;
  nssASN1Decoder *decoder;

  /* This call will do our pointer-checking for us! */
  decoder = nssASN1Decoder_Create(arenaOpt, destination, template);
  if( (nssASN1Decoder *)NULL == decoder ) {
    return PR_FAILURE;
  }

  rv = nssASN1Decoder_Update(decoder, berData, amount);
  if( PR_SUCCESS != nssASN1Decoder_Finish(decoder) ) {
    rv = PR_FAILURE;
  }

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1_DecodeBER
(
  NSSArena *arenaOpt,
  void *destination,
  const nssASN1Template template[],
  const NSSBER *data
)
{
  return nssASN1_Decode(arenaOpt, destination, template, 
                        data->data, data->size);
}

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
 *  ...
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an ASN.1 Encoder upon success
 */

NSS_IMPLEMENT nssASN1Encoder *
nssASN1Encoder_Create
(
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding,
  nssASN1EncoderWriteFunction *sink,
  void *argument
)
{
  SEC_ASN1EncoderContext *rv;

#ifdef DEBUG
  if( (void *)NULL == source ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (nssASN1Encoder *)NULL;
  }

  if( (nssASN1Template *)NULL == template ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (nssASN1Encoder *)NULL;
  }

  if( (nssASN1EncoderWriteFunction *)NULL == sink ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (nssASN1Encoder *)NULL;
  }
#endif /* DEBUG */

  switch( encoding ) {
  case NSSASN1BER:
  case NSSASN1DER:
    break;
  case NSSASN1CER:
  case NSSASN1LWER:
  case NSSASN1PER:
  case NSSASN1UnknownEncoding:
  default:
    nss_SetError(NSS_ERROR_ENCODING_NOT_SUPPORTED);
    return (nssASN1Encoder *)NULL;
  }

  rv = SEC_ASN1EncoderStart((void *)source, template, 
                            (SEC_ASN1WriteProc)sink, argument);
  if( (SEC_ASN1EncoderContext *)NULL == rv ) {
    nss_SetError(PORT_GetError()); /* ugly */
    return (nssASN1Encoder *)NULL;
  }

  if( NSSASN1DER == encoding ) {
    sec_ASN1EncoderSetDER(rv);
  }

#ifdef DEBUG
  if( PR_SUCCESS != encoder_add_pointer(rv) ) {
    (void)SEC_ASN1EncoderFinish(rv);
    return (nssASN1Encoder *)NULL;
  }
#endif /* DEBUG */

  return (nssASN1Encoder *)rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_Update
(
  nssASN1Encoder *encoder,
  const void *data,
  PRUint32 length
)
{
  SECStatus rv;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }

  /*
   * Can data legitimately be NULL?  If not, verify..
   */
#endif /* DEBUG */

  rv = SEC_ASN1EncoderUpdate((SEC_ASN1EncoderContext *)encoder,
                             (const char *)data, 
                             (unsigned long)length);
  if( SECSuccess != rv ) {
    nss_SetError(PORT_GetError()); /* ugly */
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_Finish
(
  nssASN1Encoder *encoder
)
{
  PRStatus rv;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  SEC_ASN1EncoderFinish((SEC_ASN1EncoderContext *)encoder);
  rv = PR_SUCCESS; /* no error return defined for that call */

#ifdef DEBUG
  {
    PRStatus rv2 = encoder_remove_pointer(encoder);
    if( PR_SUCCESS == rv ) {
      rv = rv2;
    }
  }
#endif /* DEBUG */

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_SetNotify
(
  nssASN1Encoder *encoder,
  nssASN1NotifyFunction *callback,
  void *argument
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1NotifyFunction *)NULL == callback ) {
    SEC_ASN1EncoderClearNotifyProc((SEC_ASN1EncoderContext *)encoder);
  } else {
    SEC_ASN1EncoderSetNotifyProc((SEC_ASN1EncoderContext *)encoder,
                                 (SEC_ASN1NotifyProc)callback,
                                 argument);
  }

  /* no error return defined for those routines */

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_GetNotify
(
  nssASN1Encoder *encoder,
  nssASN1NotifyFunction **pCallbackOpt,
  void **pArgumentOpt
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (nssASN1NotifyFunction **)NULL != pCallbackOpt ) {
    *pCallbackOpt = (nssASN1NotifyFunction *)NULL;
  }

  if( (void **)NULL != pArgumentOpt ) {
    *pArgumentOpt = (void *)NULL;
  }

  /* Error because it's unimplemented */
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FAILURE;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_SetStreaming
(
  nssASN1Encoder *encoder,
  PRBool streaming
)
{
  SEC_ASN1EncoderContext *cx = (SEC_ASN1EncoderContext *)encoder;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( streaming ) {
    SEC_ASN1EncoderSetStreaming(cx);
  } else {
    SEC_ASN1EncoderClearStreaming(cx);
  }

  /* no error return defined for those routines */

  return PR_SUCCESS;
}

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
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (PRBool *)NULL != pStreaming ) {
    *pStreaming = PR_FALSE;
  }

  /* Error because it's unimplemented */
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FAILURE;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_SetTakeFromBuffer
(
  nssASN1Encoder *encoder,
  PRBool takeFromBuffer
)
{
  SEC_ASN1EncoderContext *cx = (SEC_ASN1EncoderContext *)encoder;

#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( takeFromBuffer ) {
    SEC_ASN1EncoderSetTakeFromBuf(cx);
  } else {
    SEC_ASN1EncoderClearTakeFromBuf(cx);
  }

  /* no error return defined for those routines */

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT PRStatus
nssASN1Encoder_GetTakeFromBuffer
(
  nssASN1Encoder *encoder,
  PRBool *pTakeFromBuffer
)
{
#ifdef DEBUG
  if( PR_SUCCESS != nssASN1Encoder_verify(encoder) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( (PRBool *)NULL != pTakeFromBuffer ) {
    *pTakeFromBuffer = PR_FALSE;
  }

  /* Error because it's unimplemented */
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FAILURE;
}

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

NSS_IMPLEMENT PRStatus
nssASN1_Encode
(
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding,
  nssASN1EncoderWriteFunction *sink,
  void *argument
)
{
  PRStatus rv;
  nssASN1Encoder *encoder;

  encoder = nssASN1Encoder_Create(source, template, encoding, sink, argument);
  if( (nssASN1Encoder *)NULL == encoder ) {
    return PR_FAILURE;
  }

  rv = nssASN1Encoder_Update(encoder, (const void *)NULL, 0);
  if( PR_SUCCESS != nssASN1Encoder_Finish(encoder) ) {
    rv = PR_FAILURE;
  }

  return rv;
}

/*
 * nssasn1_encode_item_count
 *
 * This is a helper function for nssASN1_EncodeItem.  It just counts
 * up the space required for an encoding.
 */

static void
nssasn1_encode_item_count
(
  void *arg,
  const char *buf,
  unsigned long len,
  int depth,
  nssASN1EncodingPart data_kind
)
{
  unsigned long *count;

  count = (unsigned long*)arg;
  PR_ASSERT (count != NULL);

  *count += len;
}

/*
 * nssasn1_encode_item_store
 *
 * This is a helper function for nssASN1_EncodeItem.  It appends the
 * new data onto the destination item.
 */

static void
nssasn1_encode_item_store
(
  void *arg,
  const char *buf,
  unsigned long len,
  int depth,
  nssASN1EncodingPart data_kind
)
{
  NSSItem *dest;

  dest = (NSSItem*)arg;
  PR_ASSERT (dest != NULL);

  memcpy((unsigned char *)dest->data + dest->size, buf, len);
  dest->size += len;
}

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

NSS_IMPLEMENT NSSDER *
nssASN1_EncodeItem
(
  NSSArena *arenaOpt,
  NSSDER *rvOpt,
  const void *source,
  const nssASN1Template template[],
  NSSASN1EncodingType encoding
)
{
  NSSDER *rv;
  PRUint32 len = 0;
  PRStatus status;

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }

  if( (void *)NULL == source ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSDER *)NULL;
  }

  if( (nssASN1Template *)NULL == template ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSDER *)NULL;
  }
#endif /* DEBUG */

  status = nssASN1_Encode(source, template, encoding, 
                          (nssASN1EncoderWriteFunction *)nssasn1_encode_item_count, 
                          &len);
  if( PR_SUCCESS != status ) {
    return (NSSDER *)NULL;
  }

  if( (NSSDER *)NULL == rvOpt ) {
    rv = nss_ZNEW(arenaOpt, NSSDER);
    if( (NSSDER *)NULL == rv ) {
      return (NSSDER *)NULL;
    }
  } else {
    rv = rvOpt;
  }

  rv->size = len;
  rv->data = nss_ZAlloc(arenaOpt, len);
  if( (void *)NULL == rv->data ) {
    if( (NSSDER *)NULL == rvOpt ) {
      nss_ZFreeIf(rv);
    }
    return (NSSDER *)NULL;
  }

  rv->size = 0; /* for nssasn1_encode_item_store */

  status = nssASN1_Encode(source, template, encoding,
                          (nssASN1EncoderWriteFunction *)nssasn1_encode_item_store, 
                          rv);
  if( PR_SUCCESS != status ) {
    nss_ZFreeIf(rv->data);
    if( (NSSDER *)NULL == rvOpt ) {
      nss_ZFreeIf(rv);
    }
    return (NSSDER *)NULL;
  }

  PR_ASSERT(rv->size == len);

  return rv;
}

/*
 * nssASN1_CreatePRUint32FromBER
 *
 */

NSS_IMPLEMENT PRStatus
nssASN1_CreatePRUint32FromBER
(
  NSSBER *encoded,
  PRUint32 *pResult
)
{
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FALSE;
}

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
)
{
  NSSDER *rv;
  PLArenaPool *hack = (PLArenaPool *)arenaOpt;
  SECItem *item;

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }
#endif /* DEBUG */

  if( (NSSDER *)NULL == rvOpt ) {
    rv = nss_ZNEW(arenaOpt, NSSDER);
    if( (NSSDER *)NULL == rv ) {
      return (NSSDER *)NULL;
    }
  } else {
    rv = rvOpt;
  }

  item = SEC_ASN1EncodeUnsignedInteger(hack, (SECItem *)rv, value);
  if( (SECItem *)NULL == item ) {
    if( (NSSDER *)NULL == rvOpt ) {
      (void)nss_ZFreeIf(rv);
    }

    nss_SetError(PORT_GetError()); /* ugly */
    return (NSSDER *)NULL;
  }

  /* 
   * I happen to know that these things look alike.. but I'm only
   * doing it for these "temporary" wrappers.  This is an evil thing.
   */
  return (NSSDER *)item;
}

/*himom*/
NSS_IMPLEMENT PRStatus
nssASN1_CreatePRInt32FromBER
(
  NSSBER *encoded,
  PRInt32 *pResult
)
{
  nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  return PR_FALSE;
}

/*
 * nssASN1_GetDERFromPRInt32
 *
 */

NSS_IMPLEMENT NSSDER *
nssASN1_GetDERFromPRInt32
(
  NSSArena *arenaOpt,
  NSSDER *rvOpt,
  PRInt32 value
)
{
  NSSDER *rv;
  PLArenaPool *hack = (PLArenaPool *)arenaOpt;
  SECItem *item;

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }
#endif /* DEBUG */

  if( (NSSDER *)NULL == rvOpt ) {
    rv = nss_ZNEW(arenaOpt, NSSDER);
    if( (NSSDER *)NULL == rv ) {
      return (NSSDER *)NULL;
    }
  } else {
    rv = rvOpt;
  }

  item = SEC_ASN1EncodeInteger(hack, (SECItem *)rv, value);
  if( (SECItem *)NULL == item ) {
    if( (NSSDER *)NULL == rvOpt ) {
      (void)nss_ZFreeIf(rv);
    }

    nss_SetError(PORT_GetError()); /* ugly */
    return (NSSDER *)NULL;
  }

  /* 
   * I happen to know that these things look alike.. but I'm only
   * doing it for these "temporary" wrappers.  This is an evil thing.
   */
  return (NSSDER *)item;
}

/*
 * Generic Templates
 * One for each of the simple types, plus a special one for ANY, plus:
 *	- a pointer to each one of those
 *	- a set of each one of those
 *
 * Note that these are alphabetical (case insensitive); please add new
 * ones in the appropriate place.
 */

const nssASN1Template *nssASN1Template_Any =                             (nssASN1Template *)SEC_AnyTemplate;
const nssASN1Template *nssASN1Template_BitString =                       (nssASN1Template *)SEC_BitStringTemplate;
const nssASN1Template *nssASN1Template_BMPString =                       (nssASN1Template *)SEC_BMPStringTemplate;
const nssASN1Template *nssASN1Template_Boolean =                         (nssASN1Template *)SEC_BooleanTemplate;
const nssASN1Template *nssASN1Template_Enumerated =                      (nssASN1Template *)SEC_EnumeratedTemplate;
const nssASN1Template *nssASN1Template_GeneralizedTime =                 (nssASN1Template *)SEC_GeneralizedTimeTemplate;
const nssASN1Template *nssASN1Template_IA5String =                       (nssASN1Template *)SEC_IA5StringTemplate;
const nssASN1Template *nssASN1Template_Integer =                         (nssASN1Template *)SEC_IntegerTemplate;
const nssASN1Template *nssASN1Template_Null =                            (nssASN1Template *)SEC_NullTemplate;
const nssASN1Template *nssASN1Template_ObjectID =                        (nssASN1Template *)SEC_ObjectIDTemplate;
const nssASN1Template *nssASN1Template_OctetString =                     (nssASN1Template *)SEC_OctetStringTemplate;
const nssASN1Template *nssASN1Template_PrintableString =                 (nssASN1Template *)SEC_PrintableStringTemplate;
const nssASN1Template *nssASN1Template_T61String =                       (nssASN1Template *)SEC_T61StringTemplate;
const nssASN1Template *nssASN1Template_UniversalString =                 (nssASN1Template *)SEC_UniversalStringTemplate;
const nssASN1Template *nssASN1Template_UTCTime =                         (nssASN1Template *)SEC_UTCTimeTemplate;
const nssASN1Template *nssASN1Template_UTF8String =                      (nssASN1Template *)SEC_UTF8StringTemplate;
const nssASN1Template *nssASN1Template_VisibleString =                   (nssASN1Template *)SEC_VisibleStringTemplate;

const nssASN1Template *nssASN1Template_PointerToAny =                    (nssASN1Template *)SEC_PointerToAnyTemplate;
const nssASN1Template *nssASN1Template_PointerToBitString =              (nssASN1Template *)SEC_PointerToBitStringTemplate;
const nssASN1Template *nssASN1Template_PointerToBMPString =              (nssASN1Template *)SEC_PointerToBMPStringTemplate;
const nssASN1Template *nssASN1Template_PointerToBoolean =                (nssASN1Template *)SEC_PointerToBooleanTemplate;
const nssASN1Template *nssASN1Template_PointerToEnumerated =             (nssASN1Template *)SEC_PointerToEnumeratedTemplate;
const nssASN1Template *nssASN1Template_PointerToGeneralizedTime =        (nssASN1Template *)SEC_PointerToGeneralizedTimeTemplate;
const nssASN1Template *nssASN1Template_PointerToIA5String =              (nssASN1Template *)SEC_PointerToIA5StringTemplate;
const nssASN1Template *nssASN1Template_PointerToInteger =                (nssASN1Template *)SEC_PointerToIntegerTemplate;
const nssASN1Template *nssASN1Template_PointerToNull =                   (nssASN1Template *)SEC_PointerToNullTemplate;
const nssASN1Template *nssASN1Template_PointerToObjectID =               (nssASN1Template *)SEC_PointerToObjectIDTemplate;
const nssASN1Template *nssASN1Template_PointerToOctetString =            (nssASN1Template *)SEC_PointerToOctetStringTemplate;
const nssASN1Template *nssASN1Template_PointerToPrintableString =        (nssASN1Template *)SEC_PointerToPrintableStringTemplate;
const nssASN1Template *nssASN1Template_PointerToT61String =              (nssASN1Template *)SEC_PointerToT61StringTemplate;
const nssASN1Template *nssASN1Template_PointerToUniversalString =        (nssASN1Template *)SEC_PointerToUniversalStringTemplate;
const nssASN1Template *nssASN1Template_PointerToUTCTime =                (nssASN1Template *)SEC_PointerToUTCTimeTemplate;
const nssASN1Template *nssASN1Template_PointerToUTF8String =             (nssASN1Template *)SEC_PointerToUTF8StringTemplate;
const nssASN1Template *nssASN1Template_PointerToVisibleString =          (nssASN1Template *)SEC_PointerToVisibleStringTemplate;

const nssASN1Template *nssASN1Template_SetOfAny =                        (nssASN1Template *)SEC_SetOfAnyTemplate;
const nssASN1Template *nssASN1Template_SetOfBitString =                  (nssASN1Template *)SEC_SetOfBitStringTemplate;
const nssASN1Template *nssASN1Template_SetOfBMPString =                  (nssASN1Template *)SEC_SetOfBMPStringTemplate;
const nssASN1Template *nssASN1Template_SetOfBoolean =                    (nssASN1Template *)SEC_SetOfBooleanTemplate;
const nssASN1Template *nssASN1Template_SetOfEnumerated =                 (nssASN1Template *)SEC_SetOfEnumeratedTemplate;
const nssASN1Template *nssASN1Template_SetOfGeneralizedTime =            (nssASN1Template *)SEC_SetOfGeneralizedTimeTemplate;
const nssASN1Template *nssASN1Template_SetOfIA5String =                  (nssASN1Template *)SEC_SetOfIA5StringTemplate;
const nssASN1Template *nssASN1Template_SetOfInteger =                    (nssASN1Template *)SEC_SetOfIntegerTemplate;
const nssASN1Template *nssASN1Template_SetOfNull =                       (nssASN1Template *)SEC_SetOfNullTemplate;
const nssASN1Template *nssASN1Template_SetOfObjectID =                   (nssASN1Template *)SEC_SetOfObjectIDTemplate;
const nssASN1Template *nssASN1Template_SetOfOctetString =                (nssASN1Template *)SEC_SetOfOctetStringTemplate;
const nssASN1Template *nssASN1Template_SetOfPrintableString =            (nssASN1Template *)SEC_SetOfPrintableStringTemplate;
const nssASN1Template *nssASN1Template_SetOfT61String =                  (nssASN1Template *)SEC_SetOfT61StringTemplate;
const nssASN1Template *nssASN1Template_SetOfUniversalString =            (nssASN1Template *)SEC_SetOfUniversalStringTemplate;
const nssASN1Template *nssASN1Template_SetOfUTCTime =                    (nssASN1Template *)SEC_SetOfUTCTimeTemplate;
const nssASN1Template *nssASN1Template_SetOfUTF8String =                 (nssASN1Template *)SEC_SetOfUTF8StringTemplate;
const nssASN1Template *nssASN1Template_SetOfVisibleString =              (nssASN1Template *)SEC_SetOfVisibleStringTemplate;

/*
 *
 */

NSS_IMPLEMENT NSSUTF8 *
nssUTF8_CreateFromBER
(
  NSSArena *arenaOpt,
  nssStringType type,
  NSSBER *berData
)
{
  NSSUTF8 *rv = NULL;
  PRUint8 tag;
  NSSArena *a;
  NSSItem in;
  NSSItem out;
  PRStatus st;
  const nssASN1Template *templ;

#ifdef NSSDEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }

  if( (NSSBER *)NULL == berData ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSUTF8 *)NULL;
  }

  if( (void *)NULL == berData->data ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSUTF8 *)NULL;
  }
#endif /* NSSDEBUG */

  a = NSSArena_Create();
  if( (NSSArena *)NULL == a ) {
    return (NSSUTF8 *)NULL;
  }

  in = *berData;

  /*
   * By the way, at first I succumbed to the temptation to make
   * this an incestuous nested switch statement.  Count yourself
   * lucky I cleaned it up.
   */

  switch( type ) {
  case nssStringType_DirectoryString:
    /*
     * draft-ietf-pkix-ipki-part1-11 says in part:
     *
     * DirectoryString { INTEGER:maxSize } ::= CHOICE {
     *   teletexString           TeletexString (SIZE (1..maxSize)),
     *   printableString         PrintableString (SIZE (1..maxSize)),
     *   universalString         UniversalString (SIZE (1..maxSize)),
     *   bmpString               BMPString (SIZE(1..maxSize)),
     *   utf8String              UTF8String (SIZE(1..maxSize))
     *                       }
     *
     * The tags are:
     *  TeletexString       UNIVERSAL 20
     *  PrintableString     UNIVERSAL 19
     *  UniversalString     UNIVERSAL 28
     *  BMPString           UNIVERSAL 30
     *  UTF8String          UNIVERSAL 12
     *
     * "UNIVERSAL" tags have bits 8 and 7 zero, bit 6 is zero for
     * primitive encodings, and if the tag value is less than 30,
     * the tag value is directly encoded in bits 5 through 1.
     */
    in.data = (void *)&(((PRUint8 *)berData->data)[1]);
    in.size = berData->size-1;
    
    tag = *(PRUint8 *)berData->data;
    switch( tag & nssASN1_TAGNUM_MASK ) {
    case 20:
      /*
       * XXX fgmr-- we have to accept Latin-1 for Teletex; (see
       * below) but is T61 a suitable value for "Latin-1"?
       */
      templ = nssASN1Template_T61String;
      type = nssStringType_TeletexString;
      break;
    case 19:
      templ = nssASN1Template_PrintableString;
      type = nssStringType_PrintableString;
      break;
    case 28:
      templ = nssASN1Template_UniversalString;
      type = nssStringType_UniversalString;
      break;
    case 30:
      templ = nssASN1Template_BMPString;
      type = nssStringType_BMPString;
      break;
    case 12:
      templ = nssASN1Template_UTF8String;
      type = nssStringType_UTF8String;
      break;
    default:
      nss_SetError(NSS_ERROR_INVALID_POINTER); /* "pointer"? */
      (void)NSSArena_Destroy(a);
      return (NSSUTF8 *)NULL;
    }

    break;

  case nssStringType_TeletexString:
    /*
     * XXX fgmr-- we have to accept Latin-1 for Teletex; (see
     * below) but is T61 a suitable value for "Latin-1"?
     */
    templ = nssASN1Template_T61String;
    break;
   
  case nssStringType_PrintableString:
    templ = nssASN1Template_PrintableString;
    break;

  case nssStringType_UniversalString:
    templ = nssASN1Template_UniversalString;
    break;

  case nssStringType_BMPString:
    templ = nssASN1Template_BMPString;
    break;

  case nssStringType_UTF8String:
    templ = nssASN1Template_UTF8String;
    break;

  case nssStringType_PHGString:
    templ = nssASN1Template_IA5String;
    break;

  default:
    nss_SetError(NSS_ERROR_UNSUPPORTED_TYPE);
    (void)NSSArena_Destroy(a);
    return (NSSUTF8 *)NULL;
  }
    
  st = nssASN1_DecodeBER(a, &out, templ, &in);
  
  if( PR_SUCCESS == st ) {
    rv = nssUTF8_Create(arenaOpt, type, out.data, out.size);
  }
  (void)NSSArena_Destroy(a);
  
  return rv;
}

NSS_EXTERN NSSDER *
nssUTF8_GetDEREncoding
(
  NSSArena *arenaOpt,
  nssStringType type,
  const NSSUTF8 *string
)
{
  NSSDER *rv = (NSSDER *)NULL;
  NSSItem str;
  NSSDER *der;
  const nssASN1Template *templ;
  NSSArena *a;

#ifdef NSSDEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }

  if( (const NSSUTF8 *)NULL == string ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSDER *)NULL;
  }
#endif /* NSSDEBUG */

  str.data = (void *)string;
  str.size = nssUTF8_Size(string, (PRStatus *)NULL);
  if( 0 == str.size ) {
    return (NSSDER *)NULL;
  }

  a = NSSArena_Create();
  if( (NSSArena *)NULL == a ) {
    return (NSSDER *)NULL;
  }

  switch( type ) {
  case nssStringType_DirectoryString:
    {
      NSSDER *utf;
      PRUint8 *c;

      utf = nssASN1_EncodeItem(a, (NSSDER *)NULL, &str, 
                               nssASN1Template_UTF8String,
                               NSSASN1DER);
      if( (NSSDER *)NULL == utf ) {
        (void)NSSArena_Destroy(a);
        return (NSSDER *)NULL;
      }

      rv = nss_ZNEW(arenaOpt, NSSDER);
      if( (NSSDER *)NULL == rv ) {
        (void)NSSArena_Destroy(a);
        return (NSSDER *)NULL;
      }

      rv->size = utf->size + 1;
      rv->data = nss_ZAlloc(arenaOpt, rv->size);
      if( (void *)NULL == rv->data ) {
        (void)nss_ZFreeIf(rv);
        (void)NSSArena_Destroy(a);
        return (NSSDER *)NULL;
      }
      
      c = (PRUint8 *)rv->data;
      (void)nsslibc_memcpy(&c[1], utf->data, utf->size);
      *c = 12; /* UNIVERSAL primitive encoding tag for UTF8String */

      (void)NSSArena_Destroy(a);
      return rv;
    }
  case nssStringType_TeletexString:
    /*
     * XXX fgmr-- we have to accept Latin-1 for Teletex; (see
     * below) but is T61 a suitable value for "Latin-1"?
     */
    templ = nssASN1Template_T61String;
    break;
  case nssStringType_PrintableString:
    templ = nssASN1Template_PrintableString;
    break;

  case nssStringType_UniversalString:
    templ = nssASN1Template_UniversalString;
    break;

  case nssStringType_BMPString:
    templ = nssASN1Template_BMPString;
    break;

  case nssStringType_UTF8String:
    templ = nssASN1Template_UTF8String;
    break;

  case nssStringType_PHGString:
    templ = nssASN1Template_IA5String;
    break;

  default:
    nss_SetError(NSS_ERROR_UNSUPPORTED_TYPE);
    (void)NSSArena_Destroy(a);
    return (NSSDER *)NULL;
  }

  der = nssUTF8_GetDEREncoding(a, type, string);
  if( (NSSItem *)NULL == der ) {
    (void)NSSArena_Destroy(a);
    return (NSSDER *)NULL;
  }

  rv = nssASN1_EncodeItem(arenaOpt, (NSSDER *)NULL, der, templ, NSSASN1DER);
  if( (NSSDER *)NULL == rv ) {
    (void)NSSArena_Destroy(a);
    return (NSSDER *)NULL;
  }

  (void)NSSArena_Destroy(a);
  return rv;
}
