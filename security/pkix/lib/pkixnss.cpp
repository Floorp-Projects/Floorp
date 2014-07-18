/*- *- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "pkix/pkixnss.h"

#include <limits>
#include <stdint.h>

#include "cert.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/ScopedPtr.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

typedef ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey> ScopedSECKeyPublicKey;

Result
CheckPublicKeySize(const SECItem& subjectPublicKeyInfo,
                   /*out*/ ScopedSECKeyPublicKey& publicKey)
{
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&subjectPublicKeyInfo));
  if (!spki) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  publicKey = SECKEY_ExtractPublicKey(spki.get());
  if (!publicKey) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  static const unsigned int MINIMUM_NON_ECC_BITS = 1024;

  switch (publicKey.get()->keyType) {
    case ecKey:
      // TODO(bug 622859): We should check which curve.
      return Success;
    case dsaKey: // fall through
    case rsaKey:
      // TODO(bug 622859): Enforce a minimum of 2048 bits for EV certs.
      if (SECKEY_PublicKeyStrengthInBits(publicKey.get()) < MINIMUM_NON_ECC_BITS) {
        // TODO(bug 1031946): Create a new error code.
        return Result::ERROR_INVALID_KEY;
      }
      break;
    case nullKey:
    case fortezzaKey:
    case dhKey:
    case keaKey:
    case rsaPssKey:
    case rsaOaepKey:
    default:
      return Result::ERROR_UNSUPPORTED_KEYALG;
  }

  return Success;
}

Result
CheckPublicKey(const SECItem& subjectPublicKeyInfo)
{
  ScopedSECKeyPublicKey unused;
  return CheckPublicKeySize(subjectPublicKeyInfo, unused);
}

Result
VerifySignedData(const SignedDataWithSignature& sd,
                 const SECItem& subjectPublicKeyInfo, void* pkcs11PinArg)
{
  if (!sd.data.data || !sd.signature.data) {
    PR_NOT_REACHED("invalid args to VerifySignedData");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  // See bug 921585.
  if (sd.data.len > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    return Result::FATAL_ERROR_INVALID_ARGS;
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
      return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  }

  Result rv;
  ScopedSECKeyPublicKey pubKey;
  rv = CheckPublicKeySize(subjectPublicKeyInfo, pubKey);
  if (rv != Success) {
    return rv;
  }

  // The static_cast is safe according to the check above that references
  // bug 921585.
  SECStatus srv = VFY_VerifyDataDirect(sd.data.data,
                                       static_cast<int>(sd.data.len),
                                       pubKey.get(), &sd.signature, pubKeyAlg,
                                       digestAlg, nullptr, pkcs11PinArg);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  return Success;
}

Result
DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  static_assert(TrustDomain::DIGEST_LENGTH == SHA1_LENGTH,
                "TrustDomain::DIGEST_LENGTH must be 20 (SHA-1 digest length)");
  if (digestBufLen != TrustDomain::DIGEST_LENGTH) {
    PR_NOT_REACHED("invalid hash length");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (item.len >
      static_cast<decltype(item.len)>(std::numeric_limits<int32_t>::max())) {
    PR_NOT_REACHED("large OCSP responses should have already been rejected");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  SECStatus srv = PK11_HashBuf(SEC_OID_SHA1, digestBuf, item.data,
                               static_cast<int32_t>(item.len));
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  return Success;
}

#define MAP_LIST \
    MAP(Result::Success, 0) \
    MAP(Result::ERROR_BAD_DER, SEC_ERROR_BAD_DER) \
    MAP(Result::ERROR_CA_CERT_INVALID, SEC_ERROR_CA_CERT_INVALID) \
    MAP(Result::ERROR_BAD_SIGNATURE, SEC_ERROR_BAD_SIGNATURE) \
    MAP(Result::ERROR_CERT_BAD_ACCESS_LOCATION, SEC_ERROR_CERT_BAD_ACCESS_LOCATION) \
    MAP(Result::ERROR_CERT_NOT_IN_NAME_SPACE, SEC_ERROR_CERT_NOT_IN_NAME_SPACE) \
    MAP(Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED, SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED) \
    MAP(Result::ERROR_CONNECT_REFUSED, PR_CONNECT_REFUSED_ERROR) \
    MAP(Result::ERROR_EXPIRED_CERTIFICATE, SEC_ERROR_EXPIRED_CERTIFICATE) \
    MAP(Result::ERROR_EXTENSION_VALUE_INVALID, SEC_ERROR_EXTENSION_VALUE_INVALID) \
    MAP(Result::ERROR_INADEQUATE_CERT_TYPE, SEC_ERROR_INADEQUATE_CERT_TYPE) \
    MAP(Result::ERROR_INADEQUATE_KEY_USAGE, SEC_ERROR_INADEQUATE_KEY_USAGE) \
    MAP(Result::ERROR_INVALID_ALGORITHM, SEC_ERROR_INVALID_ALGORITHM) \
    MAP(Result::ERROR_INVALID_TIME, SEC_ERROR_INVALID_TIME) \
    MAP(Result::ERROR_KEY_PINNING_FAILURE, MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE) \
    MAP(Result::ERROR_PATH_LEN_CONSTRAINT_INVALID, SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID) \
    MAP(Result::ERROR_POLICY_VALIDATION_FAILED, SEC_ERROR_POLICY_VALIDATION_FAILED) \
    MAP(Result::ERROR_REVOKED_CERTIFICATE, SEC_ERROR_REVOKED_CERTIFICATE) \
    MAP(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION, SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION) \
    MAP(Result::ERROR_UNKNOWN_ERROR, PR_UNKNOWN_ERROR) \
    MAP(Result::ERROR_UNKNOWN_ISSUER, SEC_ERROR_UNKNOWN_ISSUER) \
    MAP(Result::ERROR_UNTRUSTED_CERT, SEC_ERROR_UNTRUSTED_CERT) \
    MAP(Result::ERROR_UNTRUSTED_ISSUER, SEC_ERROR_UNTRUSTED_ISSUER) \
    MAP(Result::ERROR_OCSP_BAD_SIGNATURE, SEC_ERROR_OCSP_BAD_SIGNATURE) \
    MAP(Result::ERROR_OCSP_INVALID_SIGNING_CERT, SEC_ERROR_OCSP_INVALID_SIGNING_CERT) \
    MAP(Result::ERROR_OCSP_MALFORMED_REQUEST, SEC_ERROR_OCSP_MALFORMED_REQUEST) \
    MAP(Result::ERROR_OCSP_MALFORMED_RESPONSE, SEC_ERROR_OCSP_MALFORMED_RESPONSE) \
    MAP(Result::ERROR_OCSP_OLD_RESPONSE, SEC_ERROR_OCSP_OLD_RESPONSE) \
    MAP(Result::ERROR_OCSP_REQUEST_NEEDS_SIG, SEC_ERROR_OCSP_REQUEST_NEEDS_SIG) \
    MAP(Result::ERROR_OCSP_RESPONDER_CERT_INVALID, SEC_ERROR_OCSP_RESPONDER_CERT_INVALID) \
    MAP(Result::ERROR_OCSP_SERVER_ERROR, SEC_ERROR_OCSP_SERVER_ERROR) \
    MAP(Result::ERROR_OCSP_TRY_SERVER_LATER, SEC_ERROR_OCSP_TRY_SERVER_LATER) \
    MAP(Result::ERROR_OCSP_UNAUTHORIZED_REQUEST, SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST) \
    MAP(Result::ERROR_OCSP_UNKNOWN_RESPONSE_STATUS, SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS) \
    MAP(Result::ERROR_OCSP_UNKNOWN_CERT, SEC_ERROR_OCSP_UNKNOWN_CERT) \
    MAP(Result::ERROR_OCSP_FUTURE_RESPONSE, SEC_ERROR_OCSP_FUTURE_RESPONSE) \
    MAP(Result::ERROR_INVALID_KEY, SEC_ERROR_INVALID_KEY) \
    MAP(Result::ERROR_UNSUPPORTED_KEYALG, SEC_ERROR_UNSUPPORTED_KEYALG) \
    MAP(Result::FATAL_ERROR_INVALID_ARGS, SEC_ERROR_INVALID_ARGS) \
    MAP(Result::FATAL_ERROR_INVALID_STATE, PR_INVALID_STATE_ERROR) \
    MAP(Result::FATAL_ERROR_LIBRARY_FAILURE, SEC_ERROR_LIBRARY_FAILURE) \
    MAP(Result::FATAL_ERROR_NO_MEMORY, SEC_ERROR_NO_MEMORY) \
    /* nothing here */

Result
MapPRErrorCodeToResult(PRErrorCode error)
{
  switch (error)
  {
#define MAP(mozilla_pkix_result, nss_result) \
    case nss_result: return mozilla_pkix_result;

    MAP_LIST

#undef MAP

    default:
      return Result::ERROR_UNKNOWN_ERROR;
  }
}

PRErrorCode
MapResultToPRErrorCode(Result result)
{
  switch (result)
  {
#define MAP(mozilla_pkix_result, nss_result) \
    case mozilla_pkix_result: return nss_result;

    MAP_LIST

#undef MAP

    default:
      PR_NOT_REACHED("Unknown error code in MapResultToPRErrorCode");
      return SEC_ERROR_LIBRARY_FAILURE;
  }
}

const char*
MapResultToName(Result result)
{
  switch (result)
  {
#define MAP(mozilla_pkix_result, nss_result) \
    case mozilla_pkix_result: return #mozilla_pkix_result;

    MAP_LIST

#undef MAP

    default:
      PR_NOT_REACHED("Unknown error code in MapResultToName");
      return nullptr;
  }
}

void
RegisterErrorTable()
{
  static const struct PRErrorMessage ErrorTableText[] = {
    { "MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE",
      "The server uses key pinning (HPKP) but no trusted certificate chain "
      "could be constructed that matches the pinset. Key pinning violations "
      "cannot be overridden." }
  };

  static const struct PRErrorTable ErrorTable = {
    ErrorTableText,
    "pkixerrors",
    ERROR_BASE,
    PR_ARRAY_SIZE(ErrorTableText)
  };

  (void) PR_ErrorInstallTable(&ErrorTable);
}

} } // namespace mozilla::pkix
