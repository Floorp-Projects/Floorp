/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/buffer.h"
#include "webrtc/base/gunit.h"

#include <algorithm>  // std::swap (pre-C++11)
#include <utility>  // std::swap (C++11 and later)

namespace rtc {

namespace {

// clang-format off
const uint8_t kTestData[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                             0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
// clang-format on

void TestBuf(const Buffer& b1, size_t size, size_t capacity) {
  EXPECT_EQ(b1.size(), size);
  EXPECT_EQ(b1.capacity(), capacity);
}

}  // namespace

TEST(BufferTest, TestConstructEmpty) {
  TestBuf(Buffer(), 0, 0);
  TestBuf(Buffer(Buffer()), 0, 0);
  TestBuf(Buffer(0), 0, 0);

  // We can't use a literal 0 for the first argument, because C++ will allow
  // that to be considered a null pointer, which makes the call ambiguous.
  TestBuf(Buffer(0 + 0, 10), 0, 10);

  TestBuf(Buffer(kTestData, 0), 0, 0);
  TestBuf(Buffer(kTestData, 0, 20), 0, 20);
}

TEST(BufferTest, TestConstructData) {
  Buffer buf(kTestData, 7);
  EXPECT_EQ(buf.size(), 7u);
  EXPECT_EQ(buf.capacity(), 7u);
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, 7));
}

TEST(BufferTest, TestConstructDataWithCapacity) {
  Buffer buf(kTestData, 7, 14);
  EXPECT_EQ(buf.size(), 7u);
  EXPECT_EQ(buf.capacity(), 14u);
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, 7));
}

TEST(BufferTest, TestConstructArray) {
  Buffer buf(kTestData);
  EXPECT_EQ(buf.size(), 16u);
  EXPECT_EQ(buf.capacity(), 16u);
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, 16));
}

TEST(BufferTest, TestConstructCopy) {
  Buffer buf1(kTestData), buf2(buf1);
  EXPECT_EQ(buf2.size(), 16u);
  EXPECT_EQ(buf2.capacity(), 16u);
  EXPECT_EQ(0, memcmp(buf2.data(), kTestData, 16));
  EXPECT_NE(buf1.data(), buf2.data());
  EXPECT_EQ(buf1, buf2);
}

TEST(BufferTest, TestAssign) {
  Buffer buf1, buf2(kTestData, sizeof(kTestData), 256);
  EXPECT_NE(buf1, buf2);
  buf1 = buf2;
  EXPECT_EQ(buf1, buf2);
  EXPECT_NE(buf1.data(), buf2.data());
}

TEST(BufferTest, TestSetData) {
  Buffer buf(kTestData + 4, 7);
  buf.SetData(kTestData, 9);
  EXPECT_EQ(buf.size(), 9u);
  EXPECT_EQ(buf.capacity(), 9u);
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, 9));
}

TEST(BufferTest, TestAppendData) {
  Buffer buf(kTestData + 4, 3);
  buf.AppendData(kTestData + 10, 2);
  const int8_t exp[] = {0x4, 0x5, 0x6, 0xa, 0xb};
  EXPECT_EQ(buf, Buffer(exp));
}

TEST(BufferTest, TestSetSizeSmaller) {
  Buffer buf;
  buf.SetData(kTestData, 15);
  buf.SetSize(10);
  EXPECT_EQ(buf.size(), 10u);
  EXPECT_EQ(buf.capacity(), 15u);  // Hasn't shrunk.
  EXPECT_EQ(buf, Buffer(kTestData, 10));
}

TEST(BufferTest, TestSetSizeLarger) {
  Buffer buf;
  buf.SetData(kTestData, 15);
  EXPECT_EQ(buf.size(), 15u);
  EXPECT_EQ(buf.capacity(), 15u);
  buf.SetSize(20);
  EXPECT_EQ(buf.size(), 20u);
  EXPECT_EQ(buf.capacity(), 20u);  // Has grown.
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, 15));
}

TEST(BufferTest, TestEnsureCapacitySmaller) {
  Buffer buf(kTestData);
  const char* data = buf.data<char>();
  buf.EnsureCapacity(4);
  EXPECT_EQ(buf.capacity(), 16u);     // Hasn't shrunk.
  EXPECT_EQ(buf.data<char>(), data);  // No reallocation.
  EXPECT_EQ(buf, Buffer(kTestData));
}

TEST(BufferTest, TestEnsureCapacityLarger) {
  Buffer buf(kTestData, 5);
  buf.EnsureCapacity(10);
  const int8_t* data = buf.data<int8_t>();
  EXPECT_EQ(buf.capacity(), 10u);
  buf.AppendData(kTestData + 5, 5);
  EXPECT_EQ(buf.data<int8_t>(), data);  // No reallocation.
  EXPECT_EQ(buf, Buffer(kTestData, 10));
}

TEST(BufferTest, TestMoveConstruct) {
  Buffer buf1(kTestData, 3, 40);
  const uint8_t* data = buf1.data();
  Buffer buf2(buf1.DEPRECATED_Pass());
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 40u);
  EXPECT_EQ(buf2.data(), data);
  buf1.Clear();
  EXPECT_EQ(buf1.size(), 0u);
  EXPECT_EQ(buf1.capacity(), 0u);
  EXPECT_EQ(buf1.data(), nullptr);
}

TEST(BufferTest, TestMoveAssign) {
  Buffer buf1(kTestData, 3, 40);
  const uint8_t* data = buf1.data();
  Buffer buf2(kTestData);
  buf2 = buf1.DEPRECATED_Pass();
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 40u);
  EXPECT_EQ(buf2.data(), data);
  buf1.Clear();
  EXPECT_EQ(buf1.size(), 0u);
  EXPECT_EQ(buf1.capacity(), 0u);
  EXPECT_EQ(buf1.data(), nullptr);
}

TEST(BufferTest, TestSwap) {
  Buffer buf1(kTestData, 3);
  Buffer buf2(kTestData, 6, 40);
  uint8_t* data1 = buf1.data();
  uint8_t* data2 = buf2.data();
  using std::swap;
  swap(buf1, buf2);
  EXPECT_EQ(buf1.size(), 6u);
  EXPECT_EQ(buf1.capacity(), 40u);
  EXPECT_EQ(buf1.data(), data2);
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 3u);
  EXPECT_EQ(buf2.data(), data1);
}

}  // namespace rtc
