/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2015 Mozilla Contributors
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

#include "mozpkix/pkixutil.h"

namespace mozilla { namespace pkix {

Result
DigestSignedData(TrustDomain& trustDomain,
                 const der::SignedDataWithSignature& signedData,
                 /*out*/ uint8_t(&digestBuf)[MAX_DIGEST_SIZE_IN_BYTES],
                 /*out*/ der::PublicKeyAlgorithm& publicKeyAlg,
                 /*out*/ SignedDigest& signedDigest)
{
  Reader signatureAlg(signedData.algorithm);
  Result rv = der::SignatureAlgorithmIdentifierValue(
                signatureAlg, publicKeyAlg, signedDigest.digestAlgorithm);
  if (rv != Success) {
    return rv;
  }
  if (!signatureAlg.AtEnd()) {
    return Result::ERROR_BAD_DER;
  }

  size_t digestLen;
  switch (signedDigest.digestAlgorithm) {
    case DigestAlgorithm::sha512: digestLen = 512 / 8; break;
    case DigestAlgorithm::sha384: digestLen = 384 / 8; break;
    case DigestAlgorithm::sha256: digestLen = 256 / 8; break;
    case DigestAlgorithm::sha1: digestLen = 160 / 8; break;
    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
  assert(digestLen <= sizeof(digestBuf));

  rv = trustDomain.DigestBuf(signedData.data, signedDigest.digestAlgorithm,
                             digestBuf, digestLen);
  if (rv != Success) {
    return rv;
  }
  rv = signedDigest.digest.Init(digestBuf, digestLen);
  if (rv != Success) {
    return rv;
  }

  return signedDigest.signature.Init(signedData.signature);
}

Result
VerifySignedDigest(TrustDomain& trustDomain,
                   der::PublicKeyAlgorithm publicKeyAlg,
                   const SignedDigest& signedDigest,
                   Input signerSubjectPublicKeyInfo)
{
  switch (publicKeyAlg) {
    case der::PublicKeyAlgorithm::ECDSA:
      return trustDomain.VerifyECDSASignedDigest(signedDigest,
                                                 signerSubjectPublicKeyInfo);
    case der::PublicKeyAlgorithm::RSA_PKCS1:
      return trustDomain.VerifyRSAPKCS1SignedDigest(signedDigest,
                                                    signerSubjectPublicKeyInfo);
    case der::PublicKeyAlgorithm::Uninitialized:
      assert(false);
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
}

Result
VerifySignedData(TrustDomain& trustDomain,
                 const der::SignedDataWithSignature& signedData,
                 Input signerSubjectPublicKeyInfo)
{
  uint8_t digestBuf[MAX_DIGEST_SIZE_IN_BYTES];
  der::PublicKeyAlgorithm publicKeyAlg;
  SignedDigest signedDigest;
  Result rv = DigestSignedData(trustDomain, signedData, digestBuf,
                               publicKeyAlg, signedDigest);
  if (rv != Success) {
    return rv;
  }
  return VerifySignedDigest(trustDomain, publicKeyAlg, signedDigest,
                            signerSubjectPublicKeyInfo);
}

} } // namespace mozilla::pkix
