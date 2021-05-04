// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/base/byte_order.h"

#include "gtest/gtest.h"

namespace jxl {
namespace {

TEST(ByteOrderTest, TestRoundTripBE16) {
  const uint32_t in = 0x1234;
  uint8_t buf[2];
  StoreBE16(in, buf);
  EXPECT_EQ(in, LoadBE16(buf));
  EXPECT_NE(in, LoadLE16(buf));
}

TEST(ByteOrderTest, TestRoundTripLE16) {
  const uint32_t in = 0x1234;
  uint8_t buf[2];
  StoreLE16(in, buf);
  EXPECT_EQ(in, LoadLE16(buf));
  EXPECT_NE(in, LoadBE16(buf));
}

TEST(ByteOrderTest, TestRoundTripBE32) {
  const uint32_t in = 0xFEDCBA98u;
  uint8_t buf[4];
  StoreBE32(in, buf);
  EXPECT_EQ(in, LoadBE32(buf));
  EXPECT_NE(in, LoadLE32(buf));
}

TEST(ByteOrderTest, TestRoundTripLE32) {
  const uint32_t in = 0xFEDCBA98u;
  uint8_t buf[4];
  StoreLE32(in, buf);
  EXPECT_EQ(in, LoadLE32(buf));
  EXPECT_NE(in, LoadBE32(buf));
}

TEST(ByteOrderTest, TestRoundTripLE64) {
  const uint64_t in = 0xFEDCBA9876543210ull;
  uint8_t buf[8];
  StoreLE64(in, buf);
  EXPECT_EQ(in, LoadLE64(buf));
}

}  // namespace
}  // namespace jxl
