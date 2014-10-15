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
CheckPublicKeySize(Input subjectPublicKeyInfo,
                   /*out*/ ScopedSECKeyPublicKey& publicKey)
{
  SECItem subjectPublicKeyInfoSECItem =
    UnsafeMapInputToSECItem(subjectPublicKeyInfo);
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&subjectPublicKeyInfoSECItem));
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
        return Result::ERROR_INADEQUATE_KEY_SIZE;
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
CheckPublicKey(Input subjectPublicKeyInfo)
{
  ScopedSECKeyPublicKey unused;
  return CheckPublicKeySize(subjectPublicKeyInfo, unused);
}

Result
VerifySignedData(const SignedDataWithSignature& sd,
                 Input subjectPublicKeyInfo, void* pkcs11PinArg)
{
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
    case SignatureAlgorithm::unsupported_algorithm:
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

  // The static_cast is safe as long as the length of the data in sd.data can
  // fit in an int. Right now that length is stored as a uint16_t, so this
  // works. In the future this may change, hence the assertion.
  // See also bug 921585.
  static_assert(sizeof(decltype(sd.data.GetLength())) < sizeof(int),
                "sd.data.GetLength() must fit in an int");
  SECItem dataSECItem(UnsafeMapInputToSECItem(sd.data));
  SECItem signatureSECItem(UnsafeMapInputToSECItem(sd.signature));
  SECStatus srv = VFY_VerifyDataDirect(dataSECItem.data,
                                       static_cast<int>(dataSECItem.len),
                                       pubKey.get(), &signatureSECItem,
                                       pubKeyAlg, digestAlg, nullptr,
                                       pkcs11PinArg);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  return Success;
}

Result
DigestBuf(Input item, /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  static_assert(TrustDomain::DIGEST_LENGTH == SHA1_LENGTH,
                "TrustDomain::DIGEST_LENGTH must be 20 (SHA-1 digest length)");
  if (digestBufLen != TrustDomain::DIGEST_LENGTH) {
    PR_NOT_REACHED("invalid hash length");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  SECItem itemSECItem = UnsafeMapInputToSECItem(item);
  if (itemSECItem.len >
        static_cast<decltype(itemSECItem.len)>(
          std::numeric_limits<int32_t>::max())) {
    PR_NOT_REACHED("large items should not be possible here");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  SECStatus srv = PK11_HashBuf(SEC_OID_SHA1, digestBuf, itemSECItem.data,
                               static_cast<int32_t>(itemSECItem.len));
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  return Success;
}

Result
MapPRErrorCodeToResult(PRErrorCode error)
{
  switch (error)
  {
#define MOZILLA_PKIX_MAP(mozilla_pkix_result, value, nss_result) \
    case nss_result: return Result::mozilla_pkix_result;

    MOZILLA_PKIX_MAP_LIST

#undef MOZILLA_PKIX_MAP

    default:
      return Result::ERROR_UNKNOWN_ERROR;
  }
}

PRErrorCode
MapResultToPRErrorCode(Result result)
{
  switch (result)
  {
#define MOZILLA_PKIX_MAP(mozilla_pkix_result, value, nss_result) \
    case Result::mozilla_pkix_result: return nss_result;

    MOZILLA_PKIX_MAP_LIST

#undef MOZILLA_PKIX_MAP

    default:
      PR_NOT_REACHED("Unknown error code in MapResultToPRErrorCode");
      return SEC_ERROR_LIBRARY_FAILURE;
  }
}

void
RegisterErrorTable()
{
  // Note that these error strings are not localizable.
  // When these strings change, update the localization information too.
  static const struct PRErrorMessage ErrorTableText[] = {
    { "MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE",
      "The server uses key pinning (HPKP) but no trusted certificate chain "
      "could be constructed that matches the pinset. Key pinning violations "
      "cannot be overridden." },
    { "MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY",
      "The server uses a certificate with a basic constraints extension "
      "identifying it as a certificate authority. For a properly-issued "
      "certificate, this should not be the case." },
    { "MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE",
      "The server presented a certificate with a key size that is too small "
      "to establish a secure connection." },
    { "MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA",
      "An X.509 version 1 certificate that is not a trust anchor was used to "
      "issue the server's certificate. X.509 version 1 certificates are "
      "deprecated and should not be used to sign other certificates." },
  };
  // Note that these error strings are not localizable.
  // When these strings change, update the localization information too.

  static const struct PRErrorTable ErrorTable = {
    ErrorTableText,
    "pkixerrors",
    ERROR_BASE,
    PR_ARRAY_SIZE(ErrorTableText)
  };

  (void) PR_ErrorInstallTable(&ErrorTable);
}

} } // namespace mozilla::pkix
