/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiLogCTVerifier.h"

#include <stdint.h>

#include "CTLogVerifier.h"
#include "CTObjectsExtractor.h"
#include "CTSerialization.h"
#include "CTTestUtils.h"
#include "gtest/gtest.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Move.h"
#include "nss.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

class MultiLogCTVerifierTest : public ::testing::Test
{
public:
  void SetUp() override
  {
    // Does nothing if NSS is already initialized.
    MOZ_RELEASE_ASSERT(NSS_NoDB_Init(nullptr) == SECSuccess);

    ASSERT_EQ(Success, mVerifier.AddLog(InputForBuffer(GetTestPublicKey())));

    mTestCert = GetDEREncodedX509Cert();
    mEmbeddedCert = GetDEREncodedTestEmbeddedCert();
    mCaCert = GetDEREncodedCACert();
    mCaCertSPKI = ExtractCertSPKI(mCaCert);
    mIntermediateCert = GetDEREncodedIntermediateCert();
    mIntermediateCertSPKI = ExtractCertSPKI(mIntermediateCert);

    // Set the current time making sure all test timestamps are in the past.
    mNow = UINT64_MAX;
  }

  void CheckForSingleVerifiedSCTInResult(const CTVerifyResult& result,
    SignedCertificateTimestamp::Origin origin)
  {
    EXPECT_EQ(0U, result.decodingErrors);
    EXPECT_TRUE(result.invalidScts.empty());
    EXPECT_TRUE(result.unknownLogsScts.empty());
    ASSERT_EQ(1U, result.verifiedScts.length());
    EXPECT_EQ(origin, result.verifiedScts[0].origin);
  }

  // Writes an SCTList containing a single |sct| into |output|.
  void EncodeSCTListForTesting(Input sct, Buffer& output)
  {
    Vector<Input> list;
    ASSERT_TRUE(list.append(Move(sct)));
    ASSERT_EQ(Success, EncodeSCTList(list, output));
  }

  void GetSCTListWithInvalidLogID(Buffer& result)
  {
    result.clear();
    Buffer sct(GetTestSignedCertificateTimestamp());
    // Change a byte inside the Log ID part of the SCT so it does
    // not match the log used in the tests.
    sct[15] ^= '\xFF';
    EncodeSCTListForTesting(InputForBuffer(sct), result);
  }

  void CheckPrecertVerification(const Buffer& cert, const Buffer& issuerSPKI)
  {
    Buffer sctList;
    ExtractEmbeddedSCTList(cert, sctList);
    ASSERT_FALSE(sctList.empty());

    CTVerifyResult result;
    ASSERT_EQ(Success,
              mVerifier.Verify(InputForBuffer(cert), InputForBuffer(issuerSPKI),
                               InputForBuffer(sctList), Input(), Input(),
                               mNow, result));
    CheckForSingleVerifiedSCTInResult(result,
      SignedCertificateTimestamp::Origin::Embedded);
  }

protected:
  MultiLogCTVerifier mVerifier;
  Buffer mTestCert;
  Buffer mEmbeddedCert;
  Buffer mCaCert;
  Buffer mCaCertSPKI;
  Buffer mIntermediateCert;
  Buffer mIntermediateCertSPKI;
  uint64_t mNow;
};

// Test that an embedded SCT can be extracted and the extracted SCT contains
// the expected data. This tests the ExtractEmbeddedSCTList function from
// CTTestUtils.h that other tests here rely upon.
TEST_F(MultiLogCTVerifierTest, ExtractEmbeddedSCT)
{
  SignedCertificateTimestamp sct;

  // Extract the embedded SCT.

  Buffer sctList;
  ExtractEmbeddedSCTList(mEmbeddedCert, sctList);
  ASSERT_FALSE(sctList.empty());

  Reader sctReader;
  ASSERT_EQ(Success, DecodeSCTList(InputForBuffer(sctList), sctReader));
  Input sctItemInput;
  ASSERT_EQ(Success, ReadSCTListItem(sctReader, sctItemInput));
  EXPECT_TRUE(sctReader.AtEnd()); // we only expect one sct in the list

  Reader sctItemReader(sctItemInput);
  ASSERT_EQ(Success, DecodeSignedCertificateTimestamp(sctItemReader, sct));

  // Make sure the SCT contains the expected data.

  EXPECT_EQ(SignedCertificateTimestamp::Version::V1, sct.version);
  EXPECT_EQ(GetTestPublicKeyId(), sct.logId);

  uint64_t expectedTimestamp = 1365181456275;
  EXPECT_EQ(expectedTimestamp, sct.timestamp);
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCT)
{
  CheckPrecertVerification(mEmbeddedCert, mCaCertSPKI);
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithPreCA)
{
  CheckPrecertVerification(GetDEREncodedTestEmbeddedWithPreCACert(),
                           mCaCertSPKI);
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithIntermediate)
{
  CheckPrecertVerification(GetDEREncodedTestEmbeddedWithIntermediateCert(),
                           mIntermediateCertSPKI);
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithIntermediateAndPreCA)
{
  CheckPrecertVerification(GetDEREncodedTestEmbeddedWithIntermediatePreCACert(),
                           mIntermediateCertSPKI);
}

TEST_F(MultiLogCTVerifierTest, VerifiesSCTFromOCSP)
{
  Buffer sct(GetTestSignedCertificateTimestamp());
  Buffer sctList;
  EncodeSCTListForTesting(InputForBuffer(sct), sctList);

  CTVerifyResult result;
  ASSERT_EQ(Success,
            mVerifier.Verify(InputForBuffer(mTestCert), Input(),
                             Input(), InputForBuffer(sctList), Input(),
                             mNow, result));

  CheckForSingleVerifiedSCTInResult(result,
    SignedCertificateTimestamp::Origin::OCSPResponse);
}

TEST_F(MultiLogCTVerifierTest, VerifiesSCTFromTLS)
{
  Buffer sct(GetTestSignedCertificateTimestamp());
  Buffer sctList;
  EncodeSCTListForTesting(InputForBuffer(sct), sctList);

  CTVerifyResult result;
  ASSERT_EQ(Success,
            mVerifier.Verify(InputForBuffer(mTestCert), Input(),
                             Input(), Input(), InputForBuffer(sctList),
                             mNow, result));

  CheckForSingleVerifiedSCTInResult(result,
    SignedCertificateTimestamp::Origin::TLSExtension);
}

TEST_F(MultiLogCTVerifierTest, VerifiesSCTFromMultipleSources)
{
  Buffer sct(GetTestSignedCertificateTimestamp());
  Buffer sctList;
  EncodeSCTListForTesting(InputForBuffer(sct), sctList);

  CTVerifyResult result;
  ASSERT_EQ(Success,
            mVerifier.Verify(InputForBuffer(mTestCert), Input(), Input(),
                             InputForBuffer(sctList), InputForBuffer(sctList),
                             mNow, result));

  // The result should contain verified SCTs from TLS and OCSP origins.
  EnumSet<SignedCertificateTimestamp::Origin> origins;
  for (auto& sct : result.verifiedScts) {
    origins += sct.origin;
  }
  EXPECT_FALSE(
    origins.contains(SignedCertificateTimestamp::Origin::Embedded));
  EXPECT_TRUE(
    origins.contains(SignedCertificateTimestamp::Origin::OCSPResponse));
  EXPECT_TRUE(
    origins.contains(SignedCertificateTimestamp::Origin::TLSExtension));
}

TEST_F(MultiLogCTVerifierTest, IdentifiesSCTFromUnknownLog)
{
  Buffer sctList;
  GetSCTListWithInvalidLogID(sctList);

  CTVerifyResult result;
  ASSERT_EQ(Success,
            mVerifier.Verify(InputForBuffer(mTestCert), Input(),
                             Input(), Input(), InputForBuffer(sctList),
                             mNow, result));

  EXPECT_EQ(1U, result.unknownLogsScts.length());
  EXPECT_EQ(0U, result.decodingErrors);
}

} } // namespace mozilla::ct
