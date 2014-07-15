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

#include <limits>
#include <stdint.h>

#include "cert.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/ScopedPtr.h"
#include "prerror.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

SECStatus
VerifySignedData(const SignedDataWithSignature& sd,
                 const SECItem& subjectPublicKeyInfo, void* pkcs11PinArg)
{
  if (!sd.data.data || !sd.signature.data) {
    PR_NOT_REACHED("invalid args to VerifySignedData");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  // See bug 921585.
  if (sd.data.len > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  SECOidTag pubKeyAlg;
  SECOidTag digestAlg;
  switch (sd.algorithm) {
    case SignatureAlgorithm::ecdsa_with_sha512:
      pubKeyAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
      digestAlg = SEC_OID_SHA512;
      break;
    case SignatureAlgorithm::ecdsa_with_sha384:
      pubKeyAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
      digestAlg = SEC_OID_SHA384;
      break;
    case SignatureAlgorithm::ecdsa_with_sha256:
      pubKeyAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
      digestAlg = SEC_OID_SHA256;
      break;
    case SignatureAlgorithm::ecdsa_with_sha1:
      pubKeyAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
      digestAlg = SEC_OID_SHA1;
      break;
    case SignatureAlgorithm::rsa_pkcs1_with_sha512:
      pubKeyAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
      digestAlg = SEC_OID_SHA512;
      break;
    case SignatureAlgorithm::rsa_pkcs1_with_sha384:
      pubKeyAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
      digestAlg = SEC_OID_SHA384;
      break;
    case SignatureAlgorithm::rsa_pkcs1_with_sha256:
      pubKeyAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
      digestAlg = SEC_OID_SHA256;
      break;
    case SignatureAlgorithm::rsa_pkcs1_with_sha1:
      pubKeyAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
      digestAlg = SEC_OID_SHA1;
      break;
    case SignatureAlgorithm::dsa_with_sha256:
      pubKeyAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;
      digestAlg = SEC_OID_SHA256;
      break;
    case SignatureAlgorithm::dsa_with_sha1:
      pubKeyAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;
      digestAlg = SEC_OID_SHA1;
      break;
    default:
      PR_NOT_REACHED("unknown signature algorithm");
      PR_SetError(SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED, 0);
      return SECFailure;
  }

  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&subjectPublicKeyInfo));
  if (!spki) {
    return SECFailure;
  }
  ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
    pubKey(SECKEY_ExtractPublicKey(spki.get()));
  if (!pubKey) {
    return SECFailure;
  }

  // The static_cast is safe according to the check above that references
  // bug 921585.
  return VFY_VerifyDataDirect(sd.data.data, static_cast<int>(sd.data.len),
                              pubKey.get(), &sd.signature, pubKeyAlg,
                              digestAlg, nullptr, pkcs11PinArg);
}

SECStatus
DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  static_assert(TrustDomain::DIGEST_LENGTH == SHA1_LENGTH,
                "TrustDomain::DIGEST_LENGTH must be 20 (SHA-1 digest length)");
  if (digestBufLen != TrustDomain::DIGEST_LENGTH) {
    PR_NOT_REACHED("invalid hash length");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  if (item.len >
      static_cast<decltype(item.len)>(std::numeric_limits<int32_t>::max())) {
    PR_NOT_REACHED("large OCSP responses should have already been rejected");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  return PK11_HashBuf(SEC_OID_SHA1, digestBuf, item.data,
                      static_cast<int32_t>(item.len));
}

} } // namespace mozilla::pkix
