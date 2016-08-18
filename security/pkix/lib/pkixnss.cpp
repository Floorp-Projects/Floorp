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

#include "cryptohi.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkixutil.h"
#include "ScopedPtr.h"
#include "secerr.h"
#include "sslerr.h"

namespace mozilla { namespace pkix {

namespace {

Result
VerifySignedDigest(const SignedDigest& sd,
                   Input subjectPublicKeyInfo,
                   SECOidTag pubKeyAlg,
                   void* pkcs11PinArg)
{
  SECOidTag digestAlg;
  switch (sd.digestAlgorithm) {
    case DigestAlgorithm::sha512: digestAlg = SEC_OID_SHA512; break;
    case DigestAlgorithm::sha384: digestAlg = SEC_OID_SHA384; break;
    case DigestAlgorithm::sha256: digestAlg = SEC_OID_SHA256; break;
    case DigestAlgorithm::sha1: digestAlg = SEC_OID_SHA1; break;
    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }

  SECItem subjectPublicKeyInfoSECItem =
    UnsafeMapInputToSECItem(subjectPublicKeyInfo);
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&subjectPublicKeyInfoSECItem));
  if (!spki) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
    pubKey(SECKEY_ExtractPublicKey(spki.get()));
  if (!pubKey) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  SECItem digestSECItem(UnsafeMapInputToSECItem(sd.digest));
  SECItem signatureSECItem(UnsafeMapInputToSECItem(sd.signature));
  SECStatus srv = VFY_VerifyDigestDirect(&digestSECItem, pubKey.get(),
                                         &signatureSECItem, pubKeyAlg,
                                         digestAlg, pkcs11PinArg);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  return Success;
}

} // namespace

Result
VerifyRSAPKCS1SignedDigestNSS(const SignedDigest& sd,
                              Input subjectPublicKeyInfo,
                              void* pkcs11PinArg)
{
  return VerifySignedDigest(sd, subjectPublicKeyInfo,
                            SEC_OID_PKCS1_RSA_ENCRYPTION, pkcs11PinArg);
}

Result
VerifyECDSASignedDigestNSS(const SignedDigest& sd,
                           Input subjectPublicKeyInfo,
                           void* pkcs11PinArg)
{
  return VerifySignedDigest(sd, subjectPublicKeyInfo,
                            SEC_OID_ANSIX962_EC_PUBLIC_KEY, pkcs11PinArg);
}

Result
DigestBufNSS(Input item,
             DigestAlgorithm digestAlg,
             /*out*/ uint8_t* digestBuf,
             size_t digestBufLen)
{
  SECOidTag oid;
  size_t bits;
  switch (digestAlg) {
    case DigestAlgorithm::sha512: oid = SEC_OID_SHA512; bits = 512; break;
    case DigestAlgorithm::sha384: oid = SEC_OID_SHA384; bits = 384; break;
    case DigestAlgorithm::sha256: oid = SEC_OID_SHA256; bits = 256; break;
    case DigestAlgorithm::sha1: oid = SEC_OID_SHA1; bits = 160; break;
    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
  if (digestBufLen != bits / 8) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  SECItem itemSECItem = UnsafeMapInputToSECItem(item);
  if (itemSECItem.len >
        static_cast<decltype(itemSECItem.len)>(
          std::numeric_limits<int32_t>::max())) {
    PR_NOT_REACHED("large items should not be possible here");
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  SECStatus srv = PK11_HashBuf(oid, digestBuf, itemSECItem.data,
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

    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
}

void
RegisterErrorTable()
{
  // Note that these error strings are not localizable.
  // When these strings change, update the localization information too.
  static const PRErrorMessage ErrorTableText[] = {
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
    { "MOZILLA_PKIX_ERROR_NO_RFC822NAME_MATCH",
      "The certificate is not valid for the given email address." },
    { "MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE",
      "The server presented a certificate that is not yet valid." },
    { "MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE",
      "A certificate that is not yet valid was used to issue the server's "
      "certificate." },
    { "MOZILLA_PKIX_ERROR_SIGNATURE_ALGORITHM_MISMATCH",
      "The signature algorithm in the signature field of the certificate does "
      "not match the algorithm in its signatureAlgorithm field." },
    { "MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING",
      "The OCSP response does not include a status for the certificate being "
      "verified." },
    { "MOZILLA_PKIX_ERROR_VALIDITY_TOO_LONG",
      "The server presented a certificate that is valid for too long." },
    { "MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING",
      "A required TLS feature is missing." },
    { "MOZILLA_PKIX_ERROR_INVALID_INTEGER_ENCODING",
      "The server presented a certificate that contains an invalid encoding of "
      "an integer. Common causes include negative serial numbers, negative RSA "
      "moduli, and encodings that are longer than necessary." },
    { "MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME",
      "The server presented a certificate with an empty issuer distinguished "
      "name." },
  };
  // Note that these error strings are not localizable.
  // When these strings change, update the localization information too.

  static const PRErrorTable ErrorTable = {
    ErrorTableText,
    "pkixerrors",
    ERROR_BASE,
    PR_ARRAY_SIZE(ErrorTableText)
  };

  (void) PR_ErrorInstallTable(&ErrorTable);
}

} } // namespace mozilla::pkix
