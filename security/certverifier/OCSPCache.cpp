/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
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

#include <limits>

#include "NSSCertDBTrustDomain.h"
#include "pk11pub.h"
#include "pkix/pkixtypes.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gCertVerifierLog;
#endif

using namespace mozilla::pkix;

namespace mozilla { namespace psm {

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
CertIDHash(SHA384Buffer& buf, const CertID& certID)
{
  ScopedPK11Context context(PK11_CreateDigestContext(SEC_OID_SHA384));
  if (!context) {
    return SECFailure;
  }
  SECStatus rv = PK11_DigestBegin(context.get());
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), certID.issuer.data, certID.issuer.len);
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), certID.issuerSubjectPublicKeyInfo.data,
                     certID.issuerSubjectPublicKeyInfo.len);
  if (rv != SECSuccess) {
    return rv;
  }
  rv = PK11_DigestOp(context.get(), certID.serialNumber.data,
                     certID.serialNumber.len);
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
OCSPCache::Entry::Init(const CertID& aCertID, PRErrorCode aErrorCode,
                       PRTime aThisUpdate, PRTime aValidThrough)
{
  mErrorCode = aErrorCode;
  mThisUpdate = aThisUpdate;
  mValidThrough = aValidThrough;
  return CertIDHash(mIDHash, aCertID);
}

OCSPCache::OCSPCache()
  : mMutex("OCSPCache-mutex")
{
}

OCSPCache::~OCSPCache()
{
  Clear();
}

// Returns false with index in an undefined state if no matching entry was
// found.
bool
OCSPCache::FindInternal(const CertID& aCertID, /*out*/ size_t& index,
                        const MutexAutoLock& /* aProofOfLock */)
{
  if (mEntries.length() == 0) {
    return false;
  }

  SHA384Buffer idHash;
  SECStatus rv = CertIDHash(idHash, aCertID);
  if (rv != SECSuccess) {
    return false;
  }

  // mEntries is sorted with the most-recently-used entry at the end.
  // Thus, searching from the end will often be fastest.
  index = mEntries.length();
  while (index > 0) {
    --index;
    if (memcmp(mEntries[index]->mIDHash, idHash, SHA384_LENGTH) == 0) {
      return true;
    }
  }
  return false;
}

static inline void
LogWithCertID(const char* aMessage, const CertID& aCertID)
{
  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, (aMessage, &aCertID));
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
OCSPCache::Get(const CertID& aCertID, PRErrorCode& aErrorCode,
               PRTime& aValidThrough)
{
  MutexAutoLock lock(mMutex);

  size_t index;
  if (!FindInternal(aCertID, index, lock)) {
    LogWithCertID("OCSPCache::Get(%p) not in cache", aCertID);
    return false;
  }
  LogWithCertID("OCSPCache::Get(%p) in cache", aCertID);
  aErrorCode = mEntries[index]->mErrorCode;
  aValidThrough = mEntries[index]->mValidThrough;
  MakeMostRecentlyUsed(index, lock);
  return true;
}

SECStatus
OCSPCache::Put(const CertID& aCertID, PRErrorCode aErrorCode,
               PRTime aThisUpdate, PRTime aValidThrough)
{
  MutexAutoLock lock(mMutex);

  size_t index;
  if (FindInternal(aCertID, index, lock)) {
    // Never replace an entry indicating a revoked certificate.
    if (mEntries[index]->mErrorCode == SEC_ERROR_REVOKED_CERTIFICATE) {
      LogWithCertID("OCSPCache::Put(%p) already in cache as revoked - "
                    "not replacing", aCertID);
      MakeMostRecentlyUsed(index, lock);
      return SECSuccess;
    }

    // Never replace a newer entry with an older one unless the older entry
    // indicates a revoked certificate, which we want to remember.
    if (mEntries[index]->mThisUpdate > aThisUpdate &&
        aErrorCode != SEC_ERROR_REVOKED_CERTIFICATE) {
      LogWithCertID("OCSPCache::Put(%p) already in cache with more recent "
                    "validity - not replacing", aCertID);
      MakeMostRecentlyUsed(index, lock);
      return SECSuccess;
    }

    // Only known good responses or responses indicating an unknown
    // or revoked certificate should replace previously known responses.
    if (aErrorCode != 0 && aErrorCode != SEC_ERROR_OCSP_UNKNOWN_CERT &&
        aErrorCode != SEC_ERROR_REVOKED_CERTIFICATE) {
      LogWithCertID("OCSPCache::Put(%p) already in cache - not replacing "
                    "with less important status", aCertID);
      MakeMostRecentlyUsed(index, lock);
      return SECSuccess;
    }

    LogWithCertID("OCSPCache::Put(%p) already in cache - replacing", aCertID);
    mEntries[index]->mErrorCode = aErrorCode;
    mEntries[index]->mThisUpdate = aThisUpdate;
    mEntries[index]->mValidThrough = aValidThrough;
    MakeMostRecentlyUsed(index, lock);
    return SECSuccess;
  }

  if (mEntries.length() == MaxEntries) {
    LogWithCertID("OCSPCache::Put(%p) too full - evicting an entry", aCertID);
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
  SECStatus rv = newEntry->Init(aCertID, aErrorCode, aThisUpdate,
                                aValidThrough);
  if (rv != SECSuccess) {
    delete newEntry;
    return rv;
  }
  mEntries.append(newEntry);
  LogWithCertID("OCSPCache::Put(%p) added to cache", aCertID);
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
