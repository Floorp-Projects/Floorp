/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScopedNSSTypes_h
#define mozilla_ScopedNSSTypes_h

#include "mozilla/Likely.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/Scoped.h"
#include "nsError.h"
#include "nsDebug.h"
#include "prio.h"
#include "cert.h"
#include "cms.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "sechash.h"
#include "secpkcs7.h"
#include "prerror.h"
#include "ocsp.h"

namespace mozilla {

// It is very common to cast between char* and uint8_t* when doing crypto stuff.
// Here, we provide more type-safe wrappers around reinterpret_cast so you don't
// shoot yourself in the foot by reinterpret_casting completely unrelated types.

inline char *
char_ptr_cast(uint8_t * p) { return reinterpret_cast<char *>(p); }

inline const char *
char_ptr_cast(const uint8_t * p) { return reinterpret_cast<const char *>(p); }

inline uint8_t *
uint8_t_ptr_cast(char * p) { return reinterpret_cast<uint8_t*>(p); }

inline const uint8_t *
uint8_t_ptr_cast(const char * p) { return reinterpret_cast<const uint8_t*>(p); }

// NSPR APIs use PRStatus/PR_GetError and NSS APIs use SECStatus/PR_GetError to
// report success/failure. These funtions make it more convenient and *safer*
// to translate NSPR/NSS results to nsresult. They are safer because they
// refuse to traslate any bad PRStatus/SECStatus into an NS_OK, even when the
// NSPR/NSS function forgot to call PR_SetError.

// IMPORTANT: This must be called immediately after the function that set the
// error code. Prefer using MapSECStatus to this.
inline nsresult
PRErrorCode_to_nsresult(PRErrorCode error)
{
  if (!error) {
    MOZ_CRASH("Function failed without calling PR_GetError");
  }

  // From NSSErrorsService::GetXPCOMFromNSSError
  // XXX Don't make up nsresults, it's supposed to be an enum (bug 778113)
  return (nsresult)NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY,
                                             -1 * error);
}

// IMPORTANT: This must be called immediately after the function returning the
// SECStatus result. The recommended usage is:
//    nsresult rv = MapSECStatus(f(x, y, z));
inline nsresult
MapSECStatus(SECStatus rv)
{
  if (rv == SECSuccess)
    return NS_OK;

  PRErrorCode error = PR_GetError();
  return PRErrorCode_to_nsresult(error);
}

// Alphabetical order by NSS type
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc,
                                          PRFileDesc,
                                          PR_Close)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificate,
                                          CERTCertificate,
                                          CERT_DestroyCertificate)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificateList,
                                          CERTCertificateList,
                                          CERT_DestroyCertificateList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificateRequest,
                                          CERTCertificateRequest,
                                          CERT_DestroyCertificateRequest)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertList,
                                          CERTCertList,
                                          CERT_DestroyCertList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTName,
                                          CERTName,
                                          CERT_DestroyName)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertNicknames,
                                          CERTCertNicknames,
                                          CERT_FreeNicknames)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTOCSPCertID,
                                          CERTOCSPCertID,
                                          CERT_DestroyOCSPCertID)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTSubjectPublicKeyInfo,
                                          CERTSubjectPublicKeyInfo,
                                          SECKEY_DestroySubjectPublicKeyInfo)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTValidity,
                                          CERTValidity,
                                          CERT_DestroyValidity)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedNSSCMSMessage,
                                          NSSCMSMessage,
                                          NSS_CMSMessage_Destroy)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedNSSCMSSignedData,
                                          NSSCMSSignedData,
                                          NSS_CMSSignedData_Destroy)

namespace psm {

inline void
PK11_DestroyContext_true(PK11Context * ctx) {
  PK11_DestroyContext(ctx, true);
}

} // namespace mozilla::psm

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11Context,
                                          PK11Context,
                                          mozilla::psm::PK11_DestroyContext_true)

/** A more convenient way of dealing with digests calculated into
 *  stack-allocated buffers. NSS must be initialized on the main thread before
 *  use, and the caller must ensure NSS isn't shut down, typically by
 *  subclassing nsNSSShutDownObject, while Digest is in use.
 *
 * Typical usage, for digesting a buffer in memory:
 *
 *   nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &rv);
 *   Digest digest;
 *   nsresult rv = digest.DigestBuf(SEC_OID_SHA256, mybuffer, myBufferLen);
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   rv = MapSECStatus(SomeNSSFunction(..., digest.get(), ...));
 *
 * Less typical usage, for digesting while doing streaming I/O and similar:
 *
 *   Digest digest;
 *   ScopedPK11Context digestContext(PK11_CreateDigestContext(SEC_OID_SHA1));
 *   NS_ENSURE_TRUE(digestContext, NS_ERROR_OUT_OF_MEMORY);
 *   rv = MapSECStatus(PK11_DigestBegin(digestContext));
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   for (...) {
 *      rv = MapSECStatus(PK11_DigestOp(digestContext, ...));
 *      NS_ENSURE_SUCCESS(rv, rv);
 *   }
 *   rv = digest.End(SEC_OID_SHA1, digestContext);
 *   NS_ENSURE_SUCCESS(rv, rv)
 */
class Digest
{
public:
  Digest()
  {
    item.type = siBuffer;
    item.data = buf;
    item.len = 0;
  }

  nsresult DigestBuf(SECOidTag hashAlg, const uint8_t * buf, uint32_t len)
  {
    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    return MapSECStatus(PK11_HashBuf(hashAlg, item.data, buf, len));
  }

  nsresult End(SECOidTag hashAlg, ScopedPK11Context & context)
  {
    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t len;
    rv = MapSECStatus(PK11_DigestFinal(context, item.data, &len, item.len));
    NS_ENSURE_SUCCESS(rv, rv);
    context = nullptr;
    NS_ENSURE_TRUE(len == item.len, NS_ERROR_UNEXPECTED);
    return NS_OK;
  }

  const SECItem & get() const { return item; }

private:
  nsresult SetLength(SECOidTag hashType)
  {
    switch (hashType)
    {
      case SEC_OID_SHA1:   item.len = SHA1_LENGTH;   break;
      case SEC_OID_SHA256: item.len = SHA256_LENGTH; break;
      case SEC_OID_SHA384: item.len = SHA384_LENGTH; break;
      case SEC_OID_SHA512: item.len = SHA512_LENGTH; break;
      default:
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
  }

  uint8_t buf[HASH_LENGTH_MAX];
  SECItem item;
};

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SlotInfo,
                                          PK11SlotInfo,
                                          PK11_FreeSlot)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SlotList,
                                          PK11SlotList,
                                          PK11_FreeSlotList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SymKey,
                                          PK11SymKey,
                                          PK11_FreeSymKey)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSEC_PKCS7ContentInfo,
                                          SEC_PKCS7ContentInfo,
                                          SEC_PKCS7DestroyContentInfo)

// Wrapper around NSS's SECItem_AllocItem that handles OOM the same way as
// other allocators.
inline void
SECITEM_AllocItem(SECItem & item, uint32_t len)
{
  if (MOZ_UNLIKELY(!SECITEM_AllocItem(nullptr, &item, len))) {
    mozalloc_handle_oom(len);
    if (MOZ_UNLIKELY(!SECITEM_AllocItem(nullptr, &item, len))) {
      MOZ_CRASH();
    }
  }
}

class ScopedAutoSECItem MOZ_FINAL : public SECItem
{
public:
  ScopedAutoSECItem(uint32_t initialAllocatedLen = 0)
  {
    data = NULL;
    len = 0;
    if (initialAllocatedLen > 0) {
      SECITEM_AllocItem(*this, initialAllocatedLen);
    }
  }

  void reset()
  {
    SECITEM_FreeItem(this, false);
  }

  ~ScopedAutoSECItem()
  {
    reset();
  }
};

namespace psm {

inline void SECITEM_FreeItem_true(SECItem * s)
{
  return SECITEM_FreeItem(s, true);
}

} // namespace impl

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECItem,
                                          ::SECItem,
                                          ::mozilla::psm::SECITEM_FreeItem_true)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPrivateKey,
                                          SECKEYPrivateKey,
                                          SECKEY_DestroyPrivateKey)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPublicKey,
                                          SECKEYPublicKey,
                                          SECKEY_DestroyPublicKey)

} // namespace mozilla

#endif // mozilla_ScopedNSSTypes_h
