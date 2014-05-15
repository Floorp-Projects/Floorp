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

#ifndef mozilla_psm_OCSPCache_h
#define mozilla_psm_OCSPCache_h

#include "certt.h"
#include "hasht.h"
#include "pkix/pkixtypes.h"
#include "mozilla/Mutex.h"
#include "mozilla/Vector.h"
#include "prerror.h"

namespace mozilla { namespace psm {

// make SHA384Buffer be of type "array of uint8_t of length SHA384_LENGTH"
typedef uint8_t SHA384Buffer[SHA384_LENGTH];

// OCSPCache can store and retrieve OCSP response verification results. Each
// result is keyed on the certificate that purportedly corresponds to it (where
// certificates are distinguished based on serial number, issuer, and
// issuer public key, much like in an encoded OCSP response itself). A maximum
// of 1024 distinct entries can be stored.
// OCSPCache is thread-safe.
class OCSPCache
{
public:
  OCSPCache();
  ~OCSPCache();

  // Returns true if the status of the given certificate (issued by the given
  // issuer) is in the cache, and false otherwise.
  // If it is in the cache, returns by reference the error code of the cached
  // status and the time through which the status is considered trustworthy.
  bool Get(const CERTCertificate* aCert, const CERTCertificate* aIssuerCert,
           /* out */ PRErrorCode& aErrorCode, /* out */ PRTime& aValidThrough);

  // Caches the status of the given certificate (issued by the given issuer).
  // The status is considered trustworthy through the given time.
  // A status with an error code of SEC_ERROR_REVOKED_CERTIFICATE will not
  // be replaced or evicted.
  // A status with an error code of SEC_ERROR_OCSP_UNKNOWN_CERT will not
  // be evicted when the cache is full.
  // A status with a more recent thisUpdate will not be replaced with a
  // status with a less recent thisUpdate unless the less recent status
  // indicates the certificate is revoked.
  SECStatus Put(const CERTCertificate* aCert,
                const CERTCertificate* aIssuerCert,
                PRErrorCode aErrorCode,
                PRTime aThisUpdate,
                PRTime aValidThrough);

  // Removes everything from the cache.
  void Clear();

private:
  class Entry
  {
  public:
    SECStatus Init(const CERTCertificate* aCert,
                   const CERTCertificate* aIssuerCert,
                   PRErrorCode aErrorCode, PRTime aThisUpdate,
                   PRTime aValidThrough);

    PRErrorCode mErrorCode;
    PRTime mThisUpdate;
    PRTime mValidThrough;
    // The SHA-384 hash of the concatenation of the DER encodings of the
    // issuer name and issuer key, followed by the serial number.
    // See the documentation for CertIDHash in OCSPCache.cpp.
    SHA384Buffer mIDHash;
  };

  bool FindInternal(const CERTCertificate* aCert,
                    const CERTCertificate* aIssuerCert,
                    /*out*/ size_t& index,
                    const MutexAutoLock& aProofOfLock);
  void MakeMostRecentlyUsed(size_t aIndex, const MutexAutoLock& aProofOfLock);
  void LogWithCerts(const char* aMessage, const CERTCertificate* aCert,
                    const CERTCertificate* aIssuerCert);

  Mutex mMutex;
  static const size_t MaxEntries = 1024;
  // Sorted with the most-recently-used entry at the end.
  // Using 256 here reserves as much possible inline storage as the vector
  // implementation will give us. 1024 bytes is the maximum it allows,
  // which results in 256 Entry pointers or 128 Entry pointers, depending
  // on the size of a pointer.
  Vector<Entry*, 256> mEntries;
};

} } // namespace mozilla::psm

#endif // mozilla_psm_OCSPCache_h
