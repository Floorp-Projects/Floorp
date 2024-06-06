// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/toc.h"

#include <jxl/memory_manager.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "lib/jxl/base/common.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_toc.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

void Roundtrip(size_t num_entries, bool permute, Rng* rng) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  // Generate a random permutation.
  std::vector<coeff_order_t> permutation;
  std::vector<coeff_order_t> inv_permutation(num_entries);
  for (size_t i = 0; i < num_entries; i++) {
    inv_permutation[i] = i;
  }
  if (permute) {
    permutation.resize(num_entries);
    for (size_t i = 0; i < num_entries; i++) {
      permutation[i] = i;
    }
    rng->Shuffle(permutation.data(), permutation.size());
    for (size_t i = 0; i < num_entries; i++) {
      inv_permutation[permutation[i]] = i;
    }
  }

  // Generate num_entries groups of random (byte-aligned) length
  std::vector<std::unique_ptr<BitWriter>> group_codes;
  group_codes.reserve(num_entries);
  for (size_t i = 0; i < num_entries; ++i) {
    group_codes.emplace_back(jxl::make_unique<BitWriter>(memory_manager));
  }
  for (std::unique_ptr<BitWriter>& writer : group_codes) {
    const size_t max_bits = (*rng)() & 0xFFF;
    BitWriter::Allotment allotment(writer.get(), max_bits + kBitsPerByte);
    size_t i = 0;
    for (; i + BitWriter::kMaxBitsPerCall < max_bits;
         i += BitWriter::kMaxBitsPerCall) {
      writer->Write(BitWriter::kMaxBitsPerCall, 0);
    }
    for (; i < max_bits; i += 1) {
      writer->Write(/*n_bits=*/1, 0);
    }
    writer->ZeroPadToByte();
    AuxOut aux_out;
    allotment.ReclaimAndCharge(writer.get(), 0, &aux_out);
  }

  BitWriter writer{memory_manager};
  AuxOut aux_out;
  ASSERT_TRUE(WriteGroupOffsets(group_codes, permutation, &writer, &aux_out));

  BitReader reader(writer.GetSpan());
  std::vector<uint64_t> group_offsets;
  std::vector<uint32_t> group_sizes;
  uint64_t total_size;
  ASSERT_TRUE(ReadGroupOffsets(memory_manager, num_entries, &reader,
                               &group_offsets, &group_sizes, &total_size));
  ASSERT_EQ(num_entries, group_offsets.size());
  ASSERT_EQ(num_entries, group_sizes.size());
  EXPECT_TRUE(reader.Close());

  uint64_t prefix_sum = 0;
  for (size_t i = 0; i < num_entries; ++i) {
    EXPECT_EQ(prefix_sum, group_offsets[inv_permutation[i]]);

    EXPECT_EQ(0u, group_codes[i]->BitsWritten() % kBitsPerByte);
    prefix_sum += group_codes[i]->BitsWritten() / kBitsPerByte;

    if (i + 1 < num_entries) {
      EXPECT_EQ(
          group_offsets[inv_permutation[i]] + group_sizes[inv_permutation[i]],
          group_offsets[inv_permutation[i + 1]]);
    }
  }
  EXPECT_EQ(prefix_sum, total_size);
}

TEST(TocTest, Test) {
  Rng rng(0);
  for (size_t num_entries = 1; num_entries < 10; ++num_entries) {
    for (bool permute : std::vector<bool>{false, true}) {
      Roundtrip(num_entries, permute, &rng);
    }
  }
}

}  // namespace
}  // namespace jxl
