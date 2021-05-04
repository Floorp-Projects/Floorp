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

#include "lib/jxl/coeff_order.h"

#include <stdio.h>

#include <algorithm>
#include <numeric>  // iota
#include <random>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_coeff_order.h"

namespace jxl {
namespace {

void RoundtripPermutation(coeff_order_t* perm, coeff_order_t* out, size_t len,
                          size_t* size) {
  BitWriter writer;
  EncodePermutation(perm, 0, len, &writer, 0, nullptr);
  writer.ZeroPadToByte();
  Status status = true;
  {
    BitReader reader(writer.GetSpan());
    BitReaderScopedCloser closer(&reader, &status);
    ASSERT_TRUE(DecodePermutation(0, len, out, &reader));
  }
  ASSERT_TRUE(status);
  *size = writer.GetSpan().size();
}

enum Permutation { kIdentity, kFewSwaps, kFewSlides, kRandom };

constexpr size_t kNumReps = 128;
constexpr size_t kSwaps = 32;

void TestPermutation(Permutation kind, size_t len) {
  std::vector<coeff_order_t> perm(len);
  std::iota(perm.begin(), perm.end(), 0);
  std::mt19937 rng;
  if (kind == kFewSwaps) {
    std::uniform_int_distribution<size_t> dist(0, len - 1);
    for (size_t i = 0; i < kSwaps; i++) {
      size_t a = dist(rng);
      size_t b = dist(rng);
      std::swap(perm[a], perm[b]);
    }
  }
  if (kind == kFewSlides) {
    std::uniform_int_distribution<size_t> dist(0, len - 1);
    for (size_t i = 0; i < kSwaps; i++) {
      size_t a = dist(rng);
      size_t b = dist(rng);
      size_t from = std::min(a, b);
      size_t to = std::max(a, b);
      size_t start = perm[from];
      for (size_t j = from; j < to; j++) {
        perm[j] = perm[j + 1];
      }
      perm[to] = start;
    }
  }
  if (kind == kRandom) {
    std::shuffle(perm.begin(), perm.end(), rng);
  }
  std::vector<coeff_order_t> out(len);
  size_t size = 0;
  for (size_t i = 0; i < kNumReps; i++) {
    RoundtripPermutation(perm.data(), out.data(), len, &size);
    for (size_t idx = 0; idx < len; idx++) {
      EXPECT_EQ(perm[idx], out[idx]);
    }
  }
  printf("Encoded size: %zu\n", size);
}

TEST(CoeffOrderTest, IdentitySmall) { TestPermutation(kIdentity, 256); }
TEST(CoeffOrderTest, FewSlidesSmall) { TestPermutation(kFewSlides, 256); }
TEST(CoeffOrderTest, FewSwapsSmall) { TestPermutation(kFewSwaps, 256); }
TEST(CoeffOrderTest, RandomSmall) { TestPermutation(kRandom, 256); }

TEST(CoeffOrderTest, IdentityMedium) { TestPermutation(kIdentity, 1 << 12); }
TEST(CoeffOrderTest, FewSlidesMedium) { TestPermutation(kFewSlides, 1 << 12); }
TEST(CoeffOrderTest, FewSwapsMedium) { TestPermutation(kFewSwaps, 1 << 12); }
TEST(CoeffOrderTest, RandomMedium) { TestPermutation(kRandom, 1 << 12); }

TEST(CoeffOrderTest, IdentityBig) { TestPermutation(kIdentity, 1 << 16); }
TEST(CoeffOrderTest, FewSlidesBig) { TestPermutation(kFewSlides, 1 << 16); }
TEST(CoeffOrderTest, FewSwapsBig) { TestPermutation(kFewSwaps, 1 << 16); }
TEST(CoeffOrderTest, RandomBig) { TestPermutation(kRandom, 1 << 16); }

}  // namespace
}  // namespace jxl
