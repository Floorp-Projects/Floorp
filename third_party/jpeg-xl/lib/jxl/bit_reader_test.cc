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

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {
namespace {

TEST(BitReaderTest, ExtendsWithZeroes) {
  for (size_t size = 4; size < 32; ++size) {
    std::vector<uint8_t> data(size, 0xff);

    for (size_t n_bytes = 0; n_bytes < size; n_bytes++) {
      BitReader br(Span<const uint8_t>(data.data(), n_bytes));
      // Read all the bits
      for (size_t i = 0; i < n_bytes * kBitsPerByte; i++) {
        ASSERT_EQ(br.ReadBits(1), 1) << "n_bytes=" << n_bytes << " i=" << i;
      }

      // PEEK more than the declared size - all will be zero. Cannot consume.
      for (size_t i = 0; i < BitReader::kMaxBitsPerCall; i++) {
        ASSERT_EQ(br.PeekBits(i), 0)
            << "size=" << size << "n_bytes=" << n_bytes << " i=" << i;
      }

      EXPECT_TRUE(br.Close());
    }
  }
}

struct Symbol {
  uint32_t num_bits;
  uint32_t value;
};

// Reading from output gives the same values.
TEST(BitReaderTest, TestRoundTrip) {
  ThreadPoolInternal pool(8);
  pool.Run(0, 1000, ThreadPool::SkipInit(),
           [](const int task, const int /* thread */) {
             constexpr size_t kMaxBits = 8000;
             BitWriter writer;
             BitWriter::Allotment allotment(&writer, kMaxBits);

             std::vector<Symbol> symbols;
             symbols.reserve(1000);

             std::mt19937 rng(55537 + 129 * task);
             std::uniform_int_distribution<> dist(1, 32);  // closed interval

             for (;;) {
               const uint32_t num_bits = dist(rng);
               if (writer.BitsWritten() + num_bits > kMaxBits) break;
               const uint32_t value = rng() >> (32 - num_bits);
               symbols.push_back({num_bits, value});
               writer.Write(num_bits, value);
             }

             writer.ZeroPadToByte();
             ReclaimAndCharge(&writer, &allotment, 0, nullptr);
             BitReader reader(writer.GetSpan());
             for (const Symbol& s : symbols) {
               EXPECT_EQ(s.value, reader.ReadBits(s.num_bits));
             }
             EXPECT_TRUE(reader.Close());
           });
}

// SkipBits is the same as reading that many bits.
TEST(BitReaderTest, TestSkip) {
  ThreadPoolInternal pool(8);
  pool.Run(
      0, 96, ThreadPool::SkipInit(),
      [](const int task, const int /* thread */) {
        constexpr size_t kSize = 100;

        for (size_t skip = 0; skip < 128; ++skip) {
          BitWriter writer;
          BitWriter::Allotment allotment(&writer, kSize * kBitsPerByte);
          // Start with "task" 1-bits.
          for (int i = 0; i < task; ++i) {
            writer.Write(1, 1);
          }

          // Write 0-bits that we will skip over
          for (size_t i = 0; i < skip; ++i) {
            writer.Write(1, 0);
          }

          // Write terminator bits '101'
          writer.Write(3, 5);
          EXPECT_EQ(task + skip + 3, writer.BitsWritten());
          writer.ZeroPadToByte();
          AuxOut aux_out;
          ReclaimAndCharge(&writer, &allotment, 0, &aux_out);
          EXPECT_LT(aux_out.layers[0].total_bits, kSize * 8);

          BitReader reader1(writer.GetSpan());
          BitReader reader2(writer.GetSpan());
          // Verify initial 1-bits
          for (int i = 0; i < task; ++i) {
            EXPECT_EQ(1, reader1.ReadBits(1));
            EXPECT_EQ(1, reader2.ReadBits(1));
          }

          // SkipBits or manually read "skip" bits
          reader1.SkipBits(skip);
          for (size_t i = 0; i < skip; ++i) {
            EXPECT_EQ(0, reader2.ReadBits(1)) << " skip=" << skip << " i=" << i;
          }
          EXPECT_EQ(reader1.TotalBitsConsumed(), reader2.TotalBitsConsumed());

          // Ensure both readers see the terminator bits.
          EXPECT_EQ(5, reader1.ReadBits(3));
          EXPECT_EQ(5, reader2.ReadBits(3));

          EXPECT_TRUE(reader1.Close());
          EXPECT_TRUE(reader2.Close());
        }
      });
}

// Verifies byte order and different groupings of bits.
TEST(BitReaderTest, TestOrder) {
  constexpr size_t kMaxBits = 16;

  // u(1) - bits written into LSBs of first byte
  {
    BitWriter writer;
    BitWriter::Allotment allotment(&writer, kMaxBits);
    for (size_t i = 0; i < 5; ++i) {
      writer.Write(1, 1);
    }
    for (size_t i = 0; i < 5; ++i) {
      writer.Write(1, 0);
    }
    for (size_t i = 0; i < 6; ++i) {
      writer.Write(1, 1);
    }

    writer.ZeroPadToByte();
    ReclaimAndCharge(&writer, &allotment, 0, nullptr);
    BitReader reader(writer.GetSpan());
    EXPECT_EQ(0x1F, reader.ReadFixedBits<8>());
    EXPECT_EQ(0xFC, reader.ReadFixedBits<8>());
    EXPECT_TRUE(reader.Close());
  }

  // u(8) - get bytes in the same order
  {
    BitWriter writer;
    BitWriter::Allotment allotment(&writer, kMaxBits);
    writer.Write(8, 0xF8);
    writer.Write(8, 0x3F);

    writer.ZeroPadToByte();
    ReclaimAndCharge(&writer, &allotment, 0, nullptr);
    BitReader reader(writer.GetSpan());
    EXPECT_EQ(0xF8, reader.ReadFixedBits<8>());
    EXPECT_EQ(0x3F, reader.ReadFixedBits<8>());
    EXPECT_TRUE(reader.Close());
  }

  // u(16) - little-endian bytes
  {
    BitWriter writer;
    BitWriter::Allotment allotment(&writer, kMaxBits);
    writer.Write(16, 0xF83F);

    writer.ZeroPadToByte();
    ReclaimAndCharge(&writer, &allotment, 0, nullptr);
    BitReader reader(writer.GetSpan());
    EXPECT_EQ(0x3F, reader.ReadFixedBits<8>());
    EXPECT_EQ(0xF8, reader.ReadFixedBits<8>());
    EXPECT_TRUE(reader.Close());
  }

  // Non-byte-aligned, mixed sizes
  {
    BitWriter writer;
    BitWriter::Allotment allotment(&writer, kMaxBits);
    writer.Write(1, 1);
    writer.Write(3, 6);
    writer.Write(8, 0xDB);
    writer.Write(4, 8);

    writer.ZeroPadToByte();
    ReclaimAndCharge(&writer, &allotment, 0, nullptr);
    BitReader reader(writer.GetSpan());
    EXPECT_EQ(0xBD, reader.ReadFixedBits<8>());
    EXPECT_EQ(0x8D, reader.ReadFixedBits<8>());
    EXPECT_TRUE(reader.Close());
  }
}

TEST(BitReaderTest, TotalCountersTest) {
  uint8_t buf[8] = {1, 2, 3, 4};
  BitReader reader(Span<const uint8_t>(buf, sizeof(buf)));

  EXPECT_EQ(sizeof(buf), reader.TotalBytes());
  EXPECT_EQ(0, reader.TotalBitsConsumed());
  reader.ReadFixedBits<1>();
  EXPECT_EQ(1, reader.TotalBitsConsumed());

  reader.ReadFixedBits<10>();
  EXPECT_EQ(11, reader.TotalBitsConsumed());

  reader.ReadFixedBits<4>();
  EXPECT_EQ(15, reader.TotalBitsConsumed());

  reader.ReadFixedBits<1>();
  EXPECT_EQ(16, reader.TotalBitsConsumed());

  reader.ReadFixedBits<16>();
  EXPECT_EQ(32, reader.TotalBitsConsumed());

  EXPECT_TRUE(reader.Close());
}

TEST(BitReaderTest, MoveTest) {
  uint8_t buf[8] = {1, 2, 3, 4};
  BitReader reader2;
  {
    BitReader reader1(Span<const uint8_t>(buf, sizeof(buf)));

    EXPECT_EQ(0, reader1.TotalBitsConsumed());
    reader1.ReadFixedBits<16>();
    EXPECT_EQ(16, reader1.TotalBitsConsumed());

    reader2 = std::move(reader1);
    // From this point reader1 is invalid, but can continue to access reader2
    // and we don't need to call Close() on reader1.
  }

  EXPECT_EQ(16, reader2.TotalBitsConsumed());
  EXPECT_EQ(3U, reader2.ReadFixedBits<8>());
  EXPECT_EQ(24, reader2.TotalBitsConsumed());

  EXPECT_TRUE(reader2.Close());
}

}  // namespace
}  // namespace jxl
