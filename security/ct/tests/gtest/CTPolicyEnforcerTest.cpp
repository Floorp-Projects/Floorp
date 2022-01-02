/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTPolicyEnforcer.h"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>

#include "CTLogVerifier.h"
#include "CTVerifyResult.h"
#include "SignedCertificateTimestamp.h"
#include "mozpkix/Time.h"
#include "gtest/gtest.h"
#include "hasht.h"
#include "prtime.h"

// Implemented in CertVerifier.cpp.
extern mozilla::pkix::Result GetCertLifetimeInFullMonths(
    mozilla::pkix::Time certNotBefore, mozilla::pkix::Time certNotAfter,
    size_t& months);

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

class CTPolicyEnforcerTest : public ::testing::Test {
 public:
  void SetUp() override {
    OPERATORS_1_AND_2.push_back(OPERATOR_1);
    OPERATORS_1_AND_2.push_back(OPERATOR_2);
  }

  void GetLogId(Buffer& logId, size_t logNo) {
    logId.resize(SHA256_LENGTH);
    std::fill(logId.begin(), logId.end(), 0);
    // Just raw-copy |logId| into the output buffer.
    assert(sizeof(logNo) <= logId.size());
    memcpy(logId.data(), &logNo, sizeof(logNo));
  }

  void AddSct(VerifiedSCTList& verifiedScts, size_t logNo,
              CTLogOperatorId operatorId, VerifiedSCT::Origin origin,
              uint64_t timestamp,
              VerifiedSCT::Status status = VerifiedSCT::Status::Valid) {
    VerifiedSCT verifiedSct;
    verifiedSct.status = status;
    verifiedSct.origin = origin;
    verifiedSct.logOperatorId = operatorId;
    verifiedSct.logDisqualificationTime =
        status == VerifiedSCT::Status::ValidFromDisqualifiedLog
            ? DISQUALIFIED_AT
            : UINT64_MAX;
    verifiedSct.sct.version = SignedCertificateTimestamp::Version::V1;
    verifiedSct.sct.timestamp = timestamp;
    Buffer logId;
    GetLogId(logId, logNo);
    verifiedSct.sct.logId = std::move(logId);
    verifiedScts.push_back(std::move(verifiedSct));
  }

  void AddMultipleScts(
      VerifiedSCTList& verifiedScts, size_t logsCount, uint8_t operatorsCount,
      VerifiedSCT::Origin origin, uint64_t timestamp,
      VerifiedSCT::Status status = VerifiedSCT::Status::Valid) {
    for (size_t logNo = 0; logNo < logsCount; logNo++) {
      CTLogOperatorId operatorId = logNo % operatorsCount;
      AddSct(verifiedScts, logNo, operatorId, origin, timestamp, status);
    }
  }

  void CheckCompliance(const VerifiedSCTList& verifiedSct,
                       size_t certLifetimeInCalendarMonths,
                       const CTLogOperatorList& dependentLogOperators,
                       CTPolicyCompliance expectedCompliance) {
    CTPolicyCompliance compliance;
    mPolicyEnforcer.CheckCompliance(verifiedSct, certLifetimeInCalendarMonths,
                                    dependentLogOperators, compliance);
    EXPECT_EQ(expectedCompliance, compliance);
  }

 protected:
  CTPolicyEnforcer mPolicyEnforcer;

  const size_t LOG_1 = 1;
  const size_t LOG_2 = 2;
  const size_t LOG_3 = 3;
  const size_t LOG_4 = 4;
  const size_t LOG_5 = 5;

  const CTLogOperatorId OPERATOR_1 = 1;
  const CTLogOperatorId OPERATOR_2 = 2;
  const CTLogOperatorId OPERATOR_3 = 3;

  CTLogOperatorList NO_OPERATORS;
  CTLogOperatorList OPERATORS_1_AND_2;

  const VerifiedSCT::Origin ORIGIN_EMBEDDED = VerifiedSCT::Origin::Embedded;
  const VerifiedSCT::Origin ORIGIN_TLS = VerifiedSCT::Origin::TLSExtension;
  const VerifiedSCT::Origin ORIGIN_OCSP = VerifiedSCT::Origin::OCSPResponse;

  // 4 years of cert lifetime requires 5 SCTs for the embedded case.
  const size_t DEFAULT_MONTHS = 4 * 12L;

  // Date.parse("2015-08-15T00:00:00Z")
  const uint64_t TIMESTAMP_1 = 1439596800000L;

  // Date.parse("2016-04-15T00:00:00Z")
  const uint64_t DISQUALIFIED_AT = 1460678400000L;

  // Date.parse("2016-04-01T00:00:00Z")
  const uint64_t BEFORE_DISQUALIFIED = 1459468800000L;

  // Date.parse("2016-04-16T00:00:00Z")
  const uint64_t AFTER_DISQUALIFIED = 1460764800000L;
};

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformNotEnoughDiverseNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, OPERATORS_1_AND_2,
                  CTPolicyCompliance::NotDiverseScts);
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithEmbeddedSCTs) {
  VerifiedSCTList scts;

  // 5 embedded SCTs required for DEFAULT_MONTHS.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_4, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_5, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformNotEnoughDiverseEmbeddedSCTs) {
  VerifiedSCTList scts;

  // 5 embedded SCTs required for DEFAULT_MONTHS.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_4, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_5, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, OPERATORS_1_AND_2,
                  CTPolicyCompliance::NotDiverseScts);
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithPooledNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_OCSP, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithPooledEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_OCSP, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughFreshSCTs) {
  VerifiedSCTList scts;

  // The results should be the same before and after disqualification,
  // regardless of the delivery method.

  // SCT from before disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, BEFORE_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);
  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
  // SCT from after disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, AFTER_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);
  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);

  // Embedded SCT from before disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);
  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);

  // Embedded SCT from after disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);
  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       ConformsWithDisqualifiedLogBeforeDisqualificationDate) {
  VerifiedSCTList scts;

  // 5 embedded SCTs required for DEFAULT_MONTHS.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_4, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_5, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformWithDisqualifiedLogAfterDisqualificationDate) {
  VerifiedSCTList scts;

  // 5 embedded SCTs required for DEFAULT_MONTHS.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_4, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_5, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformWithIssuanceDateAfterDisqualificationDate) {
  VerifiedSCTList scts;

  // 5 embedded SCTs required for DEFAULT_MONTHS.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED,
         VerifiedSCT::Status::ValidFromDisqualifiedLog);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED);
  AddSct(scts, LOG_4, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED);
  AddSct(scts, LOG_5, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_DISQUALIFIED);

  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformToCTPolicyNotEnoughUniqueEmbeddedLogs) {
  VerifiedSCTList scts;

  // Operator #1
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  // Operator #2, different logs
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);
  // Operator #3, same log
  AddSct(scts, LOG_4, OPERATOR_3, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_4, OPERATOR_3, ORIGIN_EMBEDDED, TIMESTAMP_1);

  // 5 embedded SCTs required. However, only 4 are from distinct logs.
  CheckCompliance(scts, DEFAULT_MONTHS, NO_OPERATORS,
                  CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       ConformsToPolicyExactNumberOfSCTsForValidityPeriod) {
  // Test multiple validity periods.
  const struct TestData {
    size_t certLifetimeInCalendarMonths;
    size_t sctsRequired;
  } kTestData[] = {{3, 2},          {12 + 2, 2},     {12 + 3, 3},
                   {2 * 12 + 2, 3}, {2 * 12 + 3, 4}, {3 * 12 + 2, 4},
                   {3 * 12 + 4, 5}};

  for (size_t i = 0; i < MOZILLA_CT_ARRAY_LENGTH(kTestData); ++i) {
    SCOPED_TRACE(i);

    size_t months = kTestData[i].certLifetimeInCalendarMonths;
    size_t sctsRequired = kTestData[i].sctsRequired;

    // Less SCTs than required is not enough.
    for (size_t sctsAvailable = 0; sctsAvailable < sctsRequired;
         ++sctsAvailable) {
      VerifiedSCTList scts;
      AddMultipleScts(scts, sctsAvailable, 1, ORIGIN_EMBEDDED, TIMESTAMP_1);

      CTPolicyCompliance compliance;
      mPolicyEnforcer.CheckCompliance(scts, months, NO_OPERATORS, compliance);
      EXPECT_EQ(CTPolicyCompliance::NotEnoughScts, compliance)
          << "i=" << i << " sctsRequired=" << sctsRequired
          << " sctsAvailable=" << sctsAvailable;
    }

    // Add exactly the required number of SCTs (from 2 operators).
    VerifiedSCTList scts;
    AddMultipleScts(scts, sctsRequired, 2, ORIGIN_EMBEDDED, TIMESTAMP_1);

    CTPolicyCompliance compliance;
    mPolicyEnforcer.CheckCompliance(scts, months, NO_OPERATORS, compliance);
    EXPECT_EQ(CTPolicyCompliance::Compliant, compliance) << "i=" << i;
  }
}

TEST_F(CTPolicyEnforcerTest, TestEdgeCasesOfGetCertLifetimeInFullMonths) {
  const struct TestData {
    uint64_t notBefore;
    uint64_t notAfter;
    size_t expectedMonths;
  } kTestData[] = {
      {                   // 1 second less than 1 month
       1424863500000000,  // Date.parse("2015-02-25T11:25:00Z") * 1000
       1427196299000000,  // Date.parse("2015-03-24T11:24:59Z") * 1000
       0},
      {                   // exactly 1 month
       1424863500000000,  // Date.parse("2015-02-25T11:25:00Z") * 1000
       1427282700000000,  // Date.parse("2015-03-25T11:25:00Z") * 1000
       1},
      {                   // 1 year, 1 month
       1427282700000000,  // Date.parse("2015-03-25T11:25:00Z") * 1000
       1461583500000000,  // Date.parse("2016-04-25T11:25:00Z") * 1000
       13},
      {// 1 year, 1 month, first day of notBefore month, last of notAfter
       1425209100000000,  // Date.parse("2015-03-01T11:25:00Z") * 1000
       1462015500000000,  // Date.parse("2016-04-30T11:25:00Z") * 1000
       13},
      {// 1 year, adjacent months, last day of notBefore month, first of
       // notAfter
       1427801100000000,  // Date.parse("2015-03-31T11:25:00Z") * 1000
       1459509900000000,  // Date.parse("2016-04-01T11:25:00Z") * 1000
       12}};

  for (size_t i = 0; i < MOZILLA_CT_ARRAY_LENGTH(kTestData); ++i) {
    SCOPED_TRACE(i);

    size_t months;
    ASSERT_EQ(Success,
              GetCertLifetimeInFullMonths(mozilla::pkix::TimeFromEpochInSeconds(
                                              kTestData[i].notBefore / 1000000),
                                          mozilla::pkix::TimeFromEpochInSeconds(
                                              kTestData[i].notAfter / 1000000),
                                          months))
        << "i=" << i;
    EXPECT_EQ(kTestData[i].expectedMonths, months) << "i=" << i;
  }
}

}  // namespace ct
}  // namespace mozilla
