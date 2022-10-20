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
#include <memory>

#include "cert.h"
#include "cms.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "mozilla/Likely.h"
#include "mozilla/UniquePtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "NSSErrorsService.h"
#include "pk11pub.h"
#include "pkcs12.h"
#include "prerror.h"
#include "prio.h"
#include "prmem.h"
#include "sechash.h"
#include "secmod.h"
#include "secpkcs7.h"
#include "secport.h"

#ifndef MOZ_NO_MOZALLOC
#  include "mozilla/mozalloc_oom.h"
#endif

// Normally this would be included from nsNSSComponent.h, but that file includes
// this file.
bool EnsureNSSInitializedChromeOrContent();

namespace mozilla {

// NSPR APIs use PRStatus/PR_GetError and NSS APIs use SECStatus/PR_GetError to
// report success/failure. This function makes it more convenient and *safer*
// to translate NSPR/NSS results to nsresult. It is safer because it
// refuses to translate any bad PRStatus/SECStatus into an NS_OK, even when the
// NSPR/NSS function forgot to call PR_SetError. The actual enforcement of
// this happens in mozilla::psm::GetXPCOMFromNSSError.
// IMPORTANT: This must be called immediately after the function returning the
// SECStatus result. The recommended usage is:
//    nsresult rv = MapSECStatus(f(x, y, z));
inline nsresult MapSECStatus(SECStatus rv) {
  if (rv == SECSuccess) {
    return NS_OK;
  }

  return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
}

namespace internal {

inline void PK11_DestroyContext_true(PK11Context* ctx) {
  PK11_DestroyContext(ctx, true);
}

inline void SECKEYEncryptedPrivateKeyInfo_true(
    SECKEYEncryptedPrivateKeyInfo* epki) {
  SECKEY_DestroyEncryptedPrivateKeyInfo(epki, true);
}

// If this was created via PK11_ListFixedKeysInSlot, we may have a list of keys,
// in which case we have to free them all (and if not, this will still free the
// one key).
inline void FreeOneOrMoreSymKeys(PK11SymKey* keys) {
  PK11SymKey* next;
  while (keys) {
    next = PK11_GetNextSymKey(keys);
    PK11_FreeSymKey(keys);
    keys = next;
  }
}

}  // namespace internal

// Emulates MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE, but for UniquePtrs.
#define MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(name, Type, Deleter) \
  struct name##DeletePolicy {                                      \
    void operator()(Type* aValue) { Deleter(aValue); }             \
  };                                                               \
  typedef std::unique_ptr<Type, name##DeletePolicy> name;

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11Context, PK11Context,
                                      internal::PK11_DestroyContext_true)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SlotInfo, PK11SlotInfo,
                                      PK11_FreeSlot)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SymKey, PK11SymKey,
                                      internal::FreeOneOrMoreSymKeys)

// Common base class for Digest and HMAC. Should not be used directly.
// Subclasses must implement a `Begin` function that initializes
// `mDigestContext` and calls `SetLength`.
class DigestBase {
 protected:
  explicit DigestBase() : mLen(0), mDigestContext(nullptr) {}

 public:
  nsresult Update(Span<const uint8_t> in) {
    return Update(in.Elements(), in.Length());
  }

  nsresult Update(const unsigned char* buf, const uint32_t len) {
    if (!mDigestContext) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    return MapSECStatus(PK11_DigestOp(mDigestContext.get(), buf, len));
  }

  nsresult End(/*out*/ nsTArray<uint8_t>& out) {
    if (!mDigestContext) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    out.SetLength(mLen);
    uint32_t len;
    nsresult rv = MapSECStatus(
        PK11_DigestFinal(mDigestContext.get(), out.Elements(), &len, mLen));
    NS_ENSURE_SUCCESS(rv, rv);
    mDigestContext = nullptr;
    NS_ENSURE_TRUE(len == mLen, NS_ERROR_UNEXPECTED);

    return NS_OK;
  }

 protected:
  nsresult SetLength(SECOidTag hashType) {
    switch (hashType) {
      case SEC_OID_MD5:
        mLen = MD5_LENGTH;
        break;
      case SEC_OID_SHA1:
        mLen = SHA1_LENGTH;
        break;
      case SEC_OID_SHA256:
        mLen = SHA256_LENGTH;
        break;
      case SEC_OID_SHA384:
        mLen = SHA384_LENGTH;
        break;
      case SEC_OID_SHA512:
        mLen = SHA512_LENGTH;
        break;
      default:
        return NS_ERROR_INVALID_ARG;
    }
    return NS_OK;
  }

 private:
  uint8_t mLen;

 protected:
  UniquePK11Context mDigestContext;
};

/** A more convenient way of dealing with digests calculated into
 *  stack-allocated buffers. NSS must be initialized on the main thread before
 *  use, and the caller must ensure NSS isn't shut down, typically by
 *  being within the lifetime of XPCOM.
 *
 * Typical usage, for digesting a buffer in memory:
 *
 *   nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &rv);
 *   nsTArray<uint8_t> digestArray;
 *   nsresult rv = Digest::DigestBuf(SEC_OID_SHA256, mybuffer, myBufferLen,
 *                                   digestArray);
 *   NS_ENSURE_SUCCESS(rv, rv);
 *
 * Less typical usage, for digesting while doing streaming I/O and similar:
 *
 *   Digest digest;
 *   nsresult rv = digest.Begin(SEC_OID_SHA256);
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   for (...) {
 *      rv = digest.Update(buf, len);
 *      NS_ENSURE_SUCCESS(rv, rv);
 *   }
 *   nsTArray<uint8_t> digestArray;
 *   rv = digest.End(digestArray);
 *   NS_ENSURE_SUCCESS(rv, rv)
 */
class Digest : public DigestBase {
 public:
  explicit Digest() : DigestBase() {}

  static nsresult DigestBuf(SECOidTag hashAlg, Span<const uint8_t> buf,
                            /*out*/ nsTArray<uint8_t>& out) {
    return Digest::DigestBuf(hashAlg, buf.Elements(), buf.Length(), out);
  }

  static nsresult DigestBuf(SECOidTag hashAlg, const uint8_t* buf, uint32_t len,
                            /*out*/ nsTArray<uint8_t>& out) {
    Digest digest;

    nsresult rv = digest.Begin(hashAlg);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = digest.Update(buf, len);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = digest.End(out);
    if (NS_FAILED(rv)) {
      return rv;
    }

    return rv;
  }

  nsresult Begin(SECOidTag hashAlg) {
    if (!EnsureNSSInitializedChromeOrContent()) {
      return NS_ERROR_FAILURE;
    }

    switch (hashAlg) {
      case SEC_OID_SHA1:
      case SEC_OID_SHA256:
      case SEC_OID_SHA384:
      case SEC_OID_SHA512:
        break;

      default:
        return NS_ERROR_INVALID_ARG;
    }

    mDigestContext = UniquePK11Context(PK11_CreateDigestContext(hashAlg));
    if (!mDigestContext) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    return MapSECStatus(PK11_DigestBegin(mDigestContext.get()));
  }
};

// A helper class to calculate HMACs over some data given a key.
// Only SHA256 and, sadly, MD5 are supported at the moment.
// Typical usage:
//   (ensure NSS is initialized)
//   (obtain raw bytes for a key, some data to calculate the HMAC for)
//   HMAC hmac;
//   nsresult rv = hmac.Begin(SEC_OID_SHA256, Span(key));
//   NS_ENSURE_SUCCESS(rv, rv);
//   rv = hmac.Update(buf, len);
//   NS_ENSURE_SUCCESS(rv, rv);
//   nsTArray<uint8_t> calculatedHmac;
//   rv = hmac.End(calculatedHmac);
//   NS_ENSURE_SUCCESS(rv, rv);
class HMAC : public DigestBase {
 public:
  explicit HMAC() : DigestBase() {}

  nsresult Begin(SECOidTag hashAlg, Span<const uint8_t> key) {
    if (!EnsureNSSInitializedChromeOrContent()) {
      return NS_ERROR_FAILURE;
    }
    CK_MECHANISM_TYPE mechType;
    switch (hashAlg) {
      case SEC_OID_SHA256:
        mechType = CKM_SHA256_HMAC;
        break;
      case SEC_OID_MD5:
        mechType = CKM_MD5_HMAC;
        break;
      default:
        return NS_ERROR_INVALID_ARG;
    }
    if (key.Length() > std::numeric_limits<unsigned int>::max()) {
      return NS_ERROR_INVALID_ARG;
    }
    // SECItem's data field is a non-const unsigned char*. The good news is the
    // data won't be mutated, but the bad news is the constness needs to be
    // casted away.
    SECItem keyItem = {siBuffer, const_cast<unsigned char*>(key.Elements()),
                       static_cast<unsigned int>(key.Length())};
    UniquePK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }
    UniquePK11SymKey symKey(
        PK11_ImportSymKey(slot.get(), CKM_GENERIC_SECRET_KEY_GEN,
                          PK11_OriginUnwrap, CKA_SIGN, &keyItem, nullptr));
    if (!symKey) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }
    SECItem emptyData = {siBuffer, nullptr, 0};
    mDigestContext = UniquePK11Context(PK11_CreateContextBySymKey(
        mechType, CKA_SIGN, symKey.get(), &emptyData));
    if (!mDigestContext) {
      return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
    }

    nsresult rv = SetLength(hashAlg);
    NS_ENSURE_SUCCESS(rv, rv);
    return MapSECStatus(PK11_DigestBegin(mDigestContext.get()));
  }
};

namespace internal {

inline void PORT_FreeArena_false(PLArenaPool* arena) {
  // PL_FreeArenaPool can't be used because it doesn't actually free the
  // memory, which doesn't work well with memory analysis tools.
  return PORT_FreeArena(arena, false);
}

}  // namespace internal

// Wrapper around NSS's SECItem_AllocItem that handles OOM the same way as
// other allocators.
inline void SECITEM_AllocItem(SECItem& item, uint32_t len) {
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

class ScopedAutoSECItem final : public SECItem {
 public:
  explicit ScopedAutoSECItem(uint32_t initialAllocatedLen = 0) {
    data = nullptr;
    len = 0;
    if (initialAllocatedLen > 0) {
      SECITEM_AllocItem(*this, initialAllocatedLen);
    }
  }

  void reset() { SECITEM_FreeItem(this, false); }

  ~ScopedAutoSECItem() { reset(); }
};

class MOZ_RAII AutoSECMODListReadLock final {
 public:
  AutoSECMODListReadLock() : mLock(SECMOD_GetDefaultModuleListLock()) {
    MOZ_ASSERT(mLock, "should have SECMOD lock (has NSS been initialized?)");
    SECMOD_GetReadLock(mLock);
  }

  ~AutoSECMODListReadLock() { SECMOD_ReleaseReadLock(mLock); }

 private:
  SECMODListLock* mLock;
};

namespace internal {

inline void SECITEM_FreeItem_true(SECItem* s) {
  return SECITEM_FreeItem(s, true);
}

inline void SECOID_DestroyAlgorithmID_true(SECAlgorithmID* a) {
  return SECOID_DestroyAlgorithmID(a, true);
}

inline void VFY_DestroyContext_true(VFYContext* ctx) {
  VFY_DestroyContext(ctx, true);
}

}  // namespace internal

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertificate, CERTCertificate,
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
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTCertList, CERTCertList,
                                      CERT_DestroyCertList)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTName, CERTName,
                                      CERT_DestroyName)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTOidSequence, CERTOidSequence,
                                      CERT_DestroyOidSequence)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTSubjectPublicKeyInfo,
                                      CERTSubjectPublicKeyInfo,
                                      SECKEY_DestroySubjectPublicKeyInfo)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTUserNotice, CERTUserNotice,
                                      CERT_DestroyUserNotice)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueCERTValidity, CERTValidity,
                                      CERT_DestroyValidity)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueHASHContext, HASHContext,
                                      HASH_Destroy)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueNSSCMSMessage, NSSCMSMessage,
                                      NSS_CMSMessage_Destroy)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueNSSCMSSignedData, NSSCMSSignedData,
                                      NSS_CMSSignedData_Destroy)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11GenericObject,
                                      PK11GenericObject,
                                      PK11_DestroyGenericObject)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePK11SlotList, PK11SlotList,
                                      PK11_FreeSlotList)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePLArenaPool, PLArenaPool,
                                      internal::PORT_FreeArena_false)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePORTString, char, PORT_Free)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePRFileDesc, PRFileDesc, PR_Close)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniquePRString, char, PR_Free)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECAlgorithmID, SECAlgorithmID,
                                      internal::SECOID_DestroyAlgorithmID_true)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECItem, SECItem,
                                      internal::SECITEM_FreeItem_true)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECKEYPrivateKey, SECKEYPrivateKey,
                                      SECKEY_DestroyPrivateKey)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECKEYPrivateKeyList,
                                      SECKEYPrivateKeyList,
                                      SECKEY_DestroyPrivateKeyList)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECKEYPublicKey, SECKEYPublicKey,
                                      SECKEY_DestroyPublicKey)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSECMODModule, SECMODModule,
                                      SECMOD_DestroyModule)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSGNDigestInfo, SGNDigestInfo,
                                      SGN_DestroyDigestInfo)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueVFYContext, VFYContext,
                                      internal::VFY_DestroyContext_true)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSEC_PKCS12DecoderContext,
                                      SEC_PKCS12DecoderContext,
                                      SEC_PKCS12DecoderFinish)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueSEC_PKCS12ExportContext,
                                      SEC_PKCS12ExportContext,
                                      SEC_PKCS12DestroyExportContext)
MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(
    UniqueSECKEYEncryptedPrivateKeyInfo, SECKEYEncryptedPrivateKeyInfo,
    internal::SECKEYEncryptedPrivateKeyInfo_true)
}  // namespace mozilla

#endif  // ScopedNSSTypes_h
