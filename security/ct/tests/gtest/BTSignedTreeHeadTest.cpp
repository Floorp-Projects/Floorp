/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BTVerifier.h"
#include "CTTestUtils.h"
#include "gtest/gtest.h"

#include "nss.h"

namespace mozilla {
namespace ct {

using namespace pkix;

struct BTSignedTreeHeadTestParams {
  const char* mSubjectPublicKeyInfoHex;
  pkix::DigestAlgorithm mDigestAlgorithm;
  pkix::der::PublicKeyAlgorithm mPublicKeyAlgorithm;
  const char* mSignedTreeHeadHex;

  pkix::Result mExpectedSignedTreeHeadResult;
  uint64_t mExpectedTimestamp;
  uint64_t mExpectedTreeSize;
  const char* mExpectedRootHashHex;
};

class BTSignedTreeHeadTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<BTSignedTreeHeadTestParams> {
  void SetUp() override {
    if (!NSS_IsInitialized()) {
      if (NSS_NoDB_Init(nullptr) != SECSuccess) {
        abort();
      }
    }
  }
};

namespace ValidSTH {
#include "valid-sth.inc"
}
namespace ValidWithExtensionSTH {
#include "valid-with-extension-sth.inc"
}
namespace ValidSecp521r1SHA512STH {
#include "valid-secp521r1-sha512-sth.inc"
}
namespace SignatureCoversLogIDSTH {
#include "signature-covers-log-id-sth.inc"
}
namespace WrongSPKISTH {
#include "wrong-spki-sth.inc"
}
namespace WrongSigningKeySTH {
#include "wrong-signing-key-sth.inc"
}
namespace MissingLogIDSTH {
#include "missing-log-id-sth.inc"
}
namespace MissingTimestampSTH {
#include "missing-timestamp-sth.inc"
}
namespace MissingTreeSizeSTH {
#include "missing-tree-size-sth.inc"
}
namespace MissingRootHashSTH {
#include "missing-root-hash-sth.inc"
}
namespace MissingExtensionsSTH {
#include "missing-extensions-sth.inc"
}
namespace TruncatedLogIDSTH {
#include "truncated-log-id-sth.inc"
}
namespace TruncatedTimestampSTH {
#include "truncated-timestamp-sth.inc"
}
namespace TruncatedTreeSizeSTH {
#include "truncated-tree-size-sth.inc"
}
namespace TruncatedRootHashSTH {
#include "truncated-root-hash-sth.inc"
}
namespace TruncatedExtensionSTH {
#include "truncated-extension-sth.inc"
}
namespace RSASignerRSASPKISTH {
#include "rsa-signer-rsa-spki-sth.inc"
}
namespace RSASignerECSPKISTH {
#include "rsa-signer-ec-spki-sth.inc"
}
namespace ECSignerRSASPKISTH {
#include "ec-signer-rsa-spki-sth.inc"
}

static const char* kValidRootHashHex =
    "d1a0d3947db4ae8305f2ac32985957e02659b2ea3c10da52a48d2526e9af3bbc";

static const char* kValidRootHashSHA512Hex =
    "374d794a95cdcfd8b35993185fef9ba368f160d8daf432d08ba9f1ed1e5abe6c"
    "c69291e0fa2fe0006a52570ef18c19def4e617c33ce52ef0a6e5fbe318cb0387";

static const BTSignedTreeHeadTestParams BT_SIGNED_TREE_HEAD_TEST_PARAMS[] = {
    {ValidSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidSTH::kSTHHex, Success,
     1541189938000, 7, kValidRootHashHex},
    {ValidSTH::kSPKIHex, pkix::DigestAlgorithm::sha512,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidSTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {ValidSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::RSA_PKCS1, ValidSTH::kSTHHex,
     Result::FATAL_ERROR_INVALID_ARGS, 0, 0, nullptr},
    {ValidWithExtensionSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidWithExtensionSTH::kSTHHex,
     Success, 1541189938000, 7, kValidRootHashHex},
    {ValidSecp521r1SHA512STH::kSPKIHex, pkix::DigestAlgorithm::sha512,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidSecp521r1SHA512STH::kSTHHex,
     Success, 1542136309473, 731393445, kValidRootHashSHA512Hex},
    {ValidSecp521r1SHA512STH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidSecp521r1SHA512STH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {ValidSTH::kSPKIHex, pkix::DigestAlgorithm::sha512,
     pkix::der::PublicKeyAlgorithm::ECDSA, ValidSecp521r1SHA512STH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {SignatureCoversLogIDSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, SignatureCoversLogIDSTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {WrongSPKISTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, WrongSPKISTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {WrongSigningKeySTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, WrongSigningKeySTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {MissingLogIDSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, MissingLogIDSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {MissingTimestampSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, MissingTimestampSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {MissingTreeSizeSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, MissingTreeSizeSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {MissingRootHashSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, MissingRootHashSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {MissingExtensionsSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, MissingExtensionsSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {TruncatedLogIDSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, TruncatedLogIDSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {TruncatedTimestampSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, TruncatedTimestampSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {TruncatedTreeSizeSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, TruncatedTreeSizeSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {TruncatedRootHashSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, TruncatedRootHashSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {TruncatedExtensionSTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, TruncatedExtensionSTH::kSTHHex,
     Result::ERROR_BAD_DER, 0, 0, nullptr},
    {RSASignerRSASPKISTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, RSASignerRSASPKISTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {RSASignerECSPKISTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, RSASignerECSPKISTH::kSTHHex,
     Result::ERROR_BAD_SIGNATURE, 0, 0, nullptr},
    {ECSignerRSASPKISTH::kSPKIHex, pkix::DigestAlgorithm::sha256,
     pkix::der::PublicKeyAlgorithm::ECDSA, ECSignerRSASPKISTH::kSTHHex,
     Result::ERROR_INVALID_KEY, 0, 0, nullptr},
};

TEST_P(BTSignedTreeHeadTest, BTSignedTreeHeadSimpleTest) {
  const BTSignedTreeHeadTestParams& params(GetParam());

  Buffer subjectPublicKeyInfoBuffer(
      HexToBytes(params.mSubjectPublicKeyInfoHex));
  Input subjectPublicKeyInfoInput = InputForBuffer(subjectPublicKeyInfoBuffer);

  Buffer signedTreeHeadBuffer(HexToBytes(params.mSignedTreeHeadHex));
  Input signedTreeHeadInput = InputForBuffer(signedTreeHeadBuffer);

  SignedTreeHeadDataV2 sth;
  EXPECT_EQ(params.mExpectedSignedTreeHeadResult,
            DecodeAndVerifySignedTreeHead(
                subjectPublicKeyInfoInput, params.mDigestAlgorithm,
                params.mPublicKeyAlgorithm, signedTreeHeadInput, sth));

  if (params.mExpectedSignedTreeHeadResult == Success) {
    EXPECT_EQ(params.mExpectedTimestamp, sth.timestamp);
    EXPECT_EQ(params.mExpectedTreeSize, sth.treeSize);
    EXPECT_EQ(HexToBytes(params.mExpectedRootHashHex), sth.rootHash);
  }
}

INSTANTIATE_TEST_SUITE_P(BTSignedTreeHeadTest, BTSignedTreeHeadTest,
                         testing::ValuesIn(BT_SIGNED_TREE_HEAD_TEST_PARAMS));

TEST_F(BTSignedTreeHeadTest, BTSignedTreeHeadTamperedSignatureTest) {
  Buffer subjectPublicKeyInfoBuffer(HexToBytes(ValidSTH::kSPKIHex));
  Input subjectPublicKeyInfoInput = InputForBuffer(subjectPublicKeyInfoBuffer);

  Buffer signedTreeHeadBuffer(HexToBytes(ValidSTH::kSTHHex));
  ASSERT_TRUE(signedTreeHeadBuffer.size() > 15);
  signedTreeHeadBuffer[signedTreeHeadBuffer.size() - 15] ^= 0xff;
  Input signedTreeHeadInput = InputForBuffer(signedTreeHeadBuffer);

  SignedTreeHeadDataV2 sth;
  EXPECT_EQ(Result::ERROR_BAD_SIGNATURE,
            DecodeAndVerifySignedTreeHead(subjectPublicKeyInfoInput,
                                          pkix::DigestAlgorithm::sha256,
                                          pkix::der::PublicKeyAlgorithm::ECDSA,
                                          signedTreeHeadInput, sth));
}

TEST_F(BTSignedTreeHeadTest, BTSignedTreeHeadTruncatedSignatureTest) {
  Buffer subjectPublicKeyInfoBuffer(HexToBytes(ValidSTH::kSPKIHex));
  Input subjectPublicKeyInfoInput = InputForBuffer(subjectPublicKeyInfoBuffer);

  Buffer signedTreeHeadBuffer(HexToBytes(ValidSTH::kSTHHex));
  ASSERT_TRUE(signedTreeHeadBuffer.size() > 17);
  signedTreeHeadBuffer.resize(signedTreeHeadBuffer.size() - 17);
  Input signedTreeHeadInput = InputForBuffer(signedTreeHeadBuffer);

  SignedTreeHeadDataV2 sth;
  EXPECT_EQ(Result::ERROR_BAD_DER,
            DecodeAndVerifySignedTreeHead(subjectPublicKeyInfoInput,
                                          pkix::DigestAlgorithm::sha256,
                                          pkix::der::PublicKeyAlgorithm::ECDSA,
                                          signedTreeHeadInput, sth));
}

TEST_F(BTSignedTreeHeadTest, BTSignedTreeHeadMissingSignatureTest) {
  Buffer subjectPublicKeyInfoBuffer(HexToBytes(ValidSTH::kSPKIHex));
  Input subjectPublicKeyInfoInput = InputForBuffer(subjectPublicKeyInfoBuffer);

  Buffer signedTreeHeadBuffer = {
      0x02, 0x00, 0x00,
      // 1541189938000 milliseconds since the epoch
      0x00, 0x00, 0x01, 0x66, 0xd6, 0x14, 0x2b, 0x50, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x07,  // 7 total nodes
      0x20,                    // 32 byte hash
      0xd1, 0xa0, 0xd3, 0x94, 0x7d, 0xb4, 0xae, 0x83, 0x05, 0xf2, 0xac, 0x32,
      0x98, 0x59, 0x57, 0xe0, 0x26, 0x59, 0xb2, 0xea, 0x3c, 0x10, 0xda, 0x52,
      0xa4, 0x8d, 0x25, 0x26, 0xe9, 0xaf, 0x3b, 0xbc, 0x00,
      0x00,  // no extensions
             // missing signature
  };
  Input signedTreeHeadInput = InputForBuffer(signedTreeHeadBuffer);

  SignedTreeHeadDataV2 sth;
  EXPECT_EQ(Result::ERROR_BAD_DER,
            DecodeAndVerifySignedTreeHead(subjectPublicKeyInfoInput,
                                          pkix::DigestAlgorithm::sha256,
                                          pkix::der::PublicKeyAlgorithm::ECDSA,
                                          signedTreeHeadInput, sth));
}

}  // namespace ct
}  // namespace mozilla
