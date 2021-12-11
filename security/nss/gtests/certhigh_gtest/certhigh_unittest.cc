/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "gtest/gtest.h"

#include "cert.h"
#include "certt.h"
#include "secitem.h"

namespace nss_test {

class CERT_FormatNameUnitTest : public ::testing::Test {};

TEST_F(CERT_FormatNameUnitTest, Overflow) {
  // Construct a CERTName consisting of a single RDN with 20 organizational unit
  // AVAs and 20 domain component AVAs. The actual contents don't matter, just
  // the types.

  uint8_t oidValueBytes[] = {0x0c, 0x02, 0x58, 0x58};  // utf8String "XX"
  SECItem oidValue = {siBuffer, oidValueBytes, sizeof(oidValueBytes)};
  uint8_t oidTypeOUBytes[] = {0x55, 0x04, 0x0b};  // organizationalUnit
  SECItem oidTypeOU = {siBuffer, oidTypeOUBytes, sizeof(oidTypeOUBytes)};
  CERTAVA ouAVA = {oidTypeOU, oidValue};
  uint8_t oidTypeDCBytes[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                              0xf2, 0x2c, 0x64, 0x1,  0x19};  // domainComponent
  SECItem oidTypeDC = {siBuffer, oidTypeDCBytes, sizeof(oidTypeDCBytes)};
  CERTAVA dcAVA = {oidTypeDC, oidValue};

  const int kNumEachAVA = 20;
  CERTAVA* avas[(2 * kNumEachAVA) + 1];
  for (int i = 0; i < kNumEachAVA; i++) {
    avas[2 * i] = &ouAVA;
    avas[(2 * i) + 1] = &dcAVA;
  }
  avas[2 * kNumEachAVA] = nullptr;

  CERTRDN rdn = {avas};
  CERTRDN* rdns[2];
  rdns[0] = &rdn;
  rdns[1] = nullptr;

  std::string expectedResult =
      "XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>"
      "XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>"
      "XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>XX<br>"
      "XX<br>XX<br>XX<br>XX<br>";

  CERTName name = {nullptr, rdns};
  char* result = CERT_FormatName(&name);
  EXPECT_EQ(expectedResult, result);
  PORT_Free(result);
}

}  // namespace nss_test
