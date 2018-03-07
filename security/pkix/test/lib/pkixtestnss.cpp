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
#include "pkixtestnss.h"

#include <limits>

#include "cryptohi.h"
#include "keyhi.h"
#include "nss.h"
#include "pk11pqg.h"
#include "pk11pub.h"
#include "pkix/pkixnss.h"
#include "pkixder.h"
#include "pkixutil.h"
#include "prinit.h"
#include "secerr.h"
#include "secitem.h"

namespace mozilla { namespace pkix { namespace test {

namespace {

inline void
SECITEM_FreeItem_true(SECItem* item)
{
  SECITEM_FreeItem(item, true);
}

inline void
SECKEY_DestroyEncryptedPrivateKeyInfo_true(SECKEYEncryptedPrivateKeyInfo* e)
{
  SECKEY_DestroyEncryptedPrivateKeyInfo(e, true);
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
  reusedKeyPair.reset(GenerateKeyPairInner());
  return reusedKeyPair ? PR_SUCCESS : PR_FAILURE;
}

class NSSTestKeyPair final : public TestKeyPair
{
public:
  NSSTestKeyPair(const TestPublicKeyAlgorithm& aPublicKeyAlg,
                 const ByteString& spk,
                 const ByteString& aEncryptedPrivateKey,
                 const ByteString& aEncryptionAlgorithm,
                 const ByteString& aEncryptionParams)
    : TestKeyPair(aPublicKeyAlg, spk)
    , encryptedPrivateKey(aEncryptedPrivateKey)
    , encryptionAlgorithm(aEncryptionAlgorithm)
    , encryptionParams(aEncryptionParams)
  {
  }

  Result SignData(const ByteString& tbs,
                  const TestSignatureAlgorithm& signatureAlgorithm,
                  /*out*/ ByteString& signature) const override
  {
    SECOidTag oidTag;
    if (signatureAlgorithm.publicKeyAlg == RSA_PKCS1()) {
      switch (signatureAlgorithm.digestAlg) {
        case TestDigestAlgorithmID::MD2:
          oidTag = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::MD5:
          oidTag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::SHA1:
          oidTag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::SHA224:
          oidTag = SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::SHA256:
          oidTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::SHA384:
          oidTag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
          break;
        case TestDigestAlgorithmID::SHA512:
          oidTag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
          break;
        MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
      }
    } else {
      abort();
    }

    ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
    if (!slot) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    SECItem encryptedPrivateKeyInfoItem = {
      siBuffer,
      const_cast<uint8_t*>(encryptedPrivateKey.data()),
      static_cast<unsigned int>(encryptedPrivateKey.length())
    };
    SECItem encryptionAlgorithmItem = {
      siBuffer,
      const_cast<uint8_t*>(encryptionAlgorithm.data()),
      static_cast<unsigned int>(encryptionAlgorithm.length())
    };
    SECItem encryptionParamsItem = {
      siBuffer,
      const_cast<uint8_t*>(encryptionParams.data()),
      static_cast<unsigned int>(encryptionParams.length())
    };
    SECKEYEncryptedPrivateKeyInfo encryptedPrivateKeyInfo = {
      nullptr,
      { encryptionAlgorithmItem, encryptionParamsItem },
      encryptedPrivateKeyInfoItem
    };
    SECItem passwordItem = { siBuffer, nullptr, 0 };
    SECItem publicValueItem = {
      siBuffer,
      const_cast<uint8_t*>(subjectPublicKey.data()),
      static_cast<unsigned int>(subjectPublicKey.length())
    };
    SECKEYPrivateKey* privateKey;
    // This should always be an RSA key (we'll have aborted above if we're not
    // doing an RSA signature).
    if (PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
          slot.get(), &encryptedPrivateKeyInfo, &passwordItem, nullptr,
          &publicValueItem, false, false, rsaKey, KU_ALL, &privateKey,
          nullptr) != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    ScopedSECKEYPrivateKey scopedPrivateKey(privateKey);
    SECItem signatureItem;
    if (SEC_SignData(&signatureItem, tbs.data(),
                     static_cast<int>(tbs.length()),
                     scopedPrivateKey.get(), oidTag) != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    signature.assign(signatureItem.data, signatureItem.len);
    SECITEM_FreeItem(&signatureItem, false);
    return Success;
  }

  TestKeyPair* Clone() const override
  {
    return new (std::nothrow) NSSTestKeyPair(publicKeyAlg,
                                             subjectPublicKey,
                                             encryptedPrivateKey,
                                             encryptionAlgorithm,
                                             encryptionParams);
  }

private:
  const ByteString encryptedPrivateKey;
  const ByteString encryptionAlgorithm;
  const ByteString encryptionParams;
};

} // namespace

// This private function is also used by Gecko's PSM test framework
// (OCSPCommon.cpp).
TestKeyPair* CreateTestKeyPair(const TestPublicKeyAlgorithm publicKeyAlg,
                               const ScopedSECKEYPublicKey& publicKey,
                               const ScopedSECKEYPrivateKey& privateKey)
{
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_CreateSubjectPublicKeyInfo(publicKey.get()));
  if (!spki) {
    return nullptr;
  }
  SECItem spkDER = spki->subjectPublicKey;
  DER_ConvertBitString(&spkDER); // bits to bytes
  ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }
  // Because NSSTestKeyPair isn't tracked by XPCOM and won't otherwise be aware
  // of shutdown, we don't have a way to release NSS resources at the
  // appropriate time. To work around this, NSSTestKeyPair doesn't hold on to
  // NSS resources. Instead, we export the generated private key part as an
  // encrypted blob (with an empty password and fairly lame encryption). When we
  // need to use it (e.g. to sign something), we decrypt it and create a
  // temporary key object.
  SECItem passwordItem = { siBuffer, nullptr, 0 };
  ScopedPtr<SECKEYEncryptedPrivateKeyInfo,
            SECKEY_DestroyEncryptedPrivateKeyInfo_true> encryptedPrivateKey(
    PK11_ExportEncryptedPrivKeyInfo(
      slot.get(), SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC,
      &passwordItem, privateKey.get(), 1, nullptr));
  if (!encryptedPrivateKey) {
    return nullptr;
  }

  return new (std::nothrow) NSSTestKeyPair(
    publicKeyAlg,
    ByteString(spkDER.data, spkDER.len),
    ByteString(encryptedPrivateKey->encryptedData.data,
               encryptedPrivateKey->encryptedData.len),
    ByteString(encryptedPrivateKey->algorithm.algorithm.data,
               encryptedPrivateKey->algorithm.algorithm.len),
    ByteString(encryptedPrivateKey->algorithm.parameters.data,
               encryptedPrivateKey->algorithm.parameters.len));
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
      return CreateTestKeyPair(RSA_PKCS1(), publicKey, privateKey);
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

} // namespace

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

  ByteString p(DSS_P());
  ByteString q(DSS_Q());
  ByteString g(DSS_G());

  static const PQGParams PARAMS = {
    nullptr,
    { siBuffer,
      const_cast<uint8_t*>(p.data()),
      static_cast<unsigned int>(p.length())
    },
    { siBuffer,
      const_cast<uint8_t*>(q.data()),
      static_cast<unsigned int>(q.length())
    },
    { siBuffer,
      const_cast<uint8_t*>(g.data()),
      static_cast<unsigned int>(g.length())
    }
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
  return CreateTestKeyPair(DSS(), publicKey, privateKey);
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
