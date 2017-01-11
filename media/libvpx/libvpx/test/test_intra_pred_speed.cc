/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
//  Test and time VP9 intra-predictor functions

#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vp9_rtcd.h"
#include "test/acm_random.h"
#include "test/md5_helper.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/vpx_timer.h"

// -----------------------------------------------------------------------------

namespace {

typedef void (*VpxPredFunc)(uint8_t *dst, ptrdiff_t y_stride,
                            const uint8_t *above, const uint8_t *left);

const int kNumVp9IntraPredFuncs = 13;
const char *kVp9IntraPredNames[kNumVp9IntraPredFuncs] = {
  "DC_PRED", "DC_LEFT_PRED", "DC_TOP_PRED", "DC_128_PRED", "V_PRED", "H_PRED",
  "D45_PRED", "D135_PRED", "D117_PRED", "D153_PRED", "D207_PRED", "D63_PRED",
  "TM_PRED"
};

void TestIntraPred(const char name[], VpxPredFunc const *pred_funcs,
                   const char *const pred_func_names[], int num_funcs,
                   const char *const signatures[], int block_size,
                   int num_pixels_per_test) {
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
  const int kBPS = 32;
  const int kTotalPixels = 32 * kBPS;
  DECLARE_ALIGNED(16, uint8_t, src[kTotalPixels]);
  DECLARE_ALIGNED(16, uint8_t, ref_src[kTotalPixels]);
  DECLARE_ALIGNED(16, uint8_t, left[kBPS]);
  DECLARE_ALIGNED(16, uint8_t, above_mem[2 * kBPS + 16]);
  uint8_t *const above = above_mem + 16;
  for (int i = 0; i < kTotalPixels; ++i) ref_src[i] = rnd.Rand8();
  for (int i = 0; i < kBPS; ++i) left[i] = rnd.Rand8();
  for (int i = -1; i < kBPS; ++i) above[i] = rnd.Rand8();
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);

  // some code assumes the top row has been extended:
  // d45/d63 C-code, for instance, but not the assembly.
  // TODO(jzern): this style of extension isn't strictly necessary.
  ASSERT_LE(block_size, kBPS);
  memset(above + block_size, above[block_size - 1], 2 * kBPS - block_size);

  for (int k = 0; k < num_funcs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(src, ref_src, sizeof(src));
    vpx_usec_timer timer;
    vpx_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](src, kBPS, above, left);
    }
    vpx_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(vpx_usec_timer_elapsed(&timer) / 1000);
    libvpx_test::MD5 md5;
    md5.Add(src, sizeof(src));
    printf("Mode %s[%12s]: %5d ms     MD5: %s\n", name, pred_func_names[k],
           elapsed_time, md5.Get());
    EXPECT_STREQ(signatures[k], md5.Get());
  }
}

void TestIntraPred4(VpxPredFunc const *pred_funcs) {
  static const int kNumVp9IntraFuncs = 13;
  static const char *const kSignatures[kNumVp9IntraFuncs] = {
    "4334156168b34ab599d9b5b30f522fe9",
    "bc4649d5ba47c7ff178d92e475960fb0",
    "8d316e5933326dcac24e1064794b5d12",
    "a27270fed024eafd762c95de85f4da51",
    "c33dff000d4256c2b8f3bf9e9bab14d2",
    "44d8cddc2ad8f79b8ed3306051722b4f",
    "eb54839b2bad6699d8946f01ec041cd0",
    "ecb0d56ae5f677ea45127ce9d5c058e4",
    "0b7936841f6813da818275944895b574",
    "9117972ef64f91a58ff73e1731c81db2",
    "c56d5e8c729e46825f46dd5d3b5d508a",
    "c0889e2039bcf7bcb5d2f33cdca69adc",
    "309a618577b27c648f9c5ee45252bc8f",
  };
  TestIntraPred("Intra4", pred_funcs, kVp9IntraPredNames, kNumVp9IntraFuncs,
                kSignatures, 4, 4 * 4 * kNumVp9IntraFuncs);
}

void TestIntraPred8(VpxPredFunc const *pred_funcs) {
  static const int kNumVp9IntraFuncs = 13;
  static const char *const kSignatures[kNumVp9IntraFuncs] = {
    "7694ddeeefed887faf9d339d18850928",
    "7d726b1213591b99f736be6dec65065b",
    "19c5711281357a485591aaf9c96c0a67",
    "ba6b66877a089e71cd938e3b8c40caac",
    "802440c93317e0f8ba93fab02ef74265",
    "9e09a47a15deb0b9d8372824f9805080",
    "b7c2d8c662268c0c427da412d7b0311d",
    "78339c1c60bb1d67d248ab8c4da08b7f",
    "5c97d70f7d47de1882a6cd86c165c8a9",
    "8182bf60688b42205acd95e59e967157",
    "08323400005a297f16d7e57e7fe1eaac",
    "95f7bfc262329a5849eda66d8f7c68ce",
    "815b75c8e0d91cc1ae766dc5d3e445a3",
  };
  TestIntraPred("Intra8", pred_funcs, kVp9IntraPredNames, kNumVp9IntraFuncs,
                kSignatures, 8, 8 * 8 * kNumVp9IntraFuncs);
}

void TestIntraPred16(VpxPredFunc const *pred_funcs) {
  static const int kNumVp9IntraFuncs = 13;
  static const char *const kSignatures[kNumVp9IntraFuncs] = {
    "b40dbb555d5d16a043dc361e6694fe53",
    "fb08118cee3b6405d64c1fd68be878c6",
    "6c190f341475c837cc38c2e566b64875",
    "db5c34ccbe2c7f595d9b08b0dc2c698c",
    "a62cbfd153a1f0b9fed13e62b8408a7a",
    "143df5b4c89335e281103f610f5052e4",
    "d87feb124107cdf2cfb147655aa0bb3c",
    "7841fae7d4d47b519322e6a03eeed9dc",
    "f6ebed3f71cbcf8d6d0516ce87e11093",
    "3cc480297dbfeed01a1c2d78dd03d0c5",
    "b9f69fa6532b372c545397dcb78ef311",
    "a8fe1c70432f09d0c20c67bdb6432c4d",
    "b8a41aa968ec108af447af4217cba91b",
  };
  TestIntraPred("Intra16", pred_funcs, kVp9IntraPredNames, kNumVp9IntraFuncs,
                kSignatures, 16, 16 * 16 * kNumVp9IntraFuncs);
}

void TestIntraPred32(VpxPredFunc const *pred_funcs) {
  static const int kNumVp9IntraFuncs = 13;
  static const char *const kSignatures[kNumVp9IntraFuncs] = {
    "558541656d84f9ae7896db655826febe",
    "b3587a1f9a01495fa38c8cd3c8e2a1bf",
    "4c6501e64f25aacc55a2a16c7e8f0255",
    "b3b01379ba08916ef6b1b35f7d9ad51c",
    "0f1eb38b6cbddb3d496199ef9f329071",
    "911c06efb9ed1c3b4c104b232b55812f",
    "9225beb0ddfa7a1d24eaa1be430a6654",
    "0a6d584a44f8db9aa7ade2e2fdb9fc9e",
    "b01c9076525216925f3456f034fb6eee",
    "d267e20ad9e5cd2915d1a47254d3d149",
    "ed012a4a5da71f36c2393023184a0e59",
    "f162b51ed618d28b936974cff4391da5",
    "9e1370c6d42e08d357d9612c93a71cfc",
  };
  TestIntraPred("Intra32", pred_funcs, kVp9IntraPredNames, kNumVp9IntraFuncs,
                kSignatures, 32, 32 * 32 * kNumVp9IntraFuncs);
}

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to |test_func|. The test name is 'arch.test_func', e.g., C.TestIntraPred4.
#define INTRA_PRED_TEST(arch, test_func, dc, dc_left, dc_top, dc_128, v, h, \
                        d45, d135, d117, d153, d207, d63, tm)               \
  TEST(arch, test_func) {                                                   \
    static const VpxPredFunc vp9_intra_pred[] = {                           \
        dc,   dc_left, dc_top, dc_128, v,   h, d45,                         \
        d135, d117,    d153,   d207,   d63, tm};                            \
    test_func(vp9_intra_pred);                                              \
  }

// -----------------------------------------------------------------------------
// 4x4

INTRA_PRED_TEST(C, TestIntraPred4, vp9_dc_predictor_4x4_c,
                vp9_dc_left_predictor_4x4_c, vp9_dc_top_predictor_4x4_c,
                vp9_dc_128_predictor_4x4_c, vp9_v_predictor_4x4_c,
                vp9_h_predictor_4x4_c, vp9_d45_predictor_4x4_c,
                vp9_d135_predictor_4x4_c, vp9_d117_predictor_4x4_c,
                vp9_d153_predictor_4x4_c, vp9_d207_predictor_4x4_c,
                vp9_d63_predictor_4x4_c, vp9_tm_predictor_4x4_c)

#if HAVE_SSE
INTRA_PRED_TEST(SSE, TestIntraPred4, vp9_dc_predictor_4x4_sse,
                vp9_dc_left_predictor_4x4_sse, vp9_dc_top_predictor_4x4_sse,
                vp9_dc_128_predictor_4x4_sse, vp9_v_predictor_4x4_sse, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, vp9_tm_predictor_4x4_sse)
#endif  // HAVE_SSE

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred4, NULL, NULL, NULL, NULL, NULL,
                vp9_h_predictor_4x4_ssse3, vp9_d45_predictor_4x4_ssse3, NULL,
                NULL, vp9_d153_predictor_4x4_ssse3,
                vp9_d207_predictor_4x4_ssse3, vp9_d63_predictor_4x4_ssse3, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred4, vp9_dc_predictor_4x4_dspr2, NULL, NULL,
                NULL, NULL, vp9_h_predictor_4x4_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, vp9_tm_predictor_4x4_dspr2)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred4, NULL, NULL, NULL, NULL,
                vp9_v_predictor_4x4_neon, vp9_h_predictor_4x4_neon, NULL, NULL,
                NULL, NULL, NULL, NULL, vp9_tm_predictor_4x4_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 8x8

INTRA_PRED_TEST(C, TestIntraPred8, vp9_dc_predictor_8x8_c,
                vp9_dc_left_predictor_8x8_c, vp9_dc_top_predictor_8x8_c,
                vp9_dc_128_predictor_8x8_c, vp9_v_predictor_8x8_c,
                vp9_h_predictor_8x8_c, vp9_d45_predictor_8x8_c,
                vp9_d135_predictor_8x8_c, vp9_d117_predictor_8x8_c,
                vp9_d153_predictor_8x8_c, vp9_d207_predictor_8x8_c,
                vp9_d63_predictor_8x8_c, vp9_tm_predictor_8x8_c)

#if HAVE_SSE
INTRA_PRED_TEST(SSE, TestIntraPred8, vp9_dc_predictor_8x8_sse,
                vp9_dc_left_predictor_8x8_sse, vp9_dc_top_predictor_8x8_sse,
                vp9_dc_128_predictor_8x8_sse, vp9_v_predictor_8x8_sse, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, vp9_tm_predictor_8x8_sse2)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred8, NULL, NULL, NULL, NULL, NULL,
                vp9_h_predictor_8x8_ssse3, vp9_d45_predictor_8x8_ssse3, NULL,
                NULL, vp9_d153_predictor_8x8_ssse3,
                vp9_d207_predictor_8x8_ssse3, vp9_d63_predictor_8x8_ssse3, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred8, vp9_dc_predictor_8x8_dspr2, NULL, NULL,
                NULL, NULL, vp9_h_predictor_8x8_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, vp9_tm_predictor_8x8_c)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred8, vp9_dc_predictor_8x8_neon,
                vp9_dc_left_predictor_8x8_neon, vp9_dc_top_predictor_8x8_neon,
                vp9_dc_128_predictor_8x8_neon, vp9_v_predictor_8x8_neon,
                vp9_h_predictor_8x8_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                vp9_tm_predictor_8x8_neon)

#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 16x16

INTRA_PRED_TEST(C, TestIntraPred16, vp9_dc_predictor_16x16_c,
                vp9_dc_left_predictor_16x16_c, vp9_dc_top_predictor_16x16_c,
                vp9_dc_128_predictor_16x16_c, vp9_v_predictor_16x16_c,
                vp9_h_predictor_16x16_c, vp9_d45_predictor_16x16_c,
                vp9_d135_predictor_16x16_c, vp9_d117_predictor_16x16_c,
                vp9_d153_predictor_16x16_c, vp9_d207_predictor_16x16_c,
                vp9_d63_predictor_16x16_c, vp9_tm_predictor_16x16_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred16, vp9_dc_predictor_16x16_sse2,
                vp9_dc_left_predictor_16x16_sse2,
                vp9_dc_top_predictor_16x16_sse2,
                vp9_dc_128_predictor_16x16_sse2, vp9_v_predictor_16x16_sse2,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                vp9_tm_predictor_16x16_sse2)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred16, NULL, NULL, NULL, NULL, NULL,
                vp9_h_predictor_16x16_ssse3, vp9_d45_predictor_16x16_ssse3,
                NULL, NULL, vp9_d153_predictor_16x16_ssse3,
                vp9_d207_predictor_16x16_ssse3, vp9_d63_predictor_16x16_ssse3,
                NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred16, vp9_dc_predictor_16x16_dspr2, NULL,
                NULL, NULL, NULL, vp9_h_predictor_16x16_dspr2, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred16, vp9_dc_predictor_16x16_neon,
                vp9_dc_left_predictor_16x16_neon,
                vp9_dc_top_predictor_16x16_neon,
                vp9_dc_128_predictor_16x16_neon, vp9_v_predictor_16x16_neon,
                vp9_h_predictor_16x16_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                vp9_tm_predictor_16x16_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 32x32

INTRA_PRED_TEST(C, TestIntraPred32, vp9_dc_predictor_32x32_c,
                vp9_dc_left_predictor_32x32_c, vp9_dc_top_predictor_32x32_c,
                vp9_dc_128_predictor_32x32_c, vp9_v_predictor_32x32_c,
                vp9_h_predictor_32x32_c, vp9_d45_predictor_32x32_c,
                vp9_d135_predictor_32x32_c, vp9_d117_predictor_32x32_c,
                vp9_d153_predictor_32x32_c, vp9_d207_predictor_32x32_c,
                vp9_d63_predictor_32x32_c, vp9_tm_predictor_32x32_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred32, vp9_dc_predictor_32x32_sse2,
                vp9_dc_left_predictor_32x32_sse2,
                vp9_dc_top_predictor_32x32_sse2,
                vp9_dc_128_predictor_32x32_sse2, vp9_v_predictor_32x32_sse2,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL,
#if ARCH_X86_64
                vp9_tm_predictor_32x32_sse2
#else
                NULL
#endif
               )
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred32, NULL, NULL, NULL, NULL, NULL,
                vp9_h_predictor_32x32_ssse3, vp9_d45_predictor_32x32_ssse3,
                NULL, NULL, NULL, vp9_d207_predictor_32x32_ssse3,
                vp9_d63_predictor_32x32_ssse3, NULL)
#endif  // HAVE_SSSE3

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred32, NULL, NULL, NULL, NULL,
                vp9_v_predictor_32x32_neon, vp9_h_predictor_32x32_neon, NULL,
                NULL, NULL, NULL, NULL, NULL, vp9_tm_predictor_32x32_neon)
#endif  // HAVE_NEON

#include "test/test_libvpx.cc"
