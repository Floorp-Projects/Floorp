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

#include "lib/jxl/toc.h"

#include <random>

#include "gtest/gtest.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_toc.h"

namespace jxl {
namespace {

void Roundtrip(size_t num_entries, bool permute, std::mt19937* rng) {
  // Generate a random permutation.
  std::vector<coeff_order_t> permutation(num_entries);
  std::vector<coeff_order_t> inv_permutation(num_entries);
  for (size_t i = 0; i < num_entries; i++) {
    permutation[i] = i;
    inv_permutation[i] = i;
  }
  if (permute) {
    std::shuffle(permutation.begin(), permutation.end(), *rng);
    for (size_t i = 0; i < num_entries; i++) {
      inv_permutation[permutation[i]] = i;
    }
  }

  // Generate num_entries groups of random (byte-aligned) length
  std::vector<BitWriter> group_codes(num_entries);
  for (BitWriter& writer : group_codes) {
    const size_t max_bits = (*rng)() & 0xFFF;
    BitWriter::Allotment allotment(&writer, max_bits + kBitsPerByte);
    size_t i = 0;
    for (; i + BitWriter::kMaxBitsPerCall < max_bits;
         i += BitWriter::kMaxBitsPerCall) {
      writer.Write(BitWriter::kMaxBitsPerCall, 0);
    }
    for (; i < max_bits; i += 1) {
      writer.Write(/*n_bits=*/1, 0);
    }
    writer.ZeroPadToByte();
    AuxOut aux_out;
    ReclaimAndCharge(&writer, &allotment, 0, &aux_out);
  }

  BitWriter writer;
  AuxOut aux_out;
  ASSERT_TRUE(WriteGroupOffsets(group_codes, permute ? &permutation : nullptr,
                                &writer, &aux_out));

  BitReader reader(writer.GetSpan());
  std::vector<uint64_t> group_offsets;
  std::vector<uint32_t> group_sizes;
  uint64_t total_size;
  ASSERT_TRUE(ReadGroupOffsets(num_entries, &reader, &group_offsets,
                               &group_sizes, &total_size));
  ASSERT_EQ(num_entries, group_offsets.size());
  ASSERT_EQ(num_entries, group_sizes.size());
  EXPECT_TRUE(reader.Close());

  uint64_t prefix_sum = 0;
  for (size_t i = 0; i < num_entries; ++i) {
    EXPECT_EQ(prefix_sum, group_offsets[inv_permutation[i]]);

    EXPECT_EQ(0, group_codes[i].BitsWritten() % kBitsPerByte);
    prefix_sum += group_codes[i].BitsWritten() / kBitsPerByte;

    if (i + 1 < num_entries) {
      EXPECT_EQ(
          group_offsets[inv_permutation[i]] + group_sizes[inv_permutation[i]],
          group_offsets[inv_permutation[i + 1]]);
    }
  }
  EXPECT_EQ(prefix_sum, total_size);
}

TEST(TocTest, Test) {
  std::mt19937 rng(12345);
  for (size_t num_entries = 0; num_entries < 10; ++num_entries) {
    for (bool permute : std::vector<bool>{false, true}) {
      Roundtrip(num_entries, permute, &rng);
    }
  }
}

}  // namespace
}  // namespace jxl
