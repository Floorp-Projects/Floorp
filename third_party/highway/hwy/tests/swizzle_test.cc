// Copyright 2019 Google LLC
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

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <array>  // IWYU pragma: keep

#include "hwy/base.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/swizzle_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// For regenerating tables used in the implementation
#define HWY_PRINT_TABLES 0

struct TestGetLane {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(1));
    HWY_ASSERT_EQ(T(1), GetLane(v));
  }
};

HWY_NOINLINE void TestAllGetLane() {
  ForAllTypes(ForPartialVectors<TestGetLane>());
}

struct TestDupEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((static_cast<int>(i) & ~1) + 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), DupEven(Iota(d, 1)));
  }
};

HWY_NOINLINE void TestAllDupEven() {
  ForUIF3264(ForShrinkableVectors<TestDupEven>());
}

struct TestDupOdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((static_cast<int>(i) & ~1) + 2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), DupOdd(Iota(d, 1)));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllDupOdd() {
  ForUIF3264(ForShrinkableVectors<TestDupOdd>());
}

struct TestOddEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto even = Iota(d, 1);
    const auto odd = Iota(d, static_cast<T>(1 + N));
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>(1 + i + ((i & 1) ? N : 0));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), OddEven(odd, even));
  }
};

HWY_NOINLINE void TestAllOddEven() {
  ForAllTypes(ForShrinkableVectors<TestOddEven>());
}

struct TestOddEvenBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto even = Iota(d, 1);
    const auto odd = Iota(d, static_cast<T>(1 + N));
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / (16 / sizeof(T));
      expected[i] = static_cast<T>(1 + i + ((idx_block & 1) ? N : 0));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), OddEvenBlocks(odd, even));
  }
};

HWY_NOINLINE void TestAllOddEvenBlocks() {
  ForAllTypes(ForGEVectors<128, TestOddEvenBlocks>());
}

struct TestSwapAdjacentBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    constexpr size_t kLanesPerBlock = 16 / sizeof(T);
    if (N < 2 * kLanesPerBlock) return;
    const auto vi = Iota(d, 1);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / kLanesPerBlock;
      const size_t base = (idx_block ^ 1) * kLanesPerBlock;
      const size_t mod = i % kLanesPerBlock;
      expected[i] = static_cast<T>(1 + base + mod);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), SwapAdjacentBlocks(vi));
  }
};

HWY_NOINLINE void TestAllSwapAdjacentBlocks() {
  ForAllTypes(ForGEVectors<128, TestSwapAdjacentBlocks>());
}

struct TestTableLookupLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    const Rebind<TI, D> di;
    auto idx = AllocateAligned<TI>(N);
    memset(idx.get(), 0, N * sizeof(TI));
    auto expected = AllocateAligned<T>(N);
    const auto v = Iota(d, 1);

    if (N <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < N; ++i0) {
        idx[0] = static_cast<TI>(i0);

        for (size_t i1 = 0; i1 < N; ++i1) {
          if (N >= 2) idx[1] = static_cast<TI>(i1);
          for (size_t i2 = 0; i2 < N; ++i2) {
            if (N >= 4) idx[2] = static_cast<TI>(i2);
            for (size_t i3 = 0; i3 < N; ++i3) {
              if (N >= 4) idx[3] = static_cast<TI>(i3);

              for (size_t i = 0; i < N; ++i) {
                expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
              const auto actual1 = TableLookupLanes(v, opaque1);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

              const auto opaque2 = SetTableIndices(d, idx.get());
              const auto actual2 = TableLookupLanes(v, opaque2);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
            }
          }
        }
      }
    } else {
      // Too many permutations to test exhaustively; choose one with repeated
      // and cross-block indices and ensure indices do not exceed #lanes.
      // For larger vectors, upper lanes will be zero.
      HWY_ALIGN TI idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                     15, 14, 14, 15, 4, 9, 8, 5};
      for (size_t i = 0; i < N; ++i) {
        idx[i] = (i < 16) ? idx_source[i] : 0;
        // Avoid undefined results / asan error for scalar by capping indices.
        if (idx[i] >= static_cast<TI>(N)) {
          idx[i] = static_cast<TI>(N - 1);
        }
        expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
      }

      const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
      const auto actual1 = TableLookupLanes(v, opaque1);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

      const auto opaque2 = SetTableIndices(d, idx.get());
      const auto actual2 = TableLookupLanes(v, opaque2);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
    }
#else
    const TI index = 0;
    const auto v = Set(d, 1);
    const auto opaque1 = SetTableIndices(d, &index);
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque1));
    const auto opaque2 = IndicesFromVec(d, Zero(d));
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque2));
#endif
  }
};

HWY_NOINLINE void TestAllTableLookupLanes() {
  ForUIF3264(ForPartialVectors<TestTableLookupLanes>());
}

struct TestReverse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[N - 1 - i];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse(d, v));
  }
};

struct TestReverse2 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 1];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse2(d, v));
  }
};

struct TestReverse4 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 3];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse4(d, v));
  }
};

struct TestReverse8 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 7];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse8(d, v));
  }
};

HWY_NOINLINE void TestAllReverse() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF163264(ForPartialVectors<TestReverse>());
}

HWY_NOINLINE void TestAllReverse2() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<128, TestReverse2>());
  ForUIF32(ForGEVectors<64, TestReverse2>());
  ForUIF16(ForGEVectors<32, TestReverse2>());
}

HWY_NOINLINE void TestAllReverse4() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<256, TestReverse4>());
  ForUIF32(ForGEVectors<128, TestReverse4>());
  ForUIF16(ForGEVectors<64, TestReverse4>());
}

HWY_NOINLINE void TestAllReverse8() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<512, TestReverse8>());
  ForUIF32(ForGEVectors<256, TestReverse8>());
  ForUIF16(ForGEVectors<128, TestReverse8>());
}

struct TestReverseBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);
    const size_t num_blocks = N / kLanesPerBlock;
    HWY_ASSERT(num_blocks != 0);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / kLanesPerBlock;
      const size_t base = (num_blocks - 1 - idx_block) * kLanesPerBlock;
      expected[i] = copy[base + (i % kLanesPerBlock)];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ReverseBlocks(d, v));
  }
};

HWY_NOINLINE void TestAllReverseBlocks() {
  ForAllTypes(ForGEVectors<128, TestReverseBlocks>());
}

class TestCompress {
  template <class D, class DI, typename T = TFromD<D>, typename TI = TFromD<DI>>
  void CheckStored(D d, DI di, size_t expected_pos, size_t actual_pos,
                   const AlignedFreeUniquePtr<T[]>& in,
                   const AlignedFreeUniquePtr<TI[]>& mask_lanes,
                   const AlignedFreeUniquePtr<T[]>& expected, const T* actual_u,
                   int line) {
    if (expected_pos != actual_pos) {
      hwy::Abort(
          __FILE__, line,
          "Size mismatch for %s: expected %" PRIu64 ", actual %" PRIu64 "\n",
          TypeName(T(), Lanes(d)).c_str(), static_cast<uint64_t>(expected_pos),
          static_cast<uint64_t>(actual_pos));
    }
    // Upper lanes are undefined. Modified from AssertVecEqual.
    for (size_t i = 0; i < expected_pos; ++i) {
      if (!IsEqual(expected[i], actual_u[i])) {
        fprintf(stderr,
                "Mismatch at i=%" PRIu64 " of %" PRIu64 ", line %d:\n\n",
                static_cast<uint64_t>(i), static_cast<uint64_t>(expected_pos),
                line);
        const size_t N = Lanes(d);
        Print(di, "mask", Load(di, mask_lanes.get()), 0, N);
        Print(d, "in", Load(d, in.get()), 0, N);
        Print(d, "expect", Load(d, expected.get()), 0, N);
        Print(d, "actual", Load(d, actual_u), 0, N);
        HWY_ASSERT(false);
      }
    }
  }

 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);

    const T zero{0};

    for (int frac : {0, 2, 3}) {
      // For CompressStore
      const size_t misalign = static_cast<size_t>(frac) * N / 4;

      auto in_lanes = AllocateAligned<T>(N);
      auto mask_lanes = AllocateAligned<TI>(N);
      auto expected = AllocateAligned<T>(N);
      auto actual_a = AllocateAligned<T>(misalign + N);
      T* actual_u = actual_a.get() + misalign;

      const size_t bits_size = RoundUpTo((N + 7) / 8, 8);
      auto bits = AllocateAligned<uint8_t>(bits_size);
      memset(bits.get(), 0, bits_size);  // for MSAN

      // Each lane should have a chance of having mask=true.
      for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
        size_t expected_pos = 0;
        for (size_t i = 0; i < N; ++i) {
          const uint64_t bits = Random32(&rng);
          in_lanes[i] = T();  // cannot initialize float16_t directly.
          CopyBytes<sizeof(T)>(&bits, &in_lanes[i]);
          mask_lanes[i] = (Random32(&rng) & 1024) ? TI(1) : TI(0);
          if (mask_lanes[i] > 0) {
            expected[expected_pos++] = in_lanes[i];
          }
        }

        const auto in = Load(d, in_lanes.get());
        const auto mask =
            RebindMask(d, Gt(Load(di, mask_lanes.get()), Zero(di)));
        StoreMaskBits(d, mask, bits.get());

        // Compress
        memset(actual_u, 0, N * sizeof(T));
        StoreU(Compress(in, mask), d, actual_u);
        CheckStored(d, di, expected_pos, expected_pos, in_lanes, mask_lanes,
                    expected, actual_u, __LINE__);

        // CompressStore
        memset(actual_u, 0, N * sizeof(T));
        const size_t size1 = CompressStore(in, mask, d, actual_u);
        CheckStored(d, di, expected_pos, size1, in_lanes, mask_lanes, expected,
                    actual_u, __LINE__);

        // CompressBlendedStore
        memset(actual_u, 0, N * sizeof(T));
        const size_t size2 = CompressBlendedStore(in, mask, d, actual_u);
        CheckStored(d, di, expected_pos, size2, in_lanes, mask_lanes, expected,
                    actual_u, __LINE__);
        // Subsequent lanes are untouched.
        for (size_t i = size2; i < N; ++i) {
          HWY_ASSERT_EQ(zero, actual_u[i]);
        }

        // TODO(janwas): remove once implemented (cast or vse1)
#if HWY_TARGET != HWY_RVV
        // CompressBits
        memset(actual_u, 0, N * sizeof(T));
        StoreU(CompressBits(in, bits.get()), d, actual_u);
        CheckStored(d, di, expected_pos, expected_pos, in_lanes, mask_lanes,
                    expected, actual_u, __LINE__);

        // CompressBitsStore
        memset(actual_u, 0, N * sizeof(T));
        const size_t size3 = CompressBitsStore(in, bits.get(), d, actual_u);
        CheckStored(d, di, expected_pos, size3, in_lanes, mask_lanes, expected,
                    actual_u, __LINE__);
#endif
      }  // rep
    }    // frac
  }      // operator()
};

#if HWY_PRINT_TABLES
namespace detail {  // for code folding
void PrintCompress16x8Tables() {
  printf("======================================= 16x8\n");
  constexpr size_t N = 8;  // 128-bit SIMD
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint8_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    // Doubled (for converting lane to byte indices)
    for (size_t i = 0; i < N; ++i) {
      printf("%d,", 2 * indices[i]);
    }
  }
  printf("\n");
}

// Similar to the above, but uses native 16-bit shuffle instead of bytes.
void PrintCompress16x16HalfTables() {
  printf("======================================= 16x16Half\n");
  constexpr size_t N = 8;
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint8_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    for (size_t i = 0; i < N; ++i) {
      printf("%d,", indices[i]);
    }
    printf("\n");
  }
  printf("\n");
}

// Compressed to nibbles
void PrintCompress32x8Tables() {
  printf("======================================= 32x8\n");
  constexpr size_t N = 8;  // AVX2
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint32_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    // Convert to nibbles
    uint64_t packed = 0;
    for (size_t i = 0; i < N; ++i) {
      HWY_ASSERT(indices[i] < 16);
      packed += indices[i] << (i * 4);
    }

    HWY_ASSERT(packed < (1ull << 32));
    printf("0x%08x,", static_cast<uint32_t>(packed));
  }
  printf("\n");
}

// Pairs of 32-bit lane indices
void PrintCompress64x4Tables() {
  printf("======================================= 64x4\n");
  constexpr size_t N = 4;  // AVX2
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint32_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    for (size_t i = 0; i < N; ++i) {
      printf("%d,%d,", 2 * indices[i], 2 * indices[i] + 1);
    }
  }
  printf("\n");
}

// 4-tuple of byte indices
void PrintCompress32x4Tables() {
  printf("======================================= 32x4\n");
  using T = uint32_t;
  constexpr size_t N = 4;  // SSE4
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint32_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    for (size_t i = 0; i < N; ++i) {
      for (size_t idx_byte = 0; idx_byte < sizeof(T); ++idx_byte) {
        printf("%" PRIu64 ",",
               static_cast<uint64_t>(sizeof(T) * indices[i] + idx_byte));
      }
    }
  }
  printf("\n");
}

// 8-tuple of byte indices
void PrintCompress64x2Tables() {
  printf("======================================= 64x2\n");
  using T = uint64_t;
  constexpr size_t N = 2;  // SSE4
  for (uint64_t code = 0; code < 1ull << N; ++code) {
    std::array<uint32_t, N> indices{0};
    size_t pos = 0;
    for (size_t i = 0; i < N; ++i) {
      if (code & (1ull << i)) {
        indices[pos++] = i;
      }
    }

    for (size_t i = 0; i < N; ++i) {
      for (size_t idx_byte = 0; idx_byte < sizeof(T); ++idx_byte) {
        printf("%" PRIu64 ",",
               static_cast<uint64_t>(sizeof(T) * indices[i] + idx_byte));
      }
    }
  }
  printf("\n");
}
}  // namespace detail
#endif  // HWY_PRINT_TABLES

HWY_NOINLINE void TestAllCompress() {
#if HWY_PRINT_TABLES
  detail::PrintCompress32x8Tables();
  detail::PrintCompress64x4Tables();
  detail::PrintCompress32x4Tables();
  detail::PrintCompress64x2Tables();
  detail::PrintCompress16x8Tables();
  detail::PrintCompress16x16HalfTables();
#endif

  const ForPartialVectors<TestCompress> test;

  test(uint16_t());
  test(int16_t());
#if HWY_HAVE_FLOAT16
  test(float16_t());
#endif

  ForUIF3264(test);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwySwizzleTest);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllGetLane);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllDupEven);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllDupOdd);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllOddEven);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllOddEvenBlocks);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllSwapAdjacentBlocks);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllTableLookupLanes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllReverse);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllReverse2);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllReverse4);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllReverse8);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllReverseBlocks);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllCompress);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif
