/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secutil.h"

#include "gtest/gtest.h"
#include "scoped_ptrs.h"

namespace nss_test {

class DERIntegerDecodingTest : public ::testing::Test {
 public:
  void TestGetInteger(long number, unsigned char *der_number,
                      unsigned int len) {
    SECItem input = {siBuffer, der_number, len};
    EXPECT_EQ(number, DER_GetInteger(&input));
  }

  void GetDerLongMax(unsigned char *der_number, unsigned int len) {
    der_number[0] = 0x7F;
    for (unsigned int i = 1; i < len; ++i) {
      der_number[i] = 0xFF;
    }
  }

  void GetDerLongMin(unsigned char *der_number, unsigned int len) {
    der_number[0] = 0x80;
    for (unsigned int i = 1; i < len; ++i) {
      der_number[i] = 0x00;
    }
  }
};

TEST_F(DERIntegerDecodingTest, DecodeLongMinus126) {
  unsigned char der[] = {0x82};
  TestGetInteger(-126, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLong130) {
  unsigned char der[] = {0x00, 0x82};
  TestGetInteger(130, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLong0) {
  unsigned char der[] = {0x00};
  TestGetInteger(0, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLong1) {
  unsigned char der[] = {0x01};
  TestGetInteger(1, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMinus1) {
  unsigned char der[] = {0xFF};
  TestGetInteger(-1, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMax) {
  unsigned char der[sizeof(long)];
  GetDerLongMax(der, sizeof(long));
  TestGetInteger(LONG_MAX, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMin) {
  unsigned char der[sizeof(long)];
  GetDerLongMin(der, sizeof(long));
  TestGetInteger(LONG_MIN, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMaxMinus1) {
  unsigned char der[sizeof(long)];
  GetDerLongMax(der, sizeof(long));
  der[sizeof(long) - 1] = 0xFE;
  TestGetInteger(LONG_MAX - 1, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMinPlus1) {
  unsigned char der[sizeof(long)];
  GetDerLongMin(der, sizeof(long));
  der[sizeof(long) - 1] = 0x01;
  TestGetInteger(LONG_MIN + 1, der, sizeof(der));
}

TEST_F(DERIntegerDecodingTest, DecodeLongMinMinus1) {
  unsigned char der[sizeof(long) + 1];
  GetDerLongMax(der, sizeof(long) + 1);
  der[0] = 0xFF;
  der[1] = 0x7F;
  TestGetInteger(LONG_MIN, der, sizeof(der));
  EXPECT_EQ(SEC_ERROR_BAD_DER, PORT_GetError());
}

TEST_F(DERIntegerDecodingTest, DecodeLongMaxPlus1) {
  unsigned char der[sizeof(long) + 1];
  GetDerLongMin(der, sizeof(long) + 1);
  der[0] = 0x00;
  der[1] = 0x80;
  TestGetInteger(LONG_MAX, der, sizeof(der));
  EXPECT_EQ(SEC_ERROR_BAD_DER, PORT_GetError());
}

}  // namespace nss_test
