/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header provides smart pointers and various helpers for code that needs
// to interact with NSS.

#ifndef ScopedNSSTypes_h
#define ScopedNSSTypes_h

#include <limits>

#include "cert.h"
#include "cms.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "mozilla/Casting.h"
#include "mozilla/Likely.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "NSSErrorsService.h"
#include "pk11pub.h"
#include "pkcs12.h"
#include "prerror.h"
#include "prio.h"
#include "sechash.h"
#include "secmod.h"
#include "secpkcs7.h"
#include "secport.h"

#ifndef MOZ_NO_MOZALLOC
#include "mozilla/mozalloc_oom.h"
#endif

namespace mozilla {

// Deprecated: Use something like |mozilla::BitwiseCast<char*, uint8_t*>(p)|
//             instead.
// It is very common to cast between char* and uint8_t* when doing crypto stuff.
// Here, we provide more type-safe wrappers around reinterpret_cast so you don't
// shoot yourself in the foot by reinterpret_casting completely unrelated types.

inline char*
char_ptr_cast(uint8_t* p) { return BitwiseCast<char*>(p); }

inline const char*
char_ptr_cast(const uint8_t* p) { return BitwiseCast<const char*>(p); }

inline uint8_t*
uint8_t_ptr_cast(char* p) { return BitwiseCast<uint8_t*>(p); }

inline const uint8_t*
uint8_t_ptr_cast(const char* p) { return BitwiseCast<const uint8_t*>(p); }

// NSPR APIs use PRStatus/PR_GetError and NSS APIs use SECStatus/PR_GetError to
// report success/failure. This function makes it more convenient and *safer*
// to translate NSPR/NSS results to nsresult. It is safer because it
// refuses to translate any bad PRStatus/SECStatus into an NS_OK, even when the
// NSPR/NSS function forgot to call PR_SetError. The actual enforcement of
// this happens in mozilla::psm::GetXPCOMFromNSSError.
// IMPORTANT: This must be called immediately after the function returning the
// SECStatus result. The recommended usage is:
//    nsresult rv = MapSECStatus(f(x, y, z));
inline nsresult
MapSECStatus(SECStatus rv)
{
  if (rv == SECSuccess) {
    return NS_OK;
  }

  return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
}

// Alphabetical order by NSS type
// Deprecated: use the equivalent UniquePtr templates instead.
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
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTName,
                                          CERTName,
                                          CERT_DestroyName)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTSubjectPublicKeyInfo,
                                          CERTSubjectPublicKeyInfo,
                                          SECKEY_DestroySubjectPublicKeyInfo)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTValidity,
                                          CERTValidity,
                                          CERT_DestroyValidity)
// Deprecated: use the equivalent UniquePtr templates instead.

namespace internal {

inline void
PK11_DestroyContext_true(PK11Context * ctx) {
  PK11_DestroyContext(ctx, true);
}

} // namespace internal

// Deprecated: use the equivalent UniquePtr templates instead.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSGNDigestInfo,
                                          SGNDigestInfo,
                                          SGN_DestroyDigestInfo)

// Emulates MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE, but for UniquePtrs.
#define MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(name, Type, Deleter) \
struct name##DeletePolicy \
{ \
  void operator()(Type* aValue) { Deleter(aValue); } \
}; \
typedef UniquePtr<Type, name##DeletePolicy> name;

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11Context,
                                      PK11Context,
                                      internal::PK11_DestroyContext_true)

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
 *   UniquePK11Context digestContext(PK11_CreateDigestContext(SEC_OID_SHA256));
 *   NS_ENSURE_TRUE(digestContext, NS_ERROR_OUT_OF_MEMORY);
 *   rv = MapSECStatus(PK11_DigestBegin(digestContext.get()));
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   for (...) {
 *      rv = MapSECStatus(PK11_DigestOp(digestContext.get(), ...));
 *      NS_ENSURE_SUCCESS(rv, rv);
 *   }
 *   rv = digest.End(SEC_OID_SHA256, digestContext);
 *   NS_ENSURE_SUCCESS(rv, rv)
 */
class Digest
{
public:
  Digest()
  {
    mItem.type = siBuffer;
    mItem.data = mItemBuf;
    mItem.len = 0;
  }

  nsresult DigestBuf(SECOidTag hashAlg, const uint8_t * buf, uint32_t len)
  {
    if (len > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
      return NS_ERROR_INVALID_ARG;
    }
    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    return MapSECStatus(PK11_HashBuf(hashAlg, mItem.data, buf,
                                     static_cast<int32_t>(len)));
  }

  nsresult End(SECOidTag hashAlg, UniquePK11Context& context)
  {
    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t len;
    rv = MapSECStatus(PK11_DigestFinal(context.get(), mItem.data, &len,
                                       mItem.len));
    NS_ENSURE_SUCCESS(rv, rv);
    context = nullptr;
    NS_ENSURE_TRUE(len == mItem.len, NS_ERROR_UNEXPECTED);
    return NS_OK;
  }

  const SECItem & get() const { return mItem; }

private:
  nsresult SetLength(SECOidTag hashType)
  {
#ifdef _MSC_VER
#pragma warning(push)
    // C4061: enumerator 'symbol' in switch of enum 'symbol' is not
    // explicitly handled.
#pragma warning(disable:4061)
#endif
    switch (hashType)
    {
      case SEC_OID_SHA1:   mItem.len = SHA1_LENGTH;   break;
      case SEC_OID_SHA256: mItem.len = SHA256_LENGTH; break;
      case SEC_OID_SHA384: mItem.len = SHA384_LENGTH; break;
      case SEC_OID_SHA512: mItem.len = SHA512_LENGTH; break;
      default:
        return NS_ERROR_INVALID_ARG;
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    return NS_OK;
  }

  uint8_t mItemBuf[HASH_LENGTH_MAX];
  SECItem mItem;
};

// Deprecated: use the equivalent UniquePtr templates instead.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SlotInfo,
                                          PK11SlotInfo,
                                          PK11_FreeSlot)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SymKey,
                                          PK11SymKey,
                                          PK11_FreeSymKey)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11GenericObject,
                                          PK11GenericObject,
                                          PK11_DestroyGenericObject)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSEC_PKCS12DecoderContext,
                                          SEC_PKCS12DecoderContext,
                                          SEC_PKCS12DecoderFinish)
namespace internal {

inline void
PORT_FreeArena_false(PLArenaPool* arena)
{
  // PL_FreeArenaPool can't be used because it doesn't actually free the
  // memory, which doesn't work well with memory analysis tools.
  return PORT_FreeArena(arena, false);
}

} // namespace internal

// Deprecated: use the equivalent UniquePtr templates instead.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPLArenaPool,
                                          PLArenaPool,
                                          internal::PORT_FreeArena_false)

// Wrapper around NSS's SECItem_AllocItem that handles OOM the same way as
// other allocators.
inline void
SECITEM_AllocItem(SECItem & item, uint32_t len)
{
  if (MOZ_UNLIKELY(!SECITEM_AllocItem(nullptr, &item, len))) {
#ifndef MOZ_NO_MOZALLOC
    mozalloc_handle_oom(len);
    if (MOZ_UNLIKELY(!SECITEM_AllocItem(nullptr, &item, len)))
#endif
    {
      MOZ_CRASH();
    }
  }
}

class ScopedAutoSECItem final : public SECItem
{
public:
  explicit ScopedAutoSECItem(uint32_t initialAllocatedLen = 0)
  {
    data = nullptr;
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

namespace internal {

inline void SECITEM_FreeItem_true(SECItem * s)
{
  return SECITEM_FreeItem(s, true);
}

inline void SECOID_DestroyAlgorithmID_true(SECAlgorithmID * a)
{
  return SECOID_DestroyAlgorithmID(a, true);
}

inline void SECKEYEncryptedPrivateKeyInfo_true(SECKEYEncryptedPrivateKeyInfo * epki)
{
  return SECKEY_DestroyEncryptedPrivateKeyInfo(epki, PR_TRUE);
}

inline void VFY_DestroyContext_true(VFYContext * ctx)
{
  VFY_DestroyContext(ctx, true);
}

} // namespace internal

// Deprecated: use the equivalent UniquePtr templates instead.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECItem,
                                          SECItem,
                                          internal::SECITEM_FreeItem_true)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPrivateKey,
                                          SECKEYPrivateKey,
                                          SECKEY_DestroyPrivateKey)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYEncryptedPrivateKeyInfo,
                                          SECKEYEncryptedPrivateKeyInfo,
                                          internal::SECKEYEncryptedPrivateKeyInfo_true)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPublicKey,
                                          SECKEYPublicKey,
                                          SECKEY_DestroyPublicKey)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECAlgorithmID,
                                          SECAlgorithmID,
                                          internal::SECOID_DestroyAlgorithmID_true)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertificate,
                                      CERTCertificate,
                                      CERT_DestroyCertificate)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertificateList,
                                      CERTCertificateList,
                                      CERT_DestroyCertificateList)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertificatePolicies,
                                      CERTCertificatePolicies,
                                      CERT_DestroyCertificatePoliciesExtension)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertificateRequest,
                                      CERTCertificateRequest,
                                      CERT_DestroyCertificateRequest)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertList,
                                      CERTCertList,
                                      CERT_DestroyCertList)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertNicknames,
                                      CERTCertNicknames,
                                      CERT_FreeNicknames)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTName,
                                      CERTName,
                                      CERT_DestroyName)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTOidSequence,
                                      CERTOidSequence,
                                      CERT_DestroyOidSequence)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTSubjectPublicKeyInfo,
                                      CERTSubjectPublicKeyInfo,
                                      SECKEY_DestroySubjectPublicKeyInfo)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTUserNotice,
                                      CERTUserNotice,
                                      CERT_DestroyUserNotice)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTValidity,
                                      CERTValidity,
                                      CERT_DestroyValidity)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueNSSCMSMessage,
                                      NSSCMSMessage,
                                      NSS_CMSMessage_Destroy)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueNSSCMSSignedData,
                                      NSSCMSSignedData,
                                      NSS_CMSSignedData_Destroy)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SlotInfo,
                                      PK11SlotInfo,
                                      PK11_FreeSlot)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SlotList,
                                      PK11SlotList,
                                      PK11_FreeSlotList)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SymKey,
                                      PK11SymKey,
                                      PK11_FreeSymKey)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePLArenaPool,
                                      PLArenaPool,
                                      internal::PORT_FreeArena_false)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePORTString,
                                      char,
                                      PORT_Free);
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePRFileDesc,
                                      PRFileDesc,
                                      PR_Close)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECItem,
                                      SECItem,
                                      internal::SECITEM_FreeItem_true)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECKEYPrivateKey,
                                      SECKEYPrivateKey,
                                      SECKEY_DestroyPrivateKey)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECKEYPublicKey,
                                      SECKEYPublicKey,
                                      SECKEY_DestroyPublicKey)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECMODModule,
                                      SECMODModule,
                                      SECMOD_DestroyModule)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueVFYContext,
                                      VFYContext,
                                      internal::VFY_DestroyContext_true)
} // namespace mozilla

#endif // ScopedNSSTypes_h
