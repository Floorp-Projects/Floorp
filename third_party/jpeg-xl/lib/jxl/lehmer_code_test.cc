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

#include "lib/jxl/lehmer_code.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/thread_pool_internal.h"

namespace jxl {
namespace {

template <typename PermutationT>
struct WorkingSet {
  explicit WorkingSet(size_t max_n)
      : padded_n(1ull << CeilLog2Nonzero(max_n + 1)),
        permutation(max_n),
        temp(padded_n),
        lehmer(max_n),
        decoded(max_n) {}

  size_t padded_n;
  std::vector<PermutationT> permutation;
  std::vector<uint32_t> temp;
  std::vector<LehmerT> lehmer;
  std::vector<PermutationT> decoded;
};

template <typename PermutationT>
void Roundtrip(size_t n, WorkingSet<PermutationT>* ws) {
  JXL_ASSERT(n != 0);
  const size_t padded_n = 1ull << CeilLog2Nonzero(n);

  std::mt19937 rng(n * 65537 + 13);

  // Ensure indices fit into PermutationT
  EXPECT_LE(n, 1ULL << (sizeof(PermutationT) * 8));

  std::iota(ws->permutation.begin(), ws->permutation.begin() + n, 0);

  // For various random permutations:
  for (size_t rep = 0; rep < 100; ++rep) {
    std::shuffle(ws->permutation.begin(), ws->permutation.begin() + n, rng);

    // Must decode to the same permutation
    ComputeLehmerCode(ws->permutation.data(), ws->temp.data(), n,
                      ws->lehmer.data());
    memset(ws->temp.data(), 0, padded_n * 4);
    DecodeLehmerCode(ws->lehmer.data(), ws->temp.data(), n, ws->decoded.data());

    for (size_t i = 0; i < n; ++i) {
      EXPECT_EQ(ws->permutation[i], ws->decoded[i]);
    }
  }
}

// Preallocates arrays and tests n = [begin, end).
template <typename PermutationT>
void RoundtripSizeRange(ThreadPool* pool, uint32_t begin, uint32_t end) {
  ASSERT_NE(0, begin);  // n = 0 not allowed.
  std::vector<WorkingSet<PermutationT>> working_sets;

  RunOnPool(
      pool, begin, end,
      [&working_sets, end](size_t num_threads) {
        for (size_t i = 0; i < num_threads; i++) {
          working_sets.emplace_back(end - 1);
        }
        return true;
      },
      [&working_sets](int n, int thread) {
        Roundtrip(n, &working_sets[thread]);
      },
      "lehmer test");
}

TEST(LehmerCodeTest, TestRoundtrips) {
  ThreadPoolInternal pool(8);

  RoundtripSizeRange<uint16_t>(&pool, 1, 1026);

  // Ensures PermutationT can fit > 16 bit values.
  RoundtripSizeRange<uint32_t>(&pool, 65536, 65540);
}

}  // namespace
}  // namespace jxl
