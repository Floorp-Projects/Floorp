/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/arraysize.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

namespace rtc {

TEST(ByteBufferTest, TestByteOrder) {
  uint16_t n16 = 1;
  uint32_t n32 = 1;
  uint64_t n64 = 1;

  EXPECT_EQ(n16, NetworkToHost16(HostToNetwork16(n16)));
  EXPECT_EQ(n32, NetworkToHost32(HostToNetwork32(n32)));
  EXPECT_EQ(n64, NetworkToHost64(HostToNetwork64(n64)));

  if (IsHostBigEndian()) {
    // The host is the network (big) endian.
    EXPECT_EQ(n16, HostToNetwork16(n16));
    EXPECT_EQ(n32, HostToNetwork32(n32));
    EXPECT_EQ(n64, HostToNetwork64(n64));

    // GetBE converts big endian to little endian here.
    EXPECT_EQ(n16 >> 8, GetBE16(&n16));
    EXPECT_EQ(n32 >> 24, GetBE32(&n32));
    EXPECT_EQ(n64 >> 56, GetBE64(&n64));
  } else {
    // The host is little endian.
    EXPECT_NE(n16, HostToNetwork16(n16));
    EXPECT_NE(n32, HostToNetwork32(n32));
    EXPECT_NE(n64, HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(GetBE16(&n16), HostToNetwork16(n16));
    EXPECT_EQ(GetBE32(&n32), HostToNetwork32(n32));
    EXPECT_EQ(GetBE64(&n64), HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(n16 << 8, GetBE16(&n16));
    EXPECT_EQ(n32 << 24, GetBE32(&n32));
    EXPECT_EQ(n64 << 56, GetBE64(&n64));
  }
}

TEST(ByteBufferTest, TestBufferLength) {
  ByteBuffer buffer;
  size_t size = 0;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt8(1);
  ++size;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt16(1);
  size += 2;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt24(1);
  size += 3;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt32(1);
  size += 4;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt64(1);
  size += 8;
  EXPECT_EQ(size, buffer.Length());

  EXPECT_TRUE(buffer.Consume(0));
  EXPECT_EQ(size, buffer.Length());

  EXPECT_TRUE(buffer.Consume(4));
  size -= 4;
  EXPECT_EQ(size, buffer.Length());
}

TEST(ByteBufferTest, TestGetSetReadPosition) {
  ByteBuffer buffer("ABCDEF", 6);
  EXPECT_EQ(6U, buffer.Length());
  ByteBuffer::ReadPosition pos(buffer.GetReadPosition());
  EXPECT_TRUE(buffer.SetReadPosition(pos));
  EXPECT_EQ(6U, buffer.Length());
  std::string read;
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("ABC", read);
  EXPECT_EQ(3U, buffer.Length());
  EXPECT_TRUE(buffer.SetReadPosition(pos));
  EXPECT_EQ(6U, buffer.Length());
  read.clear();
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("ABC", read);
  EXPECT_EQ(3U, buffer.Length());
  // For a resize by writing Capacity() number of bytes.
  size_t capacity = buffer.Capacity();
  buffer.ReserveWriteBuffer(buffer.Capacity());
  EXPECT_EQ(capacity + 3U, buffer.Length());
  EXPECT_FALSE(buffer.SetReadPosition(pos));
  read.clear();
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("DEF", read);
}

TEST(ByteBufferTest, TestReadWriteBuffer) {
  ByteBuffer::ByteOrder orders[2] = { ByteBuffer::ORDER_HOST,
                                      ByteBuffer::ORDER_NETWORK };
  for (size_t i = 0; i < arraysize(orders); i++) {
    ByteBuffer buffer(orders[i]);
    EXPECT_EQ(orders[i], buffer.Order());
    uint8_t ru8;
    EXPECT_FALSE(buffer.ReadUInt8(&ru8));

    // Write and read uint8_t.
    uint8_t wu8 = 1;
    buffer.WriteUInt8(wu8);
    EXPECT_TRUE(buffer.ReadUInt8(&ru8));
    EXPECT_EQ(wu8, ru8);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint16_t.
    uint16_t wu16 = (1 << 8) + 1;
    buffer.WriteUInt16(wu16);
    uint16_t ru16;
    EXPECT_TRUE(buffer.ReadUInt16(&ru16));
    EXPECT_EQ(wu16, ru16);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint24.
    uint32_t wu24 = (3 << 16) + (2 << 8) + 1;
    buffer.WriteUInt24(wu24);
    uint32_t ru24;
    EXPECT_TRUE(buffer.ReadUInt24(&ru24));
    EXPECT_EQ(wu24, ru24);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint32_t.
    uint32_t wu32 = (4 << 24) + (3 << 16) + (2 << 8) + 1;
    buffer.WriteUInt32(wu32);
    uint32_t ru32;
    EXPECT_TRUE(buffer.ReadUInt32(&ru32));
    EXPECT_EQ(wu32, ru32);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint64_t.
    uint32_t another32 = (8 << 24) + (7 << 16) + (6 << 8) + 5;
    uint64_t wu64 = (static_cast<uint64_t>(another32) << 32) + wu32;
    buffer.WriteUInt64(wu64);
    uint64_t ru64;
    EXPECT_TRUE(buffer.ReadUInt64(&ru64));
    EXPECT_EQ(wu64, ru64);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read string.
    std::string write_string("hello");
    buffer.WriteString(write_string);
    std::string read_string;
    EXPECT_TRUE(buffer.ReadString(&read_string, write_string.size()));
    EXPECT_EQ(write_string, read_string);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read bytes
    char write_bytes[] = "foo";
    buffer.WriteBytes(write_bytes, 3);
    char read_bytes[3];
    EXPECT_TRUE(buffer.ReadBytes(read_bytes, 3));
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(write_bytes[i], read_bytes[i]);
    }
    EXPECT_EQ(0U, buffer.Length());

    // Write and read reserved buffer space
    char* write_dst = buffer.ReserveWriteBuffer(3);
    memcpy(write_dst, write_bytes, 3);
    memset(read_bytes, 0, 3);
    EXPECT_TRUE(buffer.ReadBytes(read_bytes, 3));
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(write_bytes[i], read_bytes[i]);
    }
    EXPECT_EQ(0U, buffer.Length());

    // Write and read in order.
    buffer.WriteUInt8(wu8);
    buffer.WriteUInt16(wu16);
    buffer.WriteUInt24(wu24);
    buffer.WriteUInt32(wu32);
    buffer.WriteUInt64(wu64);
    EXPECT_TRUE(buffer.ReadUInt8(&ru8));
    EXPECT_EQ(wu8, ru8);
    EXPECT_TRUE(buffer.ReadUInt16(&ru16));
    EXPECT_EQ(wu16, ru16);
    EXPECT_TRUE(buffer.ReadUInt24(&ru24));
    EXPECT_EQ(wu24, ru24);
    EXPECT_TRUE(buffer.ReadUInt32(&ru32));
    EXPECT_EQ(wu32, ru32);
    EXPECT_TRUE(buffer.ReadUInt64(&ru64));
    EXPECT_EQ(wu64, ru64);
    EXPECT_EQ(0U, buffer.Length());
  }
}

}  // namespace rtc
