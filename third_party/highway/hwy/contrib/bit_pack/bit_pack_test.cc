// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

#include <stdio.h>

#include <vector>

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"
#include "hwy/nanobenchmark.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/bit_pack/bit_pack_test.cc"  // NOLINT
#include "hwy/foreach_target.h"  // IWYU pragma: keep

#include "hwy/contrib/bit_pack/bit_pack-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

#ifndef HWY_BIT_PACK_BENCHMARK
#define HWY_BIT_PACK_BENCHMARK 0
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
// Used to prevent running benchmark (slow) for partial vectors and targets
// except the best available. Global, not per-target, hence must be outside
// HWY_NAMESPACE. Declare first because HWY_ONCE is only true after some code
// has been re-included.
extern size_t last_bits;
extern uint64_t best_target;
#if HWY_ONCE
size_t last_bits = 0;
uint64_t best_target = ~0ull;
#endif
namespace HWY_NAMESPACE {

template <size_t kBits, typename T>
T Random(RandomState& rng) {
  return static_cast<T>(Random32(&rng) & kBits);
}

template <typename T>
class Checker {
 public:
  explicit Checker(size_t num) { raw_.reserve(num); }
  void NotifyRaw(T raw) { raw_.push_back(raw); }

  void NotifyRawOutput(size_t bits, T raw) {
    if (raw_[num_verified_] != raw) {
      HWY_ABORT("%zu bits: pos %zu of %zu, expected %.0f actual %.0f\n", bits,
                num_verified_, raw_.size(),
                static_cast<double>(raw_[num_verified_]),
                static_cast<double>(raw));
    }
    ++num_verified_;
  }

 private:
  std::vector<T> raw_;
  size_t num_verified_ = 0;
};

template <class PackT>
struct TestPack {
  template <typename T, class D>
  void operator()(T /* t */, D d) {
    const size_t N = Lanes(d);
    RandomState rng(N * 129);
    const size_t num = N * PackT::kRawVectors;
    const size_t packed_size = N * PackT::kPackedVectors;
    Checker<T> checker(num);
    AlignedFreeUniquePtr<T[]> raw = hwy::AllocateAligned<T>(num);
    AlignedFreeUniquePtr<T[]> raw2 = hwy::AllocateAligned<T>(num);
    AlignedFreeUniquePtr<T[]> packed = hwy::AllocateAligned<T>(packed_size);

    for (size_t i = 0; i < num; ++i) {
      raw[i] = Random<PackT::kBits, T>(rng);
      checker.NotifyRaw(raw[i]);
    }

    best_target = HWY_MIN(best_target, HWY_TARGET);
    const bool run_bench = HWY_BIT_PACK_BENCHMARK &&
                           (PackT::kBits != last_bits) &&
                           (HWY_TARGET == best_target);
    last_bits = PackT::kBits;

    if (run_bench) {
      const size_t kNumInputs = 1;
      const size_t num_items = num * size_t(Unpredictable1());
      const FuncInput inputs[kNumInputs] = {num_items};
      Result results[kNumInputs];

      Params p;
      p.verbose = false;
      p.max_evals = 7;
      p.target_rel_mad = 0.002;
      const size_t num_results = MeasureClosure(
          [&](FuncInput) HWY_ATTR {
            PackT().Pack(d, raw.get(), packed.get());
            PackT().Unpack(d, packed.get(), raw2.get());
            return raw2[Random32(&rng) % num];
          },
          inputs, kNumInputs, results, p);
      if (num_results != kNumInputs) {
        fprintf(stderr, "MeasureClosure failed.\n");
        return;
      }
      // Print cycles per element
      for (size_t i = 0; i < num_results; ++i) {
        const double cycles_per_item =
            results[i].ticks / static_cast<double>(results[i].input);
        const double mad = results[i].variability * cycles_per_item;
        printf("Bits:%2d elements:%3d cyc/elt:%6.3f (+/- %5.3f)\n",
               static_cast<int>(PackT::kBits),
               static_cast<int>(results[i].input), cycles_per_item, mad);
      }
    } else {
      PackT().Pack(d, raw.get(), packed.get());
      PackT().Unpack(d, packed.get(), raw2.get());
    }

    for (size_t i = 0; i < num; ++i) {
      checker.NotifyRawOutput(PackT::kBits, raw2[i]);
    }
  }
};

void TestAllPack8() {
  ForShrinkableVectors<TestPack<detail::Pack8<1>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<2>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<3>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<4>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<5>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<6>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<7>>>()(uint8_t());
  ForShrinkableVectors<TestPack<detail::Pack8<8>>>()(uint8_t());
}

void TestAllPack16() {
  ForShrinkableVectors<TestPack<detail::Pack16<1>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<2>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<3>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<4>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<5>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<6>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<7>>>()(uint16_t());
  ForShrinkableVectors<TestPack<detail::Pack16<8>>>()(uint16_t());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(BitPackTest);
HWY_EXPORT_AND_TEST_P(BitPackTest, TestAllPack8);
HWY_EXPORT_AND_TEST_P(BitPackTest, TestAllPack16);
}  // namespace hwy

#endif
