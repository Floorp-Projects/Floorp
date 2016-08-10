/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTLogVerifier.h"
#include "CTTestUtils.h"
#include "nss.h"

#include "gtest/gtest.h"

namespace mozilla { namespace ct {

using namespace pkix;

class CTLogVerifierTest : public ::testing::Test
{
public:
  void SetUp() override
  {
    // Does nothing if NSS is already initialized.
    MOZ_RELEASE_ASSERT(NSS_NoDB_Init(nullptr) == SECSuccess);

    ASSERT_EQ(Success, mLog.Init(InputForBuffer(GetTestPublicKey())));
    ASSERT_EQ(GetTestPublicKeyId(), mLog.keyId());
  }

protected:
  CTLogVerifier mLog;
};

TEST_F(CTLogVerifierTest, VerifiesCertSCT)
{
  LogEntry certEntry;
  GetX509CertLogEntry(certEntry);

  SignedCertificateTimestamp certSct;
  GetX509CertSCT(certSct);

  EXPECT_EQ(Success, mLog.Verify(certEntry, certSct));
}

TEST_F(CTLogVerifierTest, VerifiesPrecertSCT)
{
  LogEntry precertEntry;
  GetPrecertLogEntry(precertEntry);

  SignedCertificateTimestamp precertSct;
  GetPrecertSCT(precertSct);

  EXPECT_EQ(Success, mLog.Verify(precertEntry, precertSct));
}

TEST_F(CTLogVerifierTest, FailsInvalidTimestamp)
{
  LogEntry certEntry;
  GetX509CertLogEntry(certEntry);

  SignedCertificateTimestamp certSct;
  GetX509CertSCT(certSct);

  // Mangle the timestamp, so that it should fail signature validation.
  certSct.timestamp = 0;

  EXPECT_EQ(Result::ERROR_BAD_SIGNATURE, mLog.Verify(certEntry, certSct));
}

TEST_F(CTLogVerifierTest, FailsInvalidSignature)
{
  LogEntry certEntry;
  GetX509CertLogEntry(certEntry);

  // Mangle the signature, making VerifyECDSASignedDigestNSS (used by
  // CTLogVerifier) return ERROR_BAD_SIGNATURE.
  SignedCertificateTimestamp certSct;
  GetX509CertSCT(certSct);
  certSct.signature.signatureData[20] ^= '\xFF';
  EXPECT_EQ(Result::ERROR_BAD_SIGNATURE, mLog.Verify(certEntry, certSct));

  // Make VerifyECDSASignedDigestNSS return ERROR_BAD_DER. We still expect
  // the verifier to return ERROR_BAD_SIGNATURE.
  SignedCertificateTimestamp certSct2;
  GetX509CertSCT(certSct2);
  certSct2.signature.signatureData[0] ^= '\xFF';
  EXPECT_EQ(Result::ERROR_BAD_SIGNATURE, mLog.Verify(certEntry, certSct2));
}

TEST_F(CTLogVerifierTest, FailsInvalidLogID)
{
  LogEntry certEntry;
  GetX509CertLogEntry(certEntry);

  SignedCertificateTimestamp certSct;
  GetX509CertSCT(certSct);

  // Mangle the log ID, which should cause it to match a different log before
  // attempting signature validation.
  MOZ_RELEASE_ASSERT(certSct.logId.append('\x0'));

  EXPECT_EQ(Result::FATAL_ERROR_INVALID_ARGS, mLog.Verify(certEntry, certSct));
}

TEST_F(CTLogVerifierTest, VerifiesValidSTH)
{
  SignedTreeHead sth;
  GetSampleSignedTreeHead(sth);
  EXPECT_EQ(Success, mLog.VerifySignedTreeHead(sth));
}

TEST_F(CTLogVerifierTest, DoesNotVerifyInvalidSTH)
{
  SignedTreeHead sth;
  GetSampleSignedTreeHead(sth);
  sth.sha256RootHash[0] ^= '\xFF';
  EXPECT_EQ(Result::ERROR_BAD_SIGNATURE, mLog.VerifySignedTreeHead(sth));
}

// Test that excess data after the public key is rejected.
TEST_F(CTLogVerifierTest, ExcessDataInPublicKey)
{
  Buffer key = GetTestPublicKey();
  MOZ_RELEASE_ASSERT(key.append("extra", 5));

  CTLogVerifier log;
  EXPECT_NE(Success, log.Init(InputForBuffer(key)));
}

} }  // namespace mozilla::ct
