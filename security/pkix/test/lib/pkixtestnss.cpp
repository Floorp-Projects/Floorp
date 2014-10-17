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

class NSSTestKeyPair : public TestKeyPair
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

  virtual Result SignData(const ByteString& tbs,
                          const ByteString& signatureAlgorithm,
                          /*out*/ ByteString& signature) const
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
    if (SEC_SignData(&signatureItem, tbs.data(), tbs.length(),
                     privateKey.get(), signatureAlgorithmOidTag)
          != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    signature.assign(signatureItem.data, signatureItem.len);
    SECITEM_FreeItem(&signatureItem, false);
    return Success;
  }

  virtual TestKeyPair* Clone() const
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
#if defined(_MSC_VER) && (_MSC_VER < 1700)
  // Older versions of MSVC don't know that abort() never returns, so silence
  // its warning by adding a redundant and never-reached return. But, only do
  // it for that ancient compiler, because some other compilers will rightly
  // warn that the return statement is unreachable.
  return nullptr;
#endif
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

ByteString
SHA1(const ByteString& toHash)
{
  InitNSSIfNeeded();

  uint8_t digestBuf[SHA1_LENGTH];
  SECStatus srv = PK11_HashBuf(SEC_OID_SHA1, digestBuf, toHash.data(),
                               static_cast<int32_t>(toHash.length()));
  if (srv != SECSuccess) {
    return ByteString();
  }
  return ByteString(digestBuf, sizeof(digestBuf));
}

Result
TestCheckPublicKey(Input subjectPublicKeyInfo)
{
  InitNSSIfNeeded();
  return CheckPublicKey(subjectPublicKeyInfo);
}

Result
TestVerifySignedData(const SignedDataWithSignature& signedData,
                     Input subjectPublicKeyInfo)
{
  InitNSSIfNeeded();
  return VerifySignedData(signedData, subjectPublicKeyInfo, nullptr);
}

Result
TestDigestBuf(Input item, /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  InitNSSIfNeeded();
  return DigestBuf(item, digestBuf, digestBufLen);
}

} } } // namespace mozilla::pkix::test
