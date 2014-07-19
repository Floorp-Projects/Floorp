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

#ifndef mozilla_pkix_test__pkixtestutils_h
#define mozilla_pkix_test__pkixtestutils_h

#include <stdint.h>
#include <stdio.h>

#include "cert.h"
#include "keyhi.h"
#include "pkix/enumclass.h"
#include "pkix/pkixtypes.h"
#include "pkix/ScopedPtr.h"
#include "seccomon.h"

namespace mozilla { namespace pkix { namespace test {

class TestInputBuffer : public InputBuffer
{
public:
  template <size_t N>
  explicit TestInputBuffer(const char (&valueString)[N])
    : InputBuffer(reinterpret_cast<const uint8_t(&)[N-1]>(valueString))
  {
  }
};

namespace {

inline void
fclose_void(FILE* file) {
  (void) fclose(file);
}

inline void
SECITEM_FreeItem_true(SECItem* item)
{
  SECITEM_FreeItem(item, true);
}

} // unnamed namespace

typedef ScopedPtr<CERTCertificate, CERT_DestroyCertificate> ScopedCERTCertificate;
typedef ScopedPtr<CERTCertList, CERT_DestroyCertList> ScopedCERTCertList;
typedef mozilla::pkix::ScopedPtr<FILE, fclose_void> ScopedFILE;
typedef mozilla::pkix::ScopedPtr<SECItem, SECITEM_FreeItem_true> ScopedSECItem;
typedef mozilla::pkix::ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
  ScopedSECKEYPublicKey;
typedef mozilla::pkix::ScopedPtr<SECKEYPrivateKey, SECKEY_DestroyPrivateKey>
  ScopedSECKEYPrivateKey;

FILE* OpenFile(const char* dir, const char* filename, const char* mode);

extern const PRTime ONE_DAY;

// e.g. YMDHMS(2016, 12, 31, 1, 23, 45) => 2016-12-31:01:23:45 (GMT)
PRTime YMDHMS(int16_t year, int16_t month, int16_t day,
              int16_t hour, int16_t minutes, int16_t seconds);

SECStatus GenerateKeyPair(/*out*/ ScopedSECKEYPublicKey& publicKey,
                          /*out*/ ScopedSECKEYPrivateKey& privateKey);

// The result will be owned by the arena
const SECItem* ASCIIToDERName(PLArenaPool* arena, const char* cn);

// Replace one substring in item with another of the same length, but only if
// the substring was found exactly once. The "only once" restriction is helpful
// for avoiding making multiple changes at once.
//
// The string to search for must be 8 or more bytes long so that it is
// extremely unlikely that there will ever be any false positive matches
// in digital signatures, keys, hashes, etc.
SECStatus TamperOnce(SECItem& item, const uint8_t* from, size_t fromLen,
                     const uint8_t* to, size_t toLen);

Result InitInputBufferFromSECItem(const SECItem* secItem,
                                  /*out*/ InputBuffer& inputBuffer);

///////////////////////////////////////////////////////////////////////////////
// Encode Certificates

enum Version { v1 = 0, v2 = 1, v3 = 2 };

// serialNumber is assumed to be the DER encoding of an INTEGER.
//
// If extensions is null, then no extensions will be encoded. Otherwise,
// extensions must point to a null-terminated array of SECItem*. If the first
// item of the array is null then an empty Extensions sequence will be encoded.
//
// If issuerPrivateKey is null, then the certificate will be self-signed.
// Parameter order is based on the order of the attributes of the certificate
// in RFC 5280.
//
// The return value, if non-null, is owned by the arena in the context and
// MUST NOT be freed.
SECItem* CreateEncodedCertificate(PLArenaPool* arena, long version,
                                  SECOidTag signature,
                                  const SECItem* serialNumber,
                                  const SECItem* issuerNameDER,
                                  PRTime notBefore, PRTime notAfter,
                                  const SECItem* subjectNameDER,
                     /*optional*/ SECItem const* const* extensions,
                     /*optional*/ SECKEYPrivateKey* issuerPrivateKey,
                                  SECOidTag signatureHashAlg,
                          /*out*/ ScopedSECKEYPrivateKey& privateKey);

SECItem* CreateEncodedSerialNumber(PLArenaPool* arena, long value);

MOZILLA_PKIX_ENUM_CLASS ExtensionCriticality { NotCritical = 0, Critical = 1 };

// The return value, if non-null, is owned by the arena and MUST NOT be freed.
SECItem* CreateEncodedBasicConstraints(PLArenaPool* arena, bool isCA,
                                       /*optional*/ long* pathLenConstraint,
                                       ExtensionCriticality criticality);

// ekus must be non-null and must must point to a SEC_OID_UNKNOWN-terminated
// array of SECOidTags. If the first item of the array is SEC_OID_UNKNOWN then
// an empty EKU extension will be encoded.
//
// The return value, if non-null, is owned by the arena and MUST NOT be freed.
SECItem* CreateEncodedEKUExtension(PLArenaPool* arena,
                                   const SECOidTag* ekus, size_t ekusCount,
                                   ExtensionCriticality criticality);

///////////////////////////////////////////////////////////////////////////////
// Encode OCSP responses

class OCSPResponseExtension
{
public:
  SECItem id;
  bool critical;
  SECItem value;
  OCSPResponseExtension* next;
};

class OCSPResponseContext
{
public:
  OCSPResponseContext(PLArenaPool* arena, const CertID& certID, PRTime time);

  PLArenaPool* arena;
  const CertID& certID;
  // TODO(bug 980538): add a way to specify what certificates are included.

  // The fields below are in the order that they appear in an OCSP response.

  // By directly using the issuer name & SPKI and signer name & private key,
  // instead of extracting those things out of CERTCertificate objects, we
  // avoid poor interactions with the NSS CERTCertificate caches. In
  // particular, there are some tests in which it is important that we know
  // that the issuer and/or signer certificates are NOT in the NSS caches
  // because we ant to make sure that our path building logic will find them
  // or we want to test what happens when those certificates cannot be found.
  // This concern doesn't apply to |cert| above because our verification code
  // for certificate chains and for OCSP responses take the end-entity cert
  // as a CERTCertificate anyway.

  enum OCSPResponseStatus {
    successful = 0,
    malformedRequest = 1,
    internalError = 2,
    tryLater = 3,
    // 4 is not used
    sigRequired = 5,
    unauthorized = 6,
  };
  uint8_t responseStatus; // an OCSPResponseStatus or an invalid value
  bool skipResponseBytes; // If true, don't include responseBytes

  // responderID
  const SECItem* signerNameDER; // If set, responderID will use the byName
                                // form; otherwise responderID will use the
                                // byKeyHash form.

  PRTime producedAt;

  OCSPResponseExtension* extensions;
  bool includeEmptyExtensions; // If true, include the extension wrapper
                               // regardless of if there are any actual
                               // extensions.
  ScopedSECKEYPrivateKey signerPrivateKey;
  bool badSignature; // If true, alter the signature to fail verification
  SECItem const* const* certs; // non-owning pointer to certs to embed

  // The following fields are on a per-SingleResponse basis. In the future we
  // may support including multiple SingleResponses per response.
  SECOidTag certIDHashAlg;
  enum CertStatus {
    good = 0,
    revoked = 1,
    unknown = 2,
  };
  uint8_t certStatus; // CertStatus or an invalid value
  PRTime revocationTime; // For certStatus == revoked
  PRTime thisUpdate;
  PRTime nextUpdate;
  bool includeNextUpdate;
};

// The return value, if non-null, is owned by the arena in the context
// and MUST NOT be freed.
// This function does its best to respect the NSPR error code convention
// (that is, if it returns null, calling PR_GetError() will return the
// error of the failed operation). However, this is not guaranteed.
SECItem* CreateEncodedOCSPResponse(OCSPResponseContext& context);

} } } // namespace mozilla::pkix::test

#endif // mozilla_pkix_test__pkixtestutils_h
