// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "core/fxcrt/fx_basic.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32_t ReferenceGetBits32(const uint8_t* pData, int bitpos, int nbits) {
  int result = 0;
  for (int i = 0; i < nbits; i++) {
    if (pData[(bitpos + i) / 8] & (1 << (7 - (bitpos + i) % 8)))
      result |= 1 << (nbits - i - 1);
  }
  return result;
}

}  // namespace

TEST(fxge, GetBits32) {
  unsigned char data[] = {0xDE, 0x3F, 0xB1, 0x7C, 0x12, 0x9A, 0x04, 0x56};
  for (int nbits = 1; nbits <= 32; ++nbits) {
    for (int bitpos = 0; bitpos < (int)sizeof(data) * 8 - nbits; ++bitpos) {
      EXPECT_EQ(ReferenceGetBits32(data, bitpos, nbits),
                GetBits32(data, bitpos, nbits));
    }
  }
}

TEST(fxcrt, FX_atonum) {
  int i;
  EXPECT_TRUE(FX_atonum("10", &i));
  EXPECT_EQ(10, i);

  EXPECT_TRUE(FX_atonum("-10", &i));
  EXPECT_EQ(-10, i);

  EXPECT_TRUE(FX_atonum("+10", &i));
  EXPECT_EQ(10, i);

  EXPECT_TRUE(FX_atonum("-2147483648", &i));
  EXPECT_EQ(std::numeric_limits<int>::min(), i);

  EXPECT_TRUE(FX_atonum("2147483647", &i));
  EXPECT_EQ(2147483647, i);

  // Value overflows.
  EXPECT_TRUE(FX_atonum("-2147483649", &i));
  EXPECT_EQ(0, i);

  // Value overflows.
  EXPECT_TRUE(FX_atonum("+2147483648", &i));
  EXPECT_EQ(0, i);

  // Value overflows.
  EXPECT_TRUE(FX_atonum("4223423494965252", &i));
  EXPECT_EQ(0, i);

  // No explicit sign will allow the number to go negative. This is for things
  // like the encryption Permissions flag (Table 3.20 PDF 1.7 spec)
  EXPECT_TRUE(FX_atonum("4294965252", &i));
  EXPECT_EQ(-2044, i);

  EXPECT_TRUE(FX_atonum("-4294965252", &i));
  EXPECT_EQ(0, i);

  EXPECT_TRUE(FX_atonum("+4294965252", &i));
  EXPECT_EQ(0, i);

  float f;
  EXPECT_FALSE(FX_atonum("3.24", &f));
  EXPECT_FLOAT_EQ(3.24f, f);
}
