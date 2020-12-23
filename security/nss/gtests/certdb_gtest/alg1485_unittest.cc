/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "gtest/gtest.h"

#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "prprf.h"

namespace nss_test {

typedef struct AVATestValuesStr {
  std::string avaString;
  bool expectedResult;
} AVATestValues;

typedef struct AVACompareValuesStr {
  std::string avaString1;
  std::string avaString2;
  SECComparison expectedResult;
} AVACompareValues;

class Alg1485Test : public ::testing::Test {};

class Alg1485ParseTest : public Alg1485Test,
                         public ::testing::WithParamInterface<AVATestValues> {};

class Alg1485CompareTest
    : public Alg1485Test,
      public ::testing::WithParamInterface<AVACompareValues> {};

static const AVATestValues kAVATestStrings[] = {
    {"CN=Marshall T. Rose, O=Dover Beach Consulting, L=Santa Clara, "
     "ST=California, C=US",
     true},
    {"C=HU,L=Budapest,O=Organization,CN=Example - Qualified Citizen "
     "CA,2.5.4.97=VATHU-10",
     true},
    {"C=HU,L=Budapest,O=Example,CN=Example - Qualified Citizen "
     "CA,OID.2.5.4.97=VATHU-10",
     true},
    {"CN=Somebody,L=Set,O=Up,C=US,1=The,2=Bomb", true},
    {"OID.2.5.4.6=😑", true},
    {"2.5.4.6=😑", true},
    {"OID.moocow=😑", false},      // OIDs must be numeric
    {"3.2=bad", false},           // OIDs cannot be overly large; 3 is too big
    {"256.257=bad", false},       // Still too big
    {"YO=LO", false},             // Unknown Tag, 'YO'
    {"CN=Tester,ZZ=Top", false},  // Unknown tag, 'ZZ'
    // These tests are disabled pending Bug 1363416
    // { "01.02.03=Nope", false }, // Numbers not in minimal form
    // { "000001.0000000001=👌", false },
    // { "CN=Somebody,L=Set,O=Up,C=US,01=The,02=Bomb", false },
};

static const AVACompareValues kAVACompareStrings[] = {
    {"CN=Max, O=Mozilla, ST=Berlin", "CN=Max, O=Mozilla, ST=Berlin, C=DE",
     SECLessThan},
    {"CN=Max, O=Mozilla, ST=Berlin, C=DE", "CN=Max, O=Mozilla, ST=Berlin",
     SECGreaterThan},
    {"CN=Max, O=Mozilla, ST=Berlin, C=DE", "CN=Max, O=Mozilla, ST=Berlin, C=DE",
     SECEqual},
    {"CN=Max1, O=Mozilla, ST=Berlin, C=DE",
     "CN=Max2, O=Mozilla, ST=Berlin, C=DE", SECLessThan},
    {"CN=Max, O=Mozilla, ST=Berlin, C=DE", "CN=Max, O=Mozilla, ST=Berlin, C=US",
     SECLessThan},
};

TEST_P(Alg1485ParseTest, TryParsingAVAStrings) {
  const AVATestValues& param(GetParam());

  ScopedCERTName certName(CERT_AsciiToName(param.avaString.c_str()));
  ASSERT_EQ(certName != nullptr, param.expectedResult);
}

TEST_P(Alg1485CompareTest, CompareAVAStrings) {
  const AVACompareValues& param(GetParam());
  ScopedCERTName a(CERT_AsciiToName(param.avaString1.c_str()));
  ScopedCERTName b(CERT_AsciiToName(param.avaString2.c_str()));
  ASSERT_TRUE(a && b);
  EXPECT_EQ(param.expectedResult, CERT_CompareName(a.get(), b.get()));
}

INSTANTIATE_TEST_SUITE_P(ParseAVAStrings, Alg1485ParseTest,
                         ::testing::ValuesIn(kAVATestStrings));
INSTANTIATE_TEST_SUITE_P(CompareAVAStrings, Alg1485CompareTest,
                         ::testing::ValuesIn(kAVACompareStrings));

TEST_F(Alg1485Test, ShortOIDTest) {
  // This is not a valid OID (too short). CERT_GetOidString should return 0.
  unsigned char data[] = {0x05};
  const SECItem oid = {siBuffer, data, sizeof(data)};
  char* result = CERT_GetOidString(&oid);
  EXPECT_EQ(result, nullptr);
}

TEST_F(Alg1485Test, BrokenOIDTest) {
  // This is not a valid OID (first bit of last byte is not set).
  // CERT_GetOidString should return 0.
  unsigned char data[] = {0x81, 0x82, 0x83, 0x84};
  const SECItem oid = {siBuffer, data, sizeof(data)};
  char* result = CERT_GetOidString(&oid);
  EXPECT_EQ(15U, strlen(result));
  EXPECT_EQ(0, strncmp("OID.UNSUPPORTED", result, 15));
  PR_smprintf_free(result);
}
}
