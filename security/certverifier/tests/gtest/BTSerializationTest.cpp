/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BTVerifier.h"
#include "CTTestUtils.h"
#include "gtest/gtest.h"

namespace mozilla { namespace ct {

using namespace pkix;

class BTSerializationTest : public ::testing::Test
{
public:
  void SetUp() override
  {
    mTestInclusionProof = GetTestInclusionProof();
    mTestInclusionProofUnexpectedData = GetTestInclusionProofUnexpectedData();
    mTestInclusionProofInvalidHashSize = GetTestInclusionProofInvalidHashSize();
    mTestInclusionProofInvalidHash = GetTestInclusionProofInvalidHash();
    mTestInclusionProofMissingLogId = GetTestInclusionProofMissingLogId();
    mTestInclusionProofNullPathLength = GetTestInclusionProofNullPathLength();
    mTestInclusionProofPathLengthTooSmall = GetTestInclusionProofPathLengthTooSmall();
    mTestInclusionProofPathLengthTooLarge = GetTestInclusionProofPathLengthTooLarge();
    mTestInclusionProofNullTreeSize = GetTestInclusionProofNullTreeSize();
    mTestInclusionProofLeafIndexOutOfBounds = GetTestInclusionProofLeafIndexOutOfBounds();
    mTestInclusionProofExtraData = GetTestInclusionProofExtraData();
  }

protected:
  Buffer mTestInclusionProof;
  Buffer mTestInclusionProofUnexpectedData;
  Buffer mTestInclusionProofInvalidHashSize;
  Buffer mTestInclusionProofInvalidHash;
  Buffer mTestInclusionProofMissingLogId;
  Buffer mTestInclusionProofNullPathLength;
  Buffer mTestInclusionProofPathLengthTooSmall;
  Buffer mTestInclusionProofPathLengthTooLarge;
  Buffer mTestInclusionProofNullTreeSize;
  Buffer mTestInclusionProofLeafIndexOutOfBounds;
  Buffer mTestInclusionProofExtraData;
};

TEST_F(BTSerializationTest, DecodesInclusionProof)
{
  const uint64_t expectedTreeSize = 4;
  const uint64_t expectedLeafIndex = 2;
  const uint64_t expectedInclusionPathElements = 2;

  const uint8_t EXPECTED_LOGID[] = { 0x01, 0x00 };
  Buffer expectedLogId;
  MOZ_RELEASE_ASSERT(expectedLogId.append(EXPECTED_LOGID, 2));


  Input encodedProofInput = InputForBuffer(mTestInclusionProof);
  Reader encodedProofReader(encodedProofInput);

  InclusionProofDataV2 ipr;
  ASSERT_EQ(Success, DecodeInclusionProof(encodedProofReader, ipr));
  EXPECT_EQ(expectedLogId, ipr.logId);
  EXPECT_EQ(expectedTreeSize, ipr.treeSize);
  EXPECT_EQ(expectedLeafIndex, ipr.leafIndex);
  EXPECT_EQ(expectedInclusionPathElements, ipr.inclusionPath.length());
  EXPECT_EQ(GetTestNodeHash0(), ipr.inclusionPath[0]);
  EXPECT_EQ(GetTestNodeHash1(), ipr.inclusionPath[1]);
}

TEST_F(BTSerializationTest, FailsDecodingInclusionProofUnexpectedData)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofUnexpectedData);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingInvalidHashSize)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofInvalidHashSize);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingInvalidHash)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofInvalidHash);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingMissingLogId)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofMissingLogId);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingNullPathLength)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofNullPathLength);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingPathLengthTooSmall)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofPathLengthTooSmall);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingPathLengthTooLarge)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofPathLengthTooLarge);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingNullTreeSize)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofNullTreeSize);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingLeafIndexOutOfBounds)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofLeafIndexOutOfBounds);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}

TEST_F(BTSerializationTest, FailsDecodingExtraData)
{
  Input encodedProofInput = InputForBuffer(mTestInclusionProofExtraData);
  Reader encodedProofReader(encodedProofInput);
  InclusionProofDataV2 ipr;

  ASSERT_EQ(pkix::Result::ERROR_BAD_DER, DecodeInclusionProof(encodedProofReader, ipr));
}
} } // namespace mozilla::ct
