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

#include "pkixtestutil.h"

#include <limits>

#include "cryptohi.h"
#include "keyhi.h"
#include "nss.h"
#include "pk11pqg.h"
#include "pk11pub.h"
#include "pkix/pkixnss.h"
#include "pkixder.h"
#include "prinit.h"
#include "secerr.h"
#include "secitem.h"

namespace mozilla { namespace pkix { namespace test {

namespace {

typedef ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
  ScopedSECKEYPublicKey;
typedef ScopedPtr<SECKEYPrivateKey, SECKEY_DestroyPrivateKey>
  ScopedSECKEYPrivateKey;

inline void
SECITEM_FreeItem_true(SECItem* item)
{
  SECITEM_FreeItem(item, true);
}

typedef mozilla::pkix::ScopedPtr<SECItem, SECITEM_FreeItem_true> ScopedSECItem;

TestKeyPair* GenerateKeyPairInner();

void
InitNSSIfNeeded()
{
  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    abort();
  }
}

static ScopedTestKeyPair reusedKeyPair;

PRStatus
InitReusedKeyPair()
{
  InitNSSIfNeeded();
  reusedKeyPair = GenerateKeyPairInner();
  return reusedKeyPair ? PR_SUCCESS : PR_FAILURE;
}

class NSSTestKeyPair final : public TestKeyPair
{
public:
  // NSSTestKeyPair takes ownership of privateKey.
  NSSTestKeyPair(const ByteString& spki,
                 const ByteString& spk,
                 SECKEYPrivateKey* privateKey)
    : TestKeyPair(spki, spk)
    , privateKey(privateKey)
  {
  }

  Result SignData(const ByteString& tbs, const ByteString& signatureAlgorithm,
                  /*out*/ ByteString& signature) const override
  {
    // signatureAlgorithm is of the form SEQUENCE { OID { <OID bytes> } },
    // whereas SECOID_GetAlgorithmTag wants just the OID bytes, so we have to
    // unwrap it here. As long as signatureAlgorithm is short enough, we don't
    // have to do full DER decoding here.
    // Also, this is just for testing purposes - there shouldn't be any
    // untrusted input given to this function. If we make a mistake, we only
    // have ourselves to blame.
    if (signatureAlgorithm.length() > 127 ||
        signatureAlgorithm.length() < 4 ||
        signatureAlgorithm.data()[0] != der::SEQUENCE ||
        signatureAlgorithm.data()[2] != der::OIDTag) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    SECAlgorithmID signatureAlgorithmID;
    signatureAlgorithmID.algorithm.data =
      const_cast<unsigned char*>(signatureAlgorithm.data() + 4);
    signatureAlgorithmID.algorithm.len = signatureAlgorithm.length() - 4;
    signatureAlgorithmID.parameters.data = nullptr;
    signatureAlgorithmID.parameters.len = 0;
    SECOidTag signatureAlgorithmOidTag =
      SECOID_GetAlgorithmTag(&signatureAlgorithmID);
    if (signatureAlgorithmOidTag == SEC_OID_UNKNOWN) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }

    SECItem signatureItem;
    if (SEC_SignData(&signatureItem, tbs.data(),
                     static_cast<int>(tbs.length()),
                     privateKey.get(), signatureAlgorithmOidTag)
          != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    signature.assign(signatureItem.data, signatureItem.len);
    SECITEM_FreeItem(&signatureItem, false);
    return Success;
  }

  TestKeyPair* Clone() const override
  {
    ScopedSECKEYPrivateKey
      privateKeyCopy(SECKEY_CopyPrivateKey(privateKey.get()));
    if (!privateKeyCopy) {
      return nullptr;
    }
    return new (std::nothrow) NSSTestKeyPair(subjectPublicKeyInfo,
                                             subjectPublicKey,
                                             privateKeyCopy.release());
  }

private:
  ScopedSECKEYPrivateKey privateKey;
};

} // unnamed namespace

// This private function is also used by Gecko's PSM test framework
// (OCSPCommon.cpp).
//
// Ownership of privateKey is transfered.
TestKeyPair* CreateTestKeyPair(const ByteString& spki,
                               const ByteString& spk,
                               SECKEYPrivateKey* privateKey)
{
  return new (std::nothrow) NSSTestKeyPair(spki, spk, privateKey);
}

namespace {

TestKeyPair*
GenerateKeyPairInner()
{
  ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
  if (!slot) {
    abort();
  }

  // Bug 1012786: PK11_GenerateKeyPair can fail if there is insufficient
  // entropy to generate a random key. Attempting to add some entropy and
  // retrying appears to solve this issue.
  for (uint32_t retries = 0; retries < 10; retries++) {
    PK11RSAGenParams params;
    params.keySizeInBits = 2048;
    params.pe = 3;
    SECKEYPublicKey* publicKeyTemp = nullptr;
    ScopedSECKEYPrivateKey
      privateKey(PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &params, &publicKeyTemp, false, true,
                                      nullptr));
    ScopedSECKEYPublicKey publicKey(publicKeyTemp);
    if (privateKey) {
      ScopedSECItem
        spkiDER(SECKEY_EncodeDERSubjectPublicKeyInfo(publicKey.get()));
      if (!spkiDER) {
        break;
      }
      ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
        spki(SECKEY_CreateSubjectPublicKeyInfo(publicKey.get()));
      if (!spki) {
        break;
      }
      SECItem spkDER = spki->subjectPublicKey;
      DER_ConvertBitString(&spkDER); // bits to bytes
      return CreateTestKeyPair(ByteString(spkiDER->data, spkiDER->len),
                               ByteString(spkDER.data, spkDER.len),
                               privateKey.release());
    }

    assert(!publicKeyTemp);

    if (PR_GetError() != SEC_ERROR_PKCS11_FUNCTION_FAILED) {
      break;
    }

    // Since these keys are only for testing, we don't need them to be good,
    // random keys.
    // https://xkcd.com/221/
    static const uint8_t RANDOM_NUMBER[] = { 4, 4, 4, 4, 4, 4, 4, 4 };
    if (PK11_RandomUpdate((void*) &RANDOM_NUMBER,
                          sizeof(RANDOM_NUMBER)) != SECSuccess) {
      break;
    }
  }

  abort();
}

} // unnamed namespace

TestKeyPair*
GenerateKeyPair()
{
  InitNSSIfNeeded();
  return GenerateKeyPairInner();
}

TestKeyPair*
CloneReusedKeyPair()
{
  static PRCallOnceType initCallOnce;
  if (PR_CallOnce(&initCallOnce, InitReusedKeyPair) != PR_SUCCESS) {
    abort();
  }
  assert(reusedKeyPair);
  return reusedKeyPair->Clone();
}

TestKeyPair*
GenerateDSSKeyPair()
{
  InitNSSIfNeeded();

  ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  // XXX: Since it takes over 20 seconds to generate the parameters, we've
  // taken the parameters generated during one run and hard-coded them for use
  // in all runs. The parameters were generated from this code:
  //
  // PQGParams* pqgParamsTemp = nullptr;
  // PQGVerify* pqgVerify = nullptr;
  // if (PK11_PQG_ParamGenV2(2048u, 256u, 256u / 8u, &pqgParamsTemp,
  //                         &pqgVerify) != SECSuccess) {
  //  return nullptr;
  // }
  // PK11_PQG_DestroyVerify(pqgVerify);
  // ScopedPtr<PQGParams, PK11_PQG_DestroyParams> params(pqgParamsTemp);

  static const uint8_t P[2048 / 8] = {
    0xff, 0x09, 0x00, 0x0e, 0x4d, 0x8e, 0xfb, 0x6b,
    0x01, 0xd9, 0xb2, 0x3a, 0x80, 0xb0, 0xf5, 0xbd,
    0xe9, 0x21, 0xcf, 0x07, 0xa0, 0x6a, 0xd3, 0x9e,
    0x5c, 0x0d, 0x2f, 0x13, 0x2e, 0x94, 0x41, 0x96,
    0x7d, 0xed, 0x90, 0x43, 0x2a, 0x0c, 0x12, 0x75,
    0xe3, 0x56, 0xa1, 0xb4, 0x59, 0xd4, 0x4f, 0xf3,
    0x3f, 0x79, 0x9a, 0x7d, 0xe9, 0x51, 0xb7, 0xe2,
    0x07, 0x0f, 0x51, 0xc7, 0x68, 0x2b, 0x82, 0x3c,
    0xa4, 0x8c, 0xdc, 0x31, 0xe0, 0x80, 0xb3, 0x6a,
    0x30, 0xd5, 0xcb, 0xc7, 0x9e, 0xe6, 0xda, 0xee,
    0x92, 0x87, 0xdf, 0x85, 0x19, 0xf0, 0xe5, 0x24,
    0x4b, 0xb4, 0xce, 0x69, 0xd0, 0x46, 0x08, 0x3a,
    0x1f, 0xf4, 0x98, 0x43, 0x2b, 0x0f, 0xb6, 0xd7,
    0xf2, 0x57, 0xac, 0x2b, 0xbb, 0x85, 0x5c, 0x67,
    0xdd, 0x22, 0xab, 0x50, 0xd8, 0x69, 0x1e, 0xbe,
    0xa2, 0xb6, 0x2d, 0xae, 0xbb, 0xf6, 0x27, 0x41,
    0x4f, 0x92, 0x04, 0x4f, 0x99, 0x15, 0x07, 0x52,
    0x22, 0xdf, 0x92, 0xea, 0xee, 0x05, 0xdc, 0xd7,
    0x23, 0x3d, 0x63, 0xc1, 0xe0, 0x92, 0x3d, 0x1a,
    0xbe, 0x53, 0xab, 0x5f, 0x6a, 0x8b, 0xca, 0x6c,
    0x86, 0x8e, 0xe7, 0xf9, 0x15, 0x62, 0xd5, 0x33,
    0xf4, 0x19, 0x3b, 0x58, 0x4e, 0xb7, 0x4f, 0xd3,
    0xdd, 0x91, 0x34, 0xae, 0x55, 0x11, 0x38, 0xd9,
    0x5d, 0x4a, 0xa0, 0xad, 0xf1, 0xea, 0x54, 0xf8,
    0xda, 0x50, 0x6e, 0xb2, 0x94, 0xa9, 0x95, 0x58,
    0x70, 0x55, 0x74, 0x3c, 0xb8, 0x57, 0xe6, 0x5a,
    0x65, 0x3f, 0x4f, 0x17, 0x32, 0xb3, 0x0b, 0x09,
    0xcd, 0x4f, 0x5a, 0x53, 0x04, 0xbf, 0xd7, 0x4d,
    0xb4, 0x3d, 0x1a, 0x5b, 0xb8, 0x5d, 0xd9, 0x35,
    0x8a, 0x56, 0x6c, 0x48, 0x1e, 0x42, 0x7b, 0x54,
    0xc4, 0xbb, 0x57, 0x16, 0xb5, 0x49, 0x79, 0x1c,
    0xaa, 0x90, 0x5b, 0x2b, 0x0b, 0x96, 0x92, 0x8d
  };

  static const uint8_t Q[256 / 8] = {
    0xd2, 0x5c, 0xab, 0xa7, 0xac, 0x92, 0x39, 0xa4,
    0x20, 0xc1, 0x7d, 0x2f, 0x81, 0xb0, 0x6a, 0x81,
    0xa6, 0xdc, 0xc3, 0xfa, 0xae, 0x7f, 0x78, 0x82,
    0xa1, 0xc7, 0xf4, 0x59, 0x62, 0x11, 0x6c, 0x67
  };

  static const uint8_t G[2048 / 8] = {
    0xde, 0x50, 0x3b, 0x1d, 0xf0, 0x82, 0xa0, 0x0a,
    0x80, 0x61, 0xed, 0x77, 0x8a, 0x0d, 0x04, 0xdc,
    0x16, 0x03, 0x4a, 0x24, 0x3a, 0x0e, 0x44, 0x8d,
    0xef, 0x94, 0x4a, 0x50, 0x5e, 0x5d, 0xa9, 0xb4,
    0x19, 0x6b, 0xc8, 0x73, 0xb1, 0xc6, 0xb9, 0x61,
    0xc1, 0x81, 0x9e, 0x8d, 0x8e, 0xd7, 0x74, 0x14,
    0xba, 0xd2, 0x30, 0x03, 0x2d, 0xf6, 0xb4, 0xe2,
    0x40, 0xdd, 0xbe, 0xe6, 0x1b, 0x1c, 0x11, 0xab,
    0x80, 0x08, 0x34, 0x96, 0xe0, 0x0b, 0x4d, 0xa5,
    0x3b, 0x5e, 0xee, 0xc0, 0x6d, 0xac, 0x05, 0x8d,
    0x45, 0x7a, 0xb7, 0x8d, 0x22, 0x42, 0x35, 0x5e,
    0x36, 0xff, 0xcf, 0x42, 0x10, 0x43, 0x07, 0x94,
    0xbf, 0x59, 0xad, 0xc3, 0x10, 0xd0, 0xc1, 0xfb,
    0xe3, 0x06, 0x40, 0x76, 0x88, 0xf6, 0xc5, 0x46,
    0xf1, 0x3d, 0x26, 0xb8, 0xa5, 0x18, 0xd5, 0x5c,
    0xe4, 0x46, 0x96, 0x3f, 0xe1, 0x90, 0x39, 0xa3,
    0xa4, 0x64, 0x3c, 0xd1, 0xc6, 0x70, 0xeb, 0xeb,
    0xd1, 0x7c, 0x53, 0x14, 0x2c, 0xa3, 0xdf, 0x70,
    0x86, 0xbf, 0xbf, 0x08, 0xed, 0xc6, 0x54, 0xc3,
    0xc6, 0xcc, 0xe5, 0xcd, 0xc5, 0x21, 0x9b, 0x07,
    0xf7, 0xd5, 0x10, 0x9a, 0xd4, 0x83, 0xb6, 0x5b,
    0x1c, 0x2e, 0xd1, 0x53, 0xdb, 0x53, 0x52, 0xcd,
    0xda, 0x48, 0x60, 0x2c, 0x99, 0x1f, 0x00, 0xde,
    0x7a, 0x45, 0xaa, 0x15, 0xb0, 0xd4, 0xfb, 0x9c,
    0xf3, 0x17, 0xc9, 0x32, 0x49, 0x13, 0xf3, 0xe6,
    0x73, 0x0b, 0x4e, 0x8b, 0x01, 0xf1, 0xb5, 0x9e,
    0xa7, 0xa7, 0x8a, 0x46, 0xcd, 0xb7, 0xee, 0xf0,
    0x0d, 0xee, 0x90, 0x2f, 0x09, 0xe1, 0x9d, 0xe9,
    0x59, 0x84, 0xd6, 0xb6, 0xf1, 0xb2, 0x27, 0xc1,
    0x7a, 0xd8, 0x37, 0xb8, 0x6a, 0xb9, 0xd1, 0x58,
    0x78, 0xad, 0x0f, 0xa5, 0xac, 0xea, 0x79, 0x8b,
    0x27, 0x79, 0xcf, 0x6c, 0x11, 0xbe, 0x9c, 0xac
  };

  static const PQGParams PARAMS = {
    nullptr,
    { siBuffer, const_cast<uint8_t*>(P), sizeof(P) },
    { siBuffer, const_cast<uint8_t*>(Q), sizeof(Q) },
    { siBuffer, const_cast<uint8_t*>(G), sizeof(G) }
  };

  SECKEYPublicKey* publicKeyTemp = nullptr;
  ScopedSECKEYPrivateKey
    privateKey(PK11_GenerateKeyPair(slot.get(), CKM_DSA_KEY_PAIR_GEN,
                                    const_cast<PQGParams*>(&PARAMS),
                                    &publicKeyTemp, false, true, nullptr));
  if (!privateKey) {
    return nullptr;
  }
  ScopedSECKEYPublicKey publicKey(publicKeyTemp);

  ScopedSECItem spkiDER(SECKEY_EncodeDERSubjectPublicKeyInfo(publicKey.get()));
  if (!spkiDER) {
    return nullptr;
  }

  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_CreateSubjectPublicKeyInfo(publicKey.get()));
  if (!spki) {
    return nullptr;
  }

  SECItem spkDER = spki->subjectPublicKey;
  DER_ConvertBitString(&spkDER); // bits to bytes
  return CreateTestKeyPair(ByteString(spkiDER->data, spkiDER->len),
                           ByteString(spkDER.data, spkDER.len),
                           privateKey.release());
}

Result
TestVerifyECDSASignedDigest(const SignedDigest& signedDigest,
                            Input subjectPublicKeyInfo)
{
  InitNSSIfNeeded();
  return VerifyECDSASignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                    nullptr);
}

Result
TestVerifyRSAPKCS1SignedDigest(const SignedDigest& signedDigest,
                               Input subjectPublicKeyInfo)
{
  InitNSSIfNeeded();
  return VerifyRSAPKCS1SignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                       nullptr);
}

Result
TestDigestBuf(Input item,
              DigestAlgorithm digestAlg,
              /*out*/ uint8_t* digestBuf,
              size_t digestBufLen)
{
  InitNSSIfNeeded();
  return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
}

} } } // namespace mozilla::pkix::test
