/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "OCSPCache.h"

#include "NSSCertDBTrustDomain.h"
#include "pk11pub.h"
#include "secerr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gCertVerifierLog;
#endif

namespace mozilla { namespace psm {

void
Insanity_PK11_DestroyContext_true(PK11Context* context)
{
  PK11_DestroyContext(context, true);
}

typedef insanity::pkix::ScopedPtr<PK11Context,
                                  Insanity_PK11_DestroyContext_true>
                                  ScopedPK11Context;

// Let derIssuer be the DER encoding of the issuer of aCert.
// Let derPublicKey be the DER encoding of the public key of aIssuerCert.
// Let serialNumber be the bytes of the serial number of aCert.
// The value calculated is SHA384(derIssuer || derPublicKey || serialNumber).
// Because the DER encodings include the length of the data encoded,
// there do not exist A(derIssuerA, derPublicKeyA, serialNumberA) and
// B(derIssuerB, derPublicKeyB, serialNumberB) such that the concatenation of
// each triplet results in the same string of bytes but where each part in A is
// not equal to its counterpart in B. This is important because as a result it
// is computationally infeasible to find collisions that would subvert this
// cache (given that SHA384 is a cryptographically-secure hash function).
static SECStatus
CertIDHash(SHA384Buffer& buf, const CERTCertificate* aCert,
       const CERTCertificate* aIssuerCert)
{
  ScopedPK11Context context(PK11_CreateDigestContext(SEC_OID_SHA384));
  if (!context) {
    return SECFailure;
  }
  SECStatus rv = PK11_DigestBegin(context.get());
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), aCert->derIssuer.data,
                     aCert->derIssuer.len);
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), aIssuerCert->derPublicKey.data,
                     aIssuerCert->derPublicKey.len);
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), aCert->serialNumber.data,
                     aCert->serialNumber.len);
  if (rv != SECSuccess) {
    return rv;
  }
  uint32_t outLen = 0;
  rv = PK11_DigestFinal(context.get(), buf, &outLen, SHA384_LENGTH);
  if (outLen != SHA384_LENGTH) {
    return SECFailure;
  }
  return rv;
}

SECStatus
OCSPCache::Entry::Init(const CERTCertificate* aCert,
                       const CERTCertificate* aIssuerCert,
                       PRErrorCode aErrorCode,
                       PRTime aThisUpdate,
                       PRTime aValidThrough)
{
  mErrorCode = aErrorCode;
  mThisUpdate = aThisUpdate;
  mValidThrough = aValidThrough;
  return CertIDHash(mIDHash, aCert, aIssuerCert);
}

OCSPCache::OCSPCache()
  : mMutex("OCSPCache-mutex")
{
}

OCSPCache::~OCSPCache()
{
  Clear();
}

// Returns -1 if no entry is found for the given (cert, issuer) pair.
int32_t
OCSPCache::FindInternal(const CERTCertificate* aCert,
                        const CERTCertificate* aIssuerCert,
                        const MutexAutoLock& /* aProofOfLock */)
{
  if (mEntries.length() == 0) {
    return -1;
  }

  SHA384Buffer idHash;
  SECStatus rv = CertIDHash(idHash, aCert, aIssuerCert);
  if (rv != SECSuccess) {
    return -1;
  }

  // mEntries is sorted with the most-recently-used entry at the end.
  // Thus, searching from the end will often be fastest.
  for (int32_t i = mEntries.length() - 1; i >= 0; i--) {
    if (memcmp(mEntries[i]->mIDHash, idHash, SHA384_LENGTH) == 0) {
      return i;
    }
  }
  return -1;
}

void
OCSPCache::LogWithCerts(const char* aMessage, const CERTCertificate* aCert,
                        const CERTCertificate* aIssuerCert)
{
#ifdef PR_LOGGING
  if (PR_LOG_TEST(gCertVerifierLog, PR_LOG_DEBUG)) {
    insanity::pkix::ScopedPtr<char, mozilla::psm::PORT_Free_string>
      cn(CERT_GetCommonName(&aCert->subject));
    insanity::pkix::ScopedPtr<char, mozilla::psm::PORT_Free_string>
      cnIssuer(CERT_GetCommonName(&aIssuerCert->subject));
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, (aMessage, cn.get(), cnIssuer.get()));
  }
#endif
}

void
OCSPCache::MakeMostRecentlyUsed(size_t aIndex,
                                const MutexAutoLock& /* aProofOfLock */)
{
  Entry* entry = mEntries[aIndex];
  // Since mEntries is sorted with the most-recently-used entry at the end,
  // aIndex is likely to be near the end, so this is likely to be fast.
  mEntries.erase(mEntries.begin() + aIndex);
  mEntries.append(entry);
}

bool
OCSPCache::Get(const CERTCertificate* aCert,
               const CERTCertificate* aIssuerCert,
               PRErrorCode& aErrorCode,
               PRTime& aValidThrough)
{
  PR_ASSERT(aCert);
  PR_ASSERT(aIssuerCert);

  MutexAutoLock lock(mMutex);

  int32_t index = FindInternal(aCert, aIssuerCert, lock);
  if (index < 0) {
    LogWithCerts("OCSPCache::Get(%s, %s) not in cache", aCert, aIssuerCert);
    return false;
  }
  LogWithCerts("OCSPCache::Get(%s, %s) in cache", aCert, aIssuerCert);
  aErrorCode = mEntries[index]->mErrorCode;
  aValidThrough = mEntries[index]->mValidThrough;
  MakeMostRecentlyUsed(index, lock);
  return true;
}

SECStatus
OCSPCache::Put(const CERTCertificate* aCert,
               const CERTCertificate* aIssuerCert,
               PRErrorCode aErrorCode,
               PRTime aThisUpdate,
               PRTime aValidThrough)
{
  PR_ASSERT(aCert);
  PR_ASSERT(aIssuerCert);

  MutexAutoLock lock(mMutex);

  int32_t index = FindInternal(aCert, aIssuerCert, lock);

  if (index >= 0) {
    // Never replace an entry indicating a revoked certificate.
    if (mEntries[index]->mErrorCode == SEC_ERROR_REVOKED_CERTIFICATE) {
      LogWithCerts("OCSPCache::Put(%s, %s) already in cache as revoked - "
                   "not replacing", aCert, aIssuerCert);
      MakeMostRecentlyUsed(index, lock);
      return SECSuccess;
    }

    // Never replace a newer entry with an older one unless the older entry
    // indicates a revoked certificate, which we want to remember.
    if (mEntries[index]->mThisUpdate > aThisUpdate &&
        aErrorCode != SEC_ERROR_REVOKED_CERTIFICATE) {
      LogWithCerts("OCSPCache::Put(%s, %s) already in cache with more recent "
                   "validity - not replacing", aCert, aIssuerCert);
      MakeMostRecentlyUsed(index, lock);
      return SECSuccess;
    }

    LogWithCerts("OCSPCache::Put(%s, %s) already in cache - replacing",
                 aCert, aIssuerCert);
    mEntries[index]->mErrorCode = aErrorCode;
    mEntries[index]->mThisUpdate = aThisUpdate;
    mEntries[index]->mValidThrough = aValidThrough;
    MakeMostRecentlyUsed(index, lock);
    return SECSuccess;
  }

  if (mEntries.length() == MaxEntries) {
    LogWithCerts("OCSPCache::Put(%s, %s) too full - evicting an entry", aCert,
                 aIssuerCert);
    for (Entry** toEvict = mEntries.begin(); toEvict != mEntries.end();
         toEvict++) {
      // Never evict an entry that indicates a revoked or unknokwn certificate,
      // because revoked responses are more security-critical to remember.
      if ((*toEvict)->mErrorCode != SEC_ERROR_REVOKED_CERTIFICATE &&
          (*toEvict)->mErrorCode != SEC_ERROR_OCSP_UNKNOWN_CERT) {
        delete *toEvict;
        mEntries.erase(toEvict);
        break;
      }
    }
    // Well, we tried, but apparently everything is revoked or unknown.
    // We don't want to remove a cached revoked or unknown response. If we're
    // trying to insert a good response, we can just return "successfully"
    // without doing so. This means we'll lose some speed, but it's not a
    // security issue. If we're trying to insert a revoked or unknown response,
    // we can't. We should return with an error that causes the current
    // verification to fail.
    if (mEntries.length() == MaxEntries) {
      if (aErrorCode != 0) {
        PR_SetError(aErrorCode, 0);
        return SECFailure;
      }
      return SECSuccess;
    }
  }

  Entry* newEntry = new Entry();
  // Normally we don't have to do this in Gecko, because OOM is fatal.
  // However, if we want to embed this in another project, OOM might not
  // be fatal, so handle this case.
  if (!newEntry) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }
  SECStatus rv = newEntry->Init(aCert, aIssuerCert, aErrorCode, aThisUpdate,
                                aValidThrough);
  if (rv != SECSuccess) {
    return rv;
  }
  mEntries.append(newEntry);
  LogWithCerts("OCSPCache::Put(%s, %s) added to cache", aCert, aIssuerCert);
  return SECSuccess;
}

void
OCSPCache::Clear()
{
  MutexAutoLock lock(mMutex);
  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("OCSPCache::Clear: clearing cache"));
  // First go through and delete the memory being pointed to by the pointers
  // in the vector.
  for (Entry** entry = mEntries.begin(); entry < mEntries.end();
       entry++) {
    delete *entry;
  }
  // Then remove the pointers themselves.
  mEntries.clearAndFree();
}

} } // namespace mozilla::psm
