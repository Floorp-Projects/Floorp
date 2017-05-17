/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "gtest/gtest.h"

#include "nss.h"
#include "scoped_ptrs.h"

namespace nss_test {

typedef struct AVATestValuesStr {
  std::string avaString;
  bool expectedResult;
} AVATestValues;

class Alg1485Test : public ::testing::Test,
                    public ::testing::WithParamInterface<AVATestValues> {};

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

TEST_P(Alg1485Test, TryParsingAVAStrings) {
  const AVATestValues& param(GetParam());

  ScopedCERTName certName(CERT_AsciiToName(param.avaString.c_str()));
  ASSERT_EQ(certName != nullptr, param.expectedResult);
}

INSTANTIATE_TEST_CASE_P(ParseAVAStrings, Alg1485Test,
                        ::testing::ValuesIn(kAVATestStrings));
}
