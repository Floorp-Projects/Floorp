// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>

#include "core/fxcodec/codec/ccodec_basicmodule.h"
#include "core/fxcodec/fx_codec.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcodec, A85TestBadInputs) {
  const uint8_t src_buf[] = {1, 2, 3, 4};
  uint8_t* dest_buf = nullptr;
  uint32_t src_size = 4;
  uint32_t dest_size = 0;

  CCodec_BasicModule* pEncoders = CCodec_ModuleMgr().GetBasicModule();
  EXPECT_TRUE(pEncoders);

  // Error codes, not segvs, should callers pass us a nullptr pointer.
  EXPECT_FALSE(pEncoders->A85Encode(src_buf, src_size, &dest_buf, nullptr));
  EXPECT_FALSE(pEncoders->A85Encode(src_buf, src_size, nullptr, &dest_size));
  EXPECT_FALSE(pEncoders->A85Encode(src_buf, 0, &dest_buf, &dest_size));
  EXPECT_FALSE(pEncoders->A85Encode(nullptr, src_size, &dest_buf, &dest_size));
}

// No leftover bytes, just translate 2 sets of symbols.
TEST(fxcodec, A85TestBasic) {
  // Make sure really big values don't break.
  const uint8_t src_buf[] = {1, 2, 3, 4, 255, 255, 255, 255};
  uint8_t* dest_buf = nullptr;
  uint32_t src_size = 8;
  uint32_t dest_size = 0;

  CCodec_BasicModule* pEncoders = CCodec_ModuleMgr().GetBasicModule();
  EXPECT_TRUE(pEncoders);

  // Should succeed.
  EXPECT_TRUE(pEncoders->A85Encode(src_buf, src_size, &dest_buf, &dest_size));

  // Should have 5 chars for each set of 4 and 2 terminators.
  ASSERT_EQ(12u, dest_size);
  const uint8_t expected_out[] = {33, 60, 78, 63, 43,  115,
                                  56, 87, 45, 33, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 12; i++)
    EXPECT_EQ(expected_out[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);
}

// Leftover bytes.
TEST(fxcodec, A85TestLeftoverBytes) {
  // 1 Leftover Byte:
  const uint8_t src_buf_1leftover[] = {1, 2, 3, 4, 255};
  uint8_t* dest_buf = nullptr;
  uint32_t src_size = 5;
  uint32_t dest_size = 0;

  CCodec_BasicModule* pEncoders = CCodec_ModuleMgr().GetBasicModule();
  EXPECT_TRUE(pEncoders);

  // Should succeed
  EXPECT_TRUE(
      pEncoders->A85Encode(src_buf_1leftover, src_size, &dest_buf, &dest_size));
  ASSERT_EQ(9u, dest_size);  // 5 chars for first symbol + 2 + 2 terminators.
  uint8_t expected_out_1leftover[] = {33, 60, 78, 63, 43, 114, 114, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 9; i++)
    EXPECT_EQ(expected_out_1leftover[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);

  // 2 Leftover bytes:
  src_size++;
  dest_buf = nullptr;
  dest_size = 0;
  const uint8_t src_buf_2leftover[] = {1, 2, 3, 4, 255, 254};
  // Should succeed
  EXPECT_TRUE(
      pEncoders->A85Encode(src_buf_2leftover, src_size, &dest_buf, &dest_size));
  ASSERT_EQ(10u, dest_size);  // 5 chars for first symbol + 3 + 2 terminators.
  const uint8_t expected_out_2leftover[] = {33,  60, 78, 63,  43,
                                            115, 56, 68, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 10; i++)
    EXPECT_EQ(expected_out_2leftover[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);

  // 3 Leftover bytes:
  src_size++;
  dest_buf = nullptr;
  dest_size = 0;
  const uint8_t src_buf_3leftover[] = {1, 2, 3, 4, 255, 254, 253};
  // Should succeed
  EXPECT_TRUE(
      pEncoders->A85Encode(src_buf_3leftover, src_size, &dest_buf, &dest_size));
  ASSERT_EQ(11u, dest_size);  // 5 chars for first symbol + 4 + 2 terminators.
  const uint8_t expected_out_3leftover[] = {33, 60, 78,  63,  43, 115,
                                            56, 77, 114, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 11; i++)
    EXPECT_EQ(expected_out_3leftover[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);
}

// Test all zeros comes through as "z".
TEST(fxcodec, A85TestZeros) {
  // Make sure really big values don't break.
  const uint8_t src_buf[] = {1, 2, 3, 4, 0, 0, 0, 0};
  uint8_t* dest_buf = nullptr;
  uint32_t src_size = 8;
  uint32_t dest_size = 0;

  CCodec_BasicModule* pEncoders = CCodec_ModuleMgr().GetBasicModule();
  EXPECT_TRUE(pEncoders);

  // Should succeed.
  EXPECT_TRUE(pEncoders->A85Encode(src_buf, src_size, &dest_buf, &dest_size));

  // Should have 5 chars for first set of 4 + 1 for z + 2 terminators.
  ASSERT_EQ(8u, dest_size);
  const uint8_t expected_out[] = {33, 60, 78, 63, 43, 122, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 8; i++)
    EXPECT_EQ(expected_out[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);

  // Should also work if it is at the start:
  dest_buf = nullptr;
  dest_size = 0;
  const uint8_t src_buf_2[] = {0, 0, 0, 0, 1, 2, 3, 4};

  // Should succeed.
  EXPECT_TRUE(pEncoders->A85Encode(src_buf_2, src_size, &dest_buf, &dest_size));

  // Should have 5 chars for set of 4 + 1 for z + 2 terminators.
  ASSERT_EQ(8u, dest_size);
  const uint8_t expected_out_2[] = {122, 33, 60, 78, 63, 43, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 8; i++)
    EXPECT_EQ(expected_out_2[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);

  // Try with 2 leftover zero bytes. Make sure we don't get a "z".
  src_size = 6;  // Cut off the last 2 zeros.
  dest_buf = nullptr;
  dest_size = 0;

  // Should succeed.
  EXPECT_TRUE(pEncoders->A85Encode(src_buf, src_size, &dest_buf, &dest_size));

  // Should have 5 chars for set of 4 + 3 for last 2 + 2 terminators.
  ASSERT_EQ(10u, dest_size);
  const uint8_t expected_out_leftover[] = {33, 60, 78, 63,  43,
                                           33, 33, 33, 126, 62};

  // Check the output
  for (uint32_t i = 0; i < 10; i++)
    EXPECT_EQ(expected_out_leftover[i], dest_buf[i]) << " at " << i;
  FX_Free(dest_buf);
}

// Make sure we get returns in the expected locations.
TEST(fxcodec, A85TestLineBreaks) {
  // Make sure really big values don't break.
  uint8_t src_buf[131] = {0};
  // 1 full line + most of a line of normal symbols.
  for (int k = 0; k < 116; k += 4) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }
  // Fill in the end, leaving an all zero gap + 3 extra zeros at the end.
  for (int k = 120; k < 128; k++) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }
  uint8_t* dest_buf = nullptr;
  uint32_t src_size = 131;
  uint32_t dest_size = 0;

  CCodec_BasicModule* pEncoders = CCodec_ModuleMgr().GetBasicModule();
  EXPECT_TRUE(pEncoders);

  // Should succeed.
  EXPECT_TRUE(pEncoders->A85Encode(src_buf, src_size, &dest_buf, &dest_size));

  // Should have 75 chars in the first row plus 2 char return,
  // 76 chars in the second row plus 2 char return,
  // and 9 chars in the last row with 2 terminators.
  ASSERT_EQ(166u, dest_size);

  // Check for the returns.
  EXPECT_EQ(13, dest_buf[75]);
  EXPECT_EQ(10, dest_buf[76]);
  EXPECT_EQ(13, dest_buf[153]);
  EXPECT_EQ(10, dest_buf[154]);

  FX_Free(dest_buf);
}
