/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTLogVerifier.h"
#include "CTObjectsExtractor.h"
#include "CTSerialization.h"
#include "CTTestUtils.h"
#include "gtest/gtest.h"
#include "nss.h"

namespace mozilla { namespace ct {

using namespace pkix;

class CTObjectsExtractorTest : public ::testing::Test
{
public:
  void SetUp() override
  {
    // Does nothing if NSS is already initialized.
    MOZ_RELEASE_ASSERT(NSS_NoDB_Init(nullptr) == SECSuccess);

    mTestCert = GetDEREncodedX509Cert();
    mEmbeddedCert = GetDEREncodedTestEmbeddedCert();
    mCaCert = GetDEREncodedCACert();
    mCaCertSPKI = ExtractCertSPKI(mCaCert);

    Buffer logPublicKey = GetTestPublicKey();
    ASSERT_EQ(Success, mLog.Init(InputForBuffer(logPublicKey),
                                 -1 /*operator id*/,
                                 CTLogStatus::Included,
                                 0 /*disqualification time*/));
  }

protected:
  Buffer mTestCert;
  Buffer mEmbeddedCert;
  Buffer mCaCert;
  Buffer mCaCertSPKI;
  CTLogVerifier mLog;
};

TEST_F(CTObjectsExtractorTest, ExtractPrecert)
{
  LogEntry entry;
  ASSERT_EQ(Success,
            GetPrecertLogEntry(InputForBuffer(mEmbeddedCert),
                               InputForBuffer(mCaCertSPKI),
                               entry));

  EXPECT_EQ(LogEntry::Type::Precert, entry.type);
  // Should have empty leaf cert for this log entry type.
  EXPECT_TRUE(entry.leafCertificate.empty());
  EXPECT_EQ(GetDefaultIssuerKeyHash(), entry.issuerKeyHash);
  EXPECT_EQ(GetDEREncodedTestTbsCert(), entry.tbsCertificate);
}

TEST_F(CTObjectsExtractorTest, ExtractOrdinaryX509Cert)
{
  LogEntry entry;
  ASSERT_EQ(Success, GetX509LogEntry(InputForBuffer(mTestCert), entry));

  EXPECT_EQ(LogEntry::Type::X509, entry.type);
  // Should have empty tbsCertificate / issuerKeyHash for this log entry type.
  EXPECT_TRUE(entry.tbsCertificate.empty());
  EXPECT_TRUE(entry.issuerKeyHash.empty());
  // Length of leafCertificate should be 718, see the CT Serialization tests.
  EXPECT_EQ(718U, entry.leafCertificate.length());
}

// Test that an externally-provided SCT verifies over the LogEntry
// of a regular X.509 Certificate
TEST_F(CTObjectsExtractorTest, ComplementarySCTVerifies)
{
  SignedCertificateTimestamp sct;
  GetX509CertSCT(sct);

  LogEntry entry;
  ASSERT_EQ(Success, GetX509LogEntry(InputForBuffer(mTestCert), entry));
  EXPECT_EQ(Success, mLog.Verify(entry, sct));
}

} }  // namespace mozilla::ct
