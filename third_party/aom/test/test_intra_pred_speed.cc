/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

//  Test and time AOM intra-predictor functions

#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/md5_helper.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"

// -----------------------------------------------------------------------------

namespace {

typedef void (*AvxPredFunc)(uint8_t *dst, ptrdiff_t y_stride,
                            const uint8_t *above, const uint8_t *left);

const int kNumAv1IntraFuncs = INTRA_MODES + 3;  // 4 DC predictor variants.
const char *kAv1IntraPredNames[kNumAv1IntraFuncs] = {
  "DC_PRED",       "DC_LEFT_PRED",  "DC_TOP_PRED", "DC_128_PRED", "V_PRED",
  "H_PRED",        "D45_PRED",      "D135_PRED",   "D117_PRED",   "D153_PRED",
  "D207_PRED",     "D63_PRED",      "TM_PRED",
#if CONFIG_ALT_INTRA
  "SMOOTH_PRED",
#if CONFIG_SMOOTH_HV
  "SMOOTH_V_PRED", "SMOOTH_H_PRED",
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
};

void TestIntraPred(const char name[], AvxPredFunc const *pred_funcs,
                   const char *const pred_func_names[], int num_funcs,
                   const char *const signatures[], int /*block_size*/,
                   int num_pixels_per_test) {
  libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
  const int kBPS = 32;
  const int kTotalPixels = 32 * kBPS;
  DECLARE_ALIGNED(16, uint8_t, src[kTotalPixels]);
  DECLARE_ALIGNED(16, uint8_t, ref_src[kTotalPixels]);
  DECLARE_ALIGNED(16, uint8_t, left[2 * kBPS]);
  DECLARE_ALIGNED(16, uint8_t, above_mem[2 * kBPS + 16]);
  uint8_t *const above = above_mem + 16;
  for (int i = 0; i < kTotalPixels; ++i) ref_src[i] = rnd.Rand8();
  for (int i = 0; i < kBPS; ++i) left[i] = rnd.Rand8();
  for (int i = -1; i < kBPS; ++i) above[i] = rnd.Rand8();
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);

  // Fill up bottom-left and top-right pixels.
  for (int i = kBPS; i < 2 * kBPS; ++i) {
    left[i] = rnd.Rand8();
    above[i] = rnd.Rand8();
  }

  for (int k = 0; k < num_funcs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(src, ref_src, sizeof(src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](src, kBPS, above, left);
    }
    libaom_test::ClearSystemState();
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    libaom_test::MD5 md5;
    md5.Add(src, sizeof(src));
    printf("Mode %s[%12s]: %5d ms     MD5: %s\n", name, pred_func_names[k],
           elapsed_time, md5.Get());
    EXPECT_STREQ(signatures[k], md5.Get());
  }
}

void TestIntraPred4(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "4334156168b34ab599d9b5b30f522fe9",
    "bc4649d5ba47c7ff178d92e475960fb0",
    "8d316e5933326dcac24e1064794b5d12",
    "a27270fed024eafd762c95de85f4da51",
    "c33dff000d4256c2b8f3bf9e9bab14d2",
    "44d8cddc2ad8f79b8ed3306051722b4f",
    "df62e96dfcb25d8a435482756a6fa990",
    "ecb0d56ae5f677ea45127ce9d5c058e4",
    "0b7936841f6813da818275944895b574",
    "9117972ef64f91a58ff73e1731c81db2",
    "46d493dccf6e5356c6f3c0c73b7dd141",
    "b852f42e6c4991d415400332d567872f",
#if CONFIG_ALT_INTRA
    "828c49a4248993cce4876fa26eab697f",
    "718c8cee9011f92ef31f77a9a7560010",
#if CONFIG_SMOOTH_HV
    "b37eeadbbd9e3bdff023a5097b59213a",
    "d6fb9c659d82c78f0d0c891da6cba87f",
#endif  // CONFIG_SMOOTH_HV
#else
    "309a618577b27c648f9c5ee45252bc8f",
#endif  // CONFIG_ALT_INTRA
  };
  TestIntraPred("Intra4", pred_funcs, kAv1IntraPredNames, kNumAv1IntraFuncs,
                kSignatures, 4, 4 * 4 * kNumAv1IntraFuncs);
}

void TestIntraPred8(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "7694ddeeefed887faf9d339d18850928",
    "7d726b1213591b99f736be6dec65065b",
    "19c5711281357a485591aaf9c96c0a67",
    "ba6b66877a089e71cd938e3b8c40caac",
    "802440c93317e0f8ba93fab02ef74265",
    "9e09a47a15deb0b9d8372824f9805080",
    "a2fd4b66e1a667a3e582588934a7e4bd",
    "78339c1c60bb1d67d248ab8c4da08b7f",
    "5c97d70f7d47de1882a6cd86c165c8a9",
    "8182bf60688b42205acd95e59e967157",
    "9d69fcaf12398e67242d3fcf5cf2267e",
    "7a09adb0fa6c2bf889a99dd816622feb",
#if CONFIG_ALT_INTRA
    "f6ade499c626d38eb70661184b79bc57",
    "1ad5b106c79b792e514ba25e87139b5e",
#if CONFIG_SMOOTH_HV
    "fe0d359b91a1d8141483d2e032f1b75f",
    "0cfd7603ced02829d1ce18b6795d73d0",
#endif  // CONFIG_SMOOTH_HV
#else
    "815b75c8e0d91cc1ae766dc5d3e445a3",
#endif  // CONFIG_ALT_INTRA
  };
  TestIntraPred("Intra8", pred_funcs, kAv1IntraPredNames, kNumAv1IntraFuncs,
                kSignatures, 8, 8 * 8 * kNumAv1IntraFuncs);
}

void TestIntraPred16(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "b40dbb555d5d16a043dc361e6694fe53",
    "fb08118cee3b6405d64c1fd68be878c6",
    "6c190f341475c837cc38c2e566b64875",
    "db5c34ccbe2c7f595d9b08b0dc2c698c",
    "a62cbfd153a1f0b9fed13e62b8408a7a",
    "143df5b4c89335e281103f610f5052e4",
    "404944b521d16f6edd160feeeb31ff35",
    "7841fae7d4d47b519322e6a03eeed9dc",
    "f6ebed3f71cbcf8d6d0516ce87e11093",
    "3cc480297dbfeed01a1c2d78dd03d0c5",
    "fbd607f15da218c5390a5b183b634a10",
    "f7063ccbc29f87303d5c3d0555b08944",
#if CONFIG_ALT_INTRA
    "7adcaaa3554eb71a81fc48cb9043984b",
    "c0acea4397c1b4d54a21bbcec5731dff",
#if CONFIG_SMOOTH_HV
    "f15b8712f0f064e98a7d804d3074afa7",
    "01a09cdb8edd06d840c84643032fc02f",
#endif  // CONFIG_SMOOTH_HV
#else
    "b8a41aa968ec108af447af4217cba91b",
#endif  // CONFIG_ALT_INTRA
  };
  TestIntraPred("Intra16", pred_funcs, kAv1IntraPredNames, kNumAv1IntraFuncs,
                kSignatures, 16, 16 * 16 * kNumAv1IntraFuncs);
}

void TestIntraPred32(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "558541656d84f9ae7896db655826febe",
    "b3587a1f9a01495fa38c8cd3c8e2a1bf",
    "4c6501e64f25aacc55a2a16c7e8f0255",
    "b3b01379ba08916ef6b1b35f7d9ad51c",
    "0f1eb38b6cbddb3d496199ef9f329071",
    "911c06efb9ed1c3b4c104b232b55812f",
    "b4f9f177a8a259514f039cfb403a61e3",
    "0a6d584a44f8db9aa7ade2e2fdb9fc9e",
    "b01c9076525216925f3456f034fb6eee",
    "d267e20ad9e5cd2915d1a47254d3d149",
    "3c45418137114cb6cef4c7a7baf4855c",
    "d520125ebd512c63c301bf67fea8e059",
#if CONFIG_ALT_INTRA
    "297e8fbb5d33c29b12b228fa9d7c40a4",
    "31b9296d70dd82238c87173e6d5e65fd",
#if CONFIG_SMOOTH_HV
    "f1041f77a34e86aaf30ea779ba84a2e8",
    "83e2b744a6a3d82321744442b1db945c",
#endif  // CONFIG_SMOOTH_HV
#else
    "9e1370c6d42e08d357d9612c93a71cfc",
#endif  // CONFIG_ALT_INTRA
  };
  TestIntraPred("Intra32", pred_funcs, kAv1IntraPredNames, kNumAv1IntraFuncs,
                kSignatures, 32, 32 * 32 * kNumAv1IntraFuncs);
}

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to |test_func|. The test name is 'arch.test_func', e.g., C.TestIntraPred4.
#define INTRA_PRED_TEST(arch, test_func, dc, dc_left, dc_top, dc_128, v, h, \
                        d45e, d135, d117, d153, d207e, d63e, tm, smooth,    \
                        smooth_v, smooth_h)                                 \
  TEST(arch, test_func) {                                                   \
    static const AvxPredFunc aom_intra_pred[] = {                           \
      dc,   dc_left, dc_top, dc_128, v,  h,      d45e,     d135,            \
      d117, d153,    d207e,  d63e,   tm, smooth, smooth_v, smooth_h         \
    };                                                                      \
    test_func(aom_intra_pred);                                              \
  }

// -----------------------------------------------------------------------------
// 4x4

#if CONFIG_ALT_INTRA
#define tm_pred_func aom_paeth_predictor_4x4_c
#define smooth_pred_func aom_smooth_predictor_4x4_c
#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_4x4_c
#define smooth_h_pred_func aom_smooth_h_predictor_4x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
#else
#define tm_pred_func aom_tm_predictor_4x4_c
#define smooth_pred_func NULL
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_ALT_INTRA

INTRA_PRED_TEST(C, TestIntraPred4, aom_dc_predictor_4x4_c,
                aom_dc_left_predictor_4x4_c, aom_dc_top_predictor_4x4_c,
                aom_dc_128_predictor_4x4_c, aom_v_predictor_4x4_c,
                aom_h_predictor_4x4_c, aom_d45e_predictor_4x4_c,
                aom_d135_predictor_4x4_c, aom_d117_predictor_4x4_c,
                aom_d153_predictor_4x4_c, aom_d207e_predictor_4x4_c,
                aom_d63e_predictor_4x4_c, tm_pred_func, smooth_pred_func,
                smooth_v_pred_func, smooth_h_pred_func)

#undef tm_pred_func
#undef smooth_pred_func
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_4x4_sse2
#endif  // CONFIG_ALT_INTRA

INTRA_PRED_TEST(SSE2, TestIntraPred4, aom_dc_predictor_4x4_sse2,
                aom_dc_left_predictor_4x4_sse2, aom_dc_top_predictor_4x4_sse2,
                aom_dc_128_predictor_4x4_sse2, aom_v_predictor_4x4_sse2,
                aom_h_predictor_4x4_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)

#undef tm_pred_func
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred4, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, aom_d153_predictor_4x4_ssse3, NULL,
                aom_d63e_predictor_4x4_ssse3, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_4x4_dspr2
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(DSPR2, TestIntraPred4, aom_dc_predictor_4x4_dspr2, NULL, NULL,
                NULL, NULL, aom_h_predictor_4x4_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_DSPR2

#if HAVE_NEON
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_4x4_neon
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(NEON, TestIntraPred4, aom_dc_predictor_4x4_neon,
                aom_dc_left_predictor_4x4_neon, aom_dc_top_predictor_4x4_neon,
                aom_dc_128_predictor_4x4_neon, aom_v_predictor_4x4_neon,
                aom_h_predictor_4x4_neon, NULL, aom_d135_predictor_4x4_neon,
                NULL, NULL, NULL, NULL, tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_NEON

#if HAVE_MSA
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_4x4_msa
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(MSA, TestIntraPred4, aom_dc_predictor_4x4_msa,
                aom_dc_left_predictor_4x4_msa, aom_dc_top_predictor_4x4_msa,
                aom_dc_128_predictor_4x4_msa, aom_v_predictor_4x4_msa,
                aom_h_predictor_4x4_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 8x8

#if CONFIG_ALT_INTRA
#define tm_pred_func aom_paeth_predictor_8x8_c
#define smooth_pred_func aom_smooth_predictor_8x8_c
#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_8x8_c
#define smooth_h_pred_func aom_smooth_h_predictor_8x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
#else
#define tm_pred_func aom_tm_predictor_8x8_c
#define smooth_pred_func NULL
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(C, TestIntraPred8, aom_dc_predictor_8x8_c,
                aom_dc_left_predictor_8x8_c, aom_dc_top_predictor_8x8_c,
                aom_dc_128_predictor_8x8_c, aom_v_predictor_8x8_c,
                aom_h_predictor_8x8_c, aom_d45e_predictor_8x8_c,
                aom_d135_predictor_8x8_c, aom_d117_predictor_8x8_c,
                aom_d153_predictor_8x8_c, aom_d207e_predictor_8x8_c,
                aom_d63e_predictor_8x8_c, tm_pred_func, smooth_pred_func,
                smooth_v_pred_func, smooth_h_pred_func)
#undef tm_pred_func
#undef smooth_pred_func
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_8x8_sse2
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(SSE2, TestIntraPred8, aom_dc_predictor_8x8_sse2,
                aom_dc_left_predictor_8x8_sse2, aom_dc_top_predictor_8x8_sse2,
                aom_dc_128_predictor_8x8_sse2, aom_v_predictor_8x8_sse2,
                aom_h_predictor_8x8_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, aom_d153_predictor_8x8_ssse3, NULL, NULL, NULL,
                NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_8x8_dspr2
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(DSPR2, TestIntraPred8, aom_dc_predictor_8x8_dspr2, NULL, NULL,
                NULL, NULL, aom_h_predictor_8x8_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_DSPR2

#if HAVE_NEON
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_8x8_neon
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(NEON, TestIntraPred8, aom_dc_predictor_8x8_neon,
                aom_dc_left_predictor_8x8_neon, aom_dc_top_predictor_8x8_neon,
                aom_dc_128_predictor_8x8_neon, aom_v_predictor_8x8_neon,
                aom_h_predictor_8x8_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_NEON

#if HAVE_MSA
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_8x8_msa
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(MSA, TestIntraPred8, aom_dc_predictor_8x8_msa,
                aom_dc_left_predictor_8x8_msa, aom_dc_top_predictor_8x8_msa,
                aom_dc_128_predictor_8x8_msa, aom_v_predictor_8x8_msa,
                aom_h_predictor_8x8_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 16x16

#if CONFIG_ALT_INTRA
#define tm_pred_func aom_paeth_predictor_16x16_c
#define smooth_pred_func aom_smooth_predictor_16x16_c
#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_16x16_c
#define smooth_h_pred_func aom_smooth_h_predictor_16x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
#else
#define tm_pred_func aom_tm_predictor_16x16_c
#define smooth_pred_func NULL
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(C, TestIntraPred16, aom_dc_predictor_16x16_c,
                aom_dc_left_predictor_16x16_c, aom_dc_top_predictor_16x16_c,
                aom_dc_128_predictor_16x16_c, aom_v_predictor_16x16_c,
                aom_h_predictor_16x16_c, aom_d45e_predictor_16x16_c,
                aom_d135_predictor_16x16_c, aom_d117_predictor_16x16_c,
                aom_d153_predictor_16x16_c, aom_d207e_predictor_16x16_c,
                aom_d63e_predictor_16x16_c, tm_pred_func, smooth_pred_func,
                smooth_v_pred_func, smooth_h_pred_func)
#undef tm_pred_func
#undef smooth_pred_func
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_16x16_sse2
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(SSE2, TestIntraPred16, aom_dc_predictor_16x16_sse2,
                aom_dc_left_predictor_16x16_sse2,
                aom_dc_top_predictor_16x16_sse2,
                aom_dc_128_predictor_16x16_sse2, aom_v_predictor_16x16_sse2,
                aom_h_predictor_16x16_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred16, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, aom_d153_predictor_16x16_ssse3, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred16, aom_dc_predictor_16x16_dspr2, NULL,
                NULL, NULL, NULL, aom_h_predictor_16x16_dspr2, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_16x16_neon
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(NEON, TestIntraPred16, aom_dc_predictor_16x16_neon,
                aom_dc_left_predictor_16x16_neon,
                aom_dc_top_predictor_16x16_neon,
                aom_dc_128_predictor_16x16_neon, aom_v_predictor_16x16_neon,
                aom_h_predictor_16x16_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_NEON

#if HAVE_MSA
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_16x16_msa
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(MSA, TestIntraPred16, aom_dc_predictor_16x16_msa,
                aom_dc_left_predictor_16x16_msa, aom_dc_top_predictor_16x16_msa,
                aom_dc_128_predictor_16x16_msa, aom_v_predictor_16x16_msa,
                aom_h_predictor_16x16_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 32x32

#if CONFIG_ALT_INTRA
#define tm_pred_func aom_paeth_predictor_32x32_c
#define smooth_pred_func aom_smooth_predictor_32x32_c
#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_32x32_c
#define smooth_h_pred_func aom_smooth_h_predictor_32x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
#else
#define tm_pred_func aom_tm_predictor_32x32_c
#define smooth_pred_func NULL
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(C, TestIntraPred32, aom_dc_predictor_32x32_c,
                aom_dc_left_predictor_32x32_c, aom_dc_top_predictor_32x32_c,
                aom_dc_128_predictor_32x32_c, aom_v_predictor_32x32_c,
                aom_h_predictor_32x32_c, aom_d45e_predictor_32x32_c,
                aom_d135_predictor_32x32_c, aom_d117_predictor_32x32_c,
                aom_d153_predictor_32x32_c, aom_d207e_predictor_32x32_c,
                aom_d63e_predictor_32x32_c, tm_pred_func, smooth_pred_func,
                smooth_v_pred_func, smooth_h_pred_func)
#undef tm_pred_func
#undef smooth_pred_func
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_32x32_sse2
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(SSE2, TestIntraPred32, aom_dc_predictor_32x32_sse2,
                aom_dc_left_predictor_32x32_sse2,
                aom_dc_top_predictor_32x32_sse2,
                aom_dc_128_predictor_32x32_sse2, aom_v_predictor_32x32_sse2,
                aom_h_predictor_32x32_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred32, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, aom_d153_predictor_32x32_ssse3, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_NEON
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_32x32_neon
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(NEON, TestIntraPred32, aom_dc_predictor_32x32_neon,
                aom_dc_left_predictor_32x32_neon,
                aom_dc_top_predictor_32x32_neon,
                aom_dc_128_predictor_32x32_neon, aom_v_predictor_32x32_neon,
                aom_h_predictor_32x32_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_NEON

#if HAVE_MSA
#if CONFIG_ALT_INTRA
#define tm_pred_func NULL
#else
#define tm_pred_func aom_tm_predictor_32x32_msa
#endif  // CONFIG_ALT_INTRA
INTRA_PRED_TEST(MSA, TestIntraPred32, aom_dc_predictor_32x32_msa,
                aom_dc_left_predictor_32x32_msa, aom_dc_top_predictor_32x32_msa,
                aom_dc_128_predictor_32x32_msa, aom_v_predictor_32x32_msa,
                aom_h_predictor_32x32_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                tm_pred_func, NULL, NULL, NULL)
#undef tm_pred_func
#endif  // HAVE_MSA

#include "test/test_libaom.cc"
