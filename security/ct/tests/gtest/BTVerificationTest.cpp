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

struct BTVerificationTestParams {
  const char* mInclusionProofHex;
  const char* mExpectedRootHashHex;
  const char* mInputHex;
  pkix::DigestAlgorithm mDigestAlgorithm;
  pkix::Result mExpectedVerificationResult;
};

class BTVerificationTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<BTVerificationTestParams> {
  void SetUp() override {
    if (!NSS_IsInitialized()) {
      if (NSS_NoDB_Init(nullptr) != SECSuccess) {
        abort();
      }
    }
  }
};

// This comes from testing/mozharness/test/test_mozilla_merkle.py
static const char* kValidInclusionProofHex =
    "020000"
    "0000000000000007"  // 7 total nodes
    "0000000000000005"  // leaf index is 5
    "0063"              // 99 (in base 10) bytes follow
    "20"                // 32 byte hash
    "2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c"
    "20"
    "bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf"
    "20"
    "380d0dc6fd7d4f37859a12dbfc7171b3cce29ab0688c6cffd2b15f3e0b21af49";

static const char* kExpectedRootHashHex =
    "d1a0d3947db4ae8305f2ac32985957e02659b2ea3c10da52a48d2526e9af3bbc";

static const char* kInputHex = "81aae91cf90be172eedd1c75c349bf9e";
static const char* kAlteredInputHex = "81aae91c0000000000000000c349bf9e";

// Same as kValidInclusionProofHex, but missing the last hash (the proof is
// still structurally valid, so it decodes successfully).
static const char* kShortInclusionProofHex =
    "020000"
    "0000000000000007"  // 7 total nodes
    "0000000000000005"  // leaf index is 5
    "0042"              // 66 (in base 10) bytes follow
    "20"                // 32 byte hash
    "2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c"
    "20"
    "bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf";

// Same as kValidInclusionProofHex, but has too many hashes (the proof is
// still structurally valid, so it decodes successfully).
static const char* kLongInclusionProofHex =
    "020000"
    "0000000000000007"  // 7 total nodes
    "0000000000000005"  // leaf index is 5
    "0084"              // 132 (in base 10) bytes follow
    "20"                // 32 byte hash
    "2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c"
    "20"
    "bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf"
    "20"
    "380d0dc6fd7d4f37859a12dbfc7171b3cce29ab0688c6cffd2b15f3e0b21af49"
    "20"
    "abbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabba";

static const char* kIncorrectRootHashHex =
    "beebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeebeee";

static const char* kWrongTreeSizeInclusionProofHex =
    "020000"
    "0000000000000009"  // 9 total nodes (this is wrong)
    "0000000000000005"  // leaf index is 5
    "0063"              // 99 (in base 10) bytes follow
    "20"                // 32 byte hash
    "2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c"
    "20"
    "bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf"
    "20"
    "380d0dc6fd7d4f37859a12dbfc7171b3cce29ab0688c6cffd2b15f3e0b21af49";

static const char* kWrongLeafIndexInclusionProofHex =
    "020000"
    "0000000000000007"  // 7 total nodes
    "0000000000000002"  // leaf index is 2 (this is wrong)
    "0063"              // 99 (in base 10) bytes follow
    "20"                // 32 byte hash
    "2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c"
    "20"
    "bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf"
    "20"
    "380d0dc6fd7d4f37859a12dbfc7171b3cce29ab0688c6cffd2b15f3e0b21af49";

// This is a longer proof taken from the Firefox 62.0.2 release.
static const char* kValidInclusionProof2Hex =
    "020000"
    "0000000000000cb3"
    "00000000000000e2"
    "018c"
    "20"
    "6d3d060c72c28cd1967a6ce4e87a470c266981ee8beb6e5956745b5839120338"
    "20"
    "cd028053c51f52447cd2b1ffea7761be546b72832d776db155008dc3071231eb"
    "20"
    "36167b32f7ba4980257c79ed65d133d8c53efbfd7addaa391a121953c78b2b98"
    "20"
    "f2b4abf09140b05058a74e25f4008157655e73fa4aeece9b3f2a40dbbae95c16"
    "20"
    "a2be4813b2ca0e7bb755ed5459789c25a83465fb3f2808590c04c64cdbb404ef"
    "20"
    "43e4341b74ea11f075f35761b867cb256e9d42c4c1253d6b5c958174daa800da"
    "20"
    "1c925ec8d2d4014c0b84d7bed8065d9e7b326e3e4eaebc90d7a6a66431249851"
    "20"
    "bd275e9e4e48c3725f6ef68c2661d4d6ae415129cfb0ea0881177f5347d683ae"
    "20"
    "01e84996a7cc1719b19a4a7e48941657b125bcd0dfd03d183daff87de6a480c3"
    "20"
    "664b6962909692980661be55e8bdadc48350b2461d37372457e8ff97824ae8da"
    "20"
    "39f7ab1500a58643fccc9e07ed409be4c707c5536b95c57f6716207656464019"
    "20"
    "d68c856664d660456d4c85498c2cc2abfb955a63bd1fb23f3e25850da80153f6";

static const char* kExpectedRootHash2Hex =
    "0d489b16a64b80035a8d08ce55549c89c4b67be64a0456382cc5be1e51ddae1a";

// This the hex encoding of the SHA-256 hash of
// linux-x86_64/en-US/firefox-62.0.2.tar.bz2, encoded (again) in hex (because
// that's how generate-checksums.py currently builds the binary transparency
// tree).
static const char* kInput2Hex =
    "3231623461306361626538663030306261663236373235643530313462386434"
    "3865353561333832336538653665656131343638313062313031623034396536";

// This is the hex encoding of an inclusion proof for
// update/win32/en-GB/firefox-61.0-62.0.2.partial.mar using SHA-512.
static const char* kValidInclusionProofSHA512Hex =
    "020000"
    "0000000000000cb3"
    "00000000000007ac"
    "030c"
    "40"
    "b39fbe359108724ec1daed2a8c88511c6b94a74563bfaaf25b774286ab03cab9"
    "b2e64bbd4eeb02003b7397cd814b5d18a160e14ad08eca04559abe146201e027"
    "40"
    "de119745a41884ed482a41638cc1979d89e1bd0ca295e0c412e7ee4b5c2e042d"
    "a1f4aa9c7f1e58b78fac10400b207197383177401976ef6baa80d482c102a94f"
    "40"
    "d0fe203b0e95cbe48e2e663538aba382718c85ae167d8c4ccf326519f81b3860"
    "55058f6a49943b8ed228af88b3414ec00e939ad8dc736f502ca90d63494eddcd"
    "40"
    "ae825352bbe12845fb1d3be27375c27dd462ebd6dc6d7bd0b5929df97aaccf1a"
    "9eedb304ae1e9742b1e2a2ba8b22408d2d52e3bb3d547632d5f03cfe112572d2"
    "40"
    "1ab0f7f7d674c691cf8e51d4409d3402d94927231b82f7f71f16828dd9e36152"
    "1a69f7e571df06aea16c83df88dc74042444ae8771375a7118477275395b570d"
    "40"
    "9c6b94c5be456c68518753cf626daf11b30ce981e18b76c036664dcafeba19b7"
    "3f5ee20ab35aa38b11c92e2c8f103d1b19113b7171c615ce0b9c5ff520242d8c"
    "40"
    "d395ef3cd63ae617a96bd6c5c361d5f6aaccff1ed90599115394310dfa0c528d"
    "611998d325f374c01a827ee900d72ed66d299845438bba565ba666c588b232a0"
    "40"
    "ce3548561f5ec7a94a6c7a2adc604f5e32869d0727cee80f12c95cab2a996965"
    "6ad61a02b35e0fb2d0031ea496381f1413d33b8cbfdf092accd903881e083ee0"
    "40"
    "a4247b649fee9017d9c9e729c4763719b7155056013abc488de0079dc7bddce6"
    "57a93f2596967cfb123d812a901bc4ffe0338cbecf30adc1a821e7261f88e3cd"
    "40"
    "7d2056ea6645b0306373fa2e26fbc4a4ccd4c82cb7d605d2372fa297654cd3a0"
    "b151f6b0fffe7472661de79c7ce7873cb75e05cad5ea40173be540e16d51414d"
    "40"
    "eed2c98b3d8b8035777af74a0d5dd38963f84c09e6f42b6ca4e290674c175035"
    "b9bc85bb4304fcf3d042c444f951b3b134563c6936a1b176679e35e94c42416b"
    "40"
    "6167cccd9d03909f95bb389936766340234b5b3e35dfa8c81d330db9055d7b7d"
    "bfaf262f1821b4a68f43af62c53987550b4174eaab8357f9bd83f2715c98d441";

// This is the hex encoding of the expected root hash of the SHA-512 tree.
static const char* kExpectedRootHashSHA512Hex =
    "6f20812da2980393767e98dd7f79d89aa41e029c790d3908739c02c01d9c6525"
    "8bb3e24ab608c95b063259620751ce258930d48929cbe543b166c33c4de65e60";

// This the hex encoding of the SHA-512 hash of
// update/win32/en-GB/firefox-61.0-62.0.2.partial.mar, encoded (again) in hex.
static const char* kInputSHA512Hex =
    "6237656563626164353363386432346230383763666437323333303162326437"
    "6234326136323536663462636336356134623231636437376133613830613734"
    "3865396533623666306432333839666234623763396563356638643833373534"
    "6239313635313165333661373238653863303236376564653761666561316638";

static const BTVerificationTestParams BT_VERIFICATION_TEST_PARAMS[] = {
    {kValidInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, Success},
    {kValidInclusionProofHex, kExpectedRootHashHex, kAlteredInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kShortInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kLongInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kValidInclusionProofHex, kIncorrectRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kWrongTreeSizeInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kWrongLeafIndexInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
    {kValidInclusionProofHex, kExpectedRootHashHex, kInputHex,
     pkix::DigestAlgorithm::sha512, pkix::Result::ERROR_BAD_SIGNATURE},
    {kValidInclusionProof2Hex, kExpectedRootHash2Hex, kInput2Hex,
     pkix::DigestAlgorithm::sha256, Success},
    {kValidInclusionProofSHA512Hex, kExpectedRootHashSHA512Hex, kInputSHA512Hex,
     pkix::DigestAlgorithm::sha512, Success},
    {kValidInclusionProofSHA512Hex, kExpectedRootHashSHA512Hex, kInputSHA512Hex,
     pkix::DigestAlgorithm::sha256, pkix::Result::ERROR_BAD_SIGNATURE},
};

TEST_P(BTVerificationTest, BTVerificationSimpleTest) {
  const BTVerificationTestParams& params(GetParam());

  Buffer encodedProofBuffer(HexToBytes(params.mInclusionProofHex));
  Input encodedProof = InputForBuffer(encodedProofBuffer);
  InclusionProofDataV2 ipr;
  ASSERT_EQ(Success, DecodeInclusionProof(encodedProof, ipr));

  Buffer leafEntryBuffer(HexToBytes(params.mInputHex));
  Input leafEntry = InputForBuffer(leafEntryBuffer);
  Buffer expectedRootHashBuffer(HexToBytes(params.mExpectedRootHashHex));
  Input expectedRootHash = InputForBuffer(expectedRootHashBuffer);
  ASSERT_EQ(params.mExpectedVerificationResult,
            VerifyInclusionProof(ipr, leafEntry, expectedRootHash,
                                 params.mDigestAlgorithm));
}

INSTANTIATE_TEST_SUITE_P(BTVerificationTest, BTVerificationTest,
                         testing::ValuesIn(BT_VERIFICATION_TEST_PARAMS));

}  // namespace ct
}  // namespace mozilla
