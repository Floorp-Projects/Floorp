/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 *  JAREVIL
 *
 *  Wrappers to callback in the mozilla thread
 *
 *  Certificate code is unsafe when called outside the
 *  mozilla thread. These functions push an event on the
 *  queue to cause the cert function to run in that thread. 
 *
 */

#include "jar.h"
#include "jarint.h"

#include "jarevil.h"

/* from libevent.h */
#ifdef MOZILLA_CLIENT_OLD
typedef void (*ETVoidPtrFunc) (void * data);
extern void ET_moz_CallFunction (ETVoidPtrFunc fn, void *data);

extern void *mozilla_event_queue;
#endif


/* Special macros facilitate running on Win 16 */
#if defined(XP_WIN16)

  /* 
   * Allocate the data passed to the callback functions from the heap...
   *
   * This inter-thread structure cannot reside on a thread stack since the 
   * thread's stack is swapped away with the thread under Win16...
   */

 #define ALLOC_OR_DEFINE(type, pointer_var_name, out_of_memory_return_value) \
         type * pointer_var_name = PORT_ZAlloc (sizeof(type));               \
         do {                                                                \
           if (!pointer_var_name)                                            \
             return (out_of_memory_return_value);                            \
         } while (0)   /* and now a semicolon can follow :-) */

 #define FREE_IF_ALLOC_IS_USED(pointer_var_name) PORT_Free(pointer_var_name)

#else /* not win 16... so we can alloc via auto variables */

 #define ALLOC_OR_DEFINE(type, pointer_var_name, out_of_memory_return_value) \
         type actual_structure_allocated_in_macro;                           \
         type * pointer_var_name = &actual_structure_allocated_in_macro;     \
         PORT_Memset (pointer_var_name, 0, sizeof (*pointer_var_name));      \
         ((void) 0) /* and now a semicolon can follow  */

 #define FREE_IF_ALLOC_IS_USED(pointer_var_name) ((void) 0)

#endif /* not Win 16 */

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_encode
 *
 *  Call SEC_PKCS7Encode inside
 *  the mozilla thread
 *
 */

struct EVIL_encode
  {
  int error;
  SECStatus status;
  SEC_PKCS7ContentInfo *cinfo;
  SEC_PKCS7EncoderOutputCallback outputfn;
  void *outputarg;
  PK11SymKey *bulkkey;
  SECKEYGetPasswordKey pwfn;
  void *pwfnarg;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_encode_fn (void *data)
  {
  SECStatus status;
  struct EVIL_encode *encode_data = (struct EVIL_encode *)data;

  PORT_SetError (encode_data->error);

  status = SEC_PKCS7Encode (encode_data->cinfo, encode_data->outputfn, 
                            encode_data->outputarg, encode_data->bulkkey, 
                            encode_data->pwfn, encode_data->pwfnarg);

  encode_data->status = status;
  encode_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
SECStatus jar_moz_encode
      (
      SEC_PKCS7ContentInfo *cinfo,
      SEC_PKCS7EncoderOutputCallback  outputfn,
      void *outputarg,
      PK11SymKey *bulkkey,
      SECKEYGetPasswordKey pwfn,
      void *pwfnarg
      )
  {
  SECStatus ret;
  ALLOC_OR_DEFINE(struct EVIL_encode, encode_data, SECFailure);

  encode_data->error     = PORT_GetError();
  encode_data->cinfo     = cinfo;
  encode_data->outputfn  = outputfn;
  encode_data->outputarg = outputarg;
  encode_data->bulkkey   = bulkkey;
  encode_data->pwfn      = pwfn;
  encode_data->pwfnarg   = pwfnarg;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_encode_fn, encode_data);
  else
    jar_moz_encode_fn (encode_data);
#else
  jar_moz_encode_fn (encode_data);
#endif

  PORT_SetError (encode_data->error);
  ret = encode_data->status;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(encode_data);
  return ret;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_verify
 *
 *  Call SEC_PKCS7VerifyDetachedSignature inside
 *  the mozilla thread
 *
 */

struct EVIL_verify
  {
  int error;
  SECStatus status;
  SEC_PKCS7ContentInfo *cinfo;
  SECCertUsage certusage;
  SECItem *detached_digest;
  HASH_HashType digest_type;
  PRBool keepcerts;
  };

/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_verify_fn (void *data)
  {
	PRBool result;
  struct EVIL_verify *verify_data = (struct EVIL_verify *)data;

  PORT_SetError (verify_data->error);

  result = SEC_PKCS7VerifyDetachedSignature
        (verify_data->cinfo, verify_data->certusage, verify_data->detached_digest, 
         verify_data->digest_type, verify_data->keepcerts);
	

  verify_data->status = result==PR_TRUE ? SECSuccess : SECFailure;
  verify_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
SECStatus jar_moz_verify
      (
      SEC_PKCS7ContentInfo *cinfo,
      SECCertUsage certusage,
      SECItem *detached_digest,
      HASH_HashType digest_type,
      PRBool keepcerts
      )
  {
  SECStatus ret;
  ALLOC_OR_DEFINE(struct EVIL_verify, verify_data, SECFailure);

  verify_data->error           = PORT_GetError();
  verify_data->cinfo           = cinfo;
  verify_data->certusage       = certusage;
  verify_data->detached_digest = detached_digest;
  verify_data->digest_type     = digest_type;
  verify_data->keepcerts       = keepcerts;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_verify_fn, verify_data);
  else
    jar_moz_verify_fn (verify_data);
#else
  jar_moz_verify_fn (verify_data);
#endif

  PORT_SetError (verify_data->error);
  ret = verify_data->status;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(verify_data);
  return ret;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_nickname
 *
 *  Call CERT_FindCertByNickname inside
 *  the mozilla thread
 *
 */

struct EVIL_nickname
  {
  int error;
  CERTCertDBHandle *certdb;
  char *nickname;
  CERTCertificate *cert;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_nickname_fn (void *data)
  {
  CERTCertificate *cert;
  struct EVIL_nickname *nickname_data = (struct EVIL_nickname *)data;

  PORT_SetError (nickname_data->error);

  cert = CERT_FindCertByNickname (nickname_data->certdb, nickname_data->nickname);

  nickname_data->cert  = cert;
  nickname_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
CERTCertificate *jar_moz_nickname (CERTCertDBHandle *certdb, char *nickname)
  {
  CERTCertificate *cert;
  ALLOC_OR_DEFINE(struct EVIL_nickname, nickname_data, NULL );

  nickname_data->error    = PORT_GetError();
  nickname_data->certdb   = certdb;
  nickname_data->nickname = nickname;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_nickname_fn, nickname_data);
  else
    jar_moz_nickname_fn (nickname_data);
#else
  jar_moz_nickname_fn (nickname_data);
#endif

  PORT_SetError (nickname_data->error);
  cert = nickname_data->cert;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(nickname_data);
  return cert;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_perm
 *
 *  Call CERT_AddTempCertToPerm inside
 *  the mozilla thread
 *
 */

struct EVIL_perm
  {
  int error;
  SECStatus status;
  CERTCertificate *cert;
  char *nickname;
  CERTCertTrust *trust;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_perm_fn (void *data)
  {
  SECStatus status;
  struct EVIL_perm *perm_data = (struct EVIL_perm *)data;

  PORT_SetError (perm_data->error);

  status = CERT_AddTempCertToPerm (perm_data->cert, perm_data->nickname, perm_data->trust);

  perm_data->status = status;
  perm_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
SECStatus jar_moz_perm 
    (CERTCertificate *cert, char *nickname, CERTCertTrust *trust)
  {
  SECStatus ret;
  ALLOC_OR_DEFINE(struct EVIL_perm, perm_data, SECFailure);

  perm_data->error    = PORT_GetError();
  perm_data->cert     = cert;
  perm_data->nickname = nickname;
  perm_data->trust    = trust;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_perm_fn, perm_data);
  else
    jar_moz_perm_fn (perm_data);
#else
  jar_moz_perm_fn (perm_data);
#endif

  PORT_SetError (perm_data->error);
  ret = perm_data->status;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(perm_data);
  return ret;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_certkey
 *
 *  Call CERT_FindCertByKey inside
 *  the mozilla thread
 *
 */

struct EVIL_certkey
  {
  int error;
  CERTCertificate *cert;
  CERTCertDBHandle *certdb;
  CERTIssuerAndSN *seckey;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_certkey_fn (void *data)
  {
  CERTCertificate *cert;
  struct EVIL_certkey *certkey_data = (struct EVIL_certkey *)data;

  PORT_SetError (certkey_data->error);

  cert=CERT_FindCertByIssuerAndSN(certkey_data->certdb, certkey_data->seckey);

  certkey_data->cert = cert;
  certkey_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
CERTCertificate *jar_moz_certkey (CERTCertDBHandle *certdb, 
						CERTIssuerAndSN *seckey)
  {
  CERTCertificate *cert;
  ALLOC_OR_DEFINE(struct EVIL_certkey, certkey_data, NULL);

  certkey_data->error  = PORT_GetError();
  certkey_data->certdb = certdb;
  certkey_data->seckey = seckey;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_certkey_fn, certkey_data);
  else
    jar_moz_certkey_fn (certkey_data);
#else
  jar_moz_certkey_fn (certkey_data);
#endif

  PORT_SetError (certkey_data->error);
  cert = certkey_data->cert;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(certkey_data);
  return cert;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_issuer
 *
 *  Call CERT_FindCertIssuer inside
 *  the mozilla thread
 *
 */

struct EVIL_issuer
  {
  int error;
  CERTCertificate *cert;
  CERTCertificate *issuer;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_issuer_fn (void *data)
  {
  CERTCertificate *issuer;
  struct EVIL_issuer *issuer_data = (struct EVIL_issuer *)data;

  PORT_SetError (issuer_data->error);

  issuer = CERT_FindCertIssuer (issuer_data->cert, PR_Now(),
				certUsageObjectSigner);

  issuer_data->issuer = issuer;
  issuer_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
CERTCertificate *jar_moz_issuer (CERTCertificate *cert)
  {
  CERTCertificate *issuer_cert;
  ALLOC_OR_DEFINE(struct EVIL_issuer, issuer_data, NULL);

  issuer_data->error = PORT_GetError();
  issuer_data->cert  = cert;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_issuer_fn, issuer_data);
  else
    jar_moz_issuer_fn (issuer_data);
#else
  jar_moz_issuer_fn (issuer_data);
#endif

  PORT_SetError (issuer_data->error);
  issuer_cert = issuer_data->issuer;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(issuer_data);
  return issuer_cert;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */

/*
 *  JAR_MOZ_dup
 *
 *  Call CERT_DupCertificate inside
 *  the mozilla thread
 *
 */

struct EVIL_dup
  {
  int error;
  CERTCertificate *cert;
  CERTCertificate *return_cert;
  };


/* This is called inside the mozilla thread */

PR_STATIC_CALLBACK(void) jar_moz_dup_fn (void *data)
  {
  CERTCertificate *return_cert;
  struct EVIL_dup *dup_data = (struct EVIL_dup *)data;

  PORT_SetError (dup_data->error);

  return_cert = CERT_DupCertificate (dup_data->cert);

  dup_data->return_cert = return_cert;
  dup_data->error = PORT_GetError();
  }


/* Wrapper for the ET_MOZ call */
 
CERTCertificate *jar_moz_dup (CERTCertificate *cert)
  {
  CERTCertificate *dup_cert;
  ALLOC_OR_DEFINE(struct EVIL_dup, dup_data, NULL);

  dup_data->error = PORT_GetError();
  dup_data->cert  = cert;

  /* Synchronously invoke the callback function on the mozilla thread. */
#ifdef MOZILLA_CLIENT_OLD
  if (mozilla_event_queue)
    ET_moz_CallFunction (jar_moz_dup_fn, dup_data);
  else
    jar_moz_dup_fn (dup_data);
#else
  jar_moz_dup_fn (dup_data);
#endif

  PORT_SetError (dup_data->error);
  dup_cert = dup_data->return_cert;

  /* Free the data passed to the callback function... */
  FREE_IF_ALLOC_IS_USED(dup_data);
  return dup_cert;
  }

/* --- --- --- --- --- --- --- --- --- --- --- --- --- */
