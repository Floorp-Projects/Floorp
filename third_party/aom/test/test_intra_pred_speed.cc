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

const int kBPS = 32;
const int kTotalPixels = kBPS * kBPS;
const int kNumAv1IntraFuncs = INTRA_MODES + 3;  // 4 DC predictor variants.
const char *kAv1IntraPredNames[kNumAv1IntraFuncs] = {
  "DC_PRED",       "DC_LEFT_PRED",  "DC_TOP_PRED", "DC_128_PRED", "V_PRED",
  "H_PRED",        "D45_PRED",      "D135_PRED",   "D117_PRED",   "D153_PRED",
  "D207_PRED",     "D63_PRED",      "TM_PRED",     "SMOOTH_PRED",
#if CONFIG_SMOOTH_HV
  "SMOOTH_V_PRED", "SMOOTH_H_PRED",
#endif  // CONFIG_SMOOTH_HV
};

template <typename Pixel>
struct IntraPredTestMem {
  void Init(int block_width, int bd) {
    libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
    Pixel *const above = above_mem + 16;
    const int mask = (1 << bd) - 1;
    for (int i = 0; i < kTotalPixels; ++i) ref_src[i] = rnd.Rand16() & mask;
    for (int i = 0; i < kBPS; ++i) left[i] = rnd.Rand16() & mask;
    for (int i = -1; i < kBPS; ++i) above[i] = rnd.Rand16() & mask;

    ASSERT_LE(block_width, kBPS);
    for (int i = kBPS; i < 2 * kBPS; ++i) {
      left[i] = rnd.Rand16() & mask;
      above[i] = rnd.Rand16() & mask;
    }
  }

  DECLARE_ALIGNED(16, Pixel, src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, ref_src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, left[2 * kBPS]);
  DECLARE_ALIGNED(16, Pixel, above_mem[2 * kBPS + 16]);
};

// -----------------------------------------------------------------------------
// Low Bittdepth

typedef IntraPredTestMem<uint8_t> Av1IntraPredTestMem;

// Note:
// APPLY_UNIT_TESTS
// 1: Do unit tests
// 0: Generate MD5 array as required
#define APPLY_UNIT_TESTS 1

void CheckMd5Signature(const char name[], const char *const signatures[],
                       const void *data, size_t data_size, int elapsed_time,
                       int idx) {
  libaom_test::MD5 md5;
  md5.Add(reinterpret_cast<const uint8_t *>(data), data_size);
#if APPLY_UNIT_TESTS
  printf("Mode %s[%13s]: %5d ms     MD5: %s\n", name, kAv1IntraPredNames[idx],
         elapsed_time, md5.Get());
  EXPECT_STREQ(signatures[idx], md5.Get());
#else
  printf("\"%s\",\n", md5.Get());
#endif
}

void TestIntraPred(const char name[], AvxPredFunc const *pred_funcs,
                   const char *const signatures[], int block_width,
                   int block_height) {
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1IntraPredTestMem intra_pred_test_mem;
  const uint8_t *const above = intra_pred_test_mem.above_mem + 16;

  intra_pred_test_mem.Init(block_width, 8);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, kBPS, above,
                    intra_pred_test_mem.left);
    }
    libaom_test::ClearSystemState();
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(name, signatures, intra_pred_test_mem.src,
                      sizeof(intra_pred_test_mem.src), elapsed_time, k);
  }
}

void TestIntraPred4(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures4x4[kNumAv1IntraFuncs] = {
    "e7ed7353c3383fff942e500e9bfe82fe",
    "2a4a26fcc6ce005eadc08354d196c8a9",
    "269d92eff86f315d9c38fe7640d85b15",
    "ae2960eea9f71ee3dabe08b282ec1773",
    "6c1abcc44e90148998b51acd11144e9c",
    "f7bb3186e1ef8a2b326037ff898cad8e",
    "87e72798518d62e84bcc77dcb17d0f3b",
    "141624072a4a56773f68fadbdd07c4a7",
    "7be49b08687a5f24df3a2c612fca3876",
    "459bb5d9fd5b238348179c9a22108cd6",
    "3d98810f418a9de92acfe2c68909c61c",
    "6310eecda3cc9496987ca10186255558",
    "59fc0e923a08cfac0a493fb38988e2bb",
    "9ff8bb37d9c830e6ab8ecb0c435d3c91",
#if CONFIG_SMOOTH_HV
    "de6937fca02354f2874dbc5dbec5d5b3",
    "723cf948137f7d8c7860d814e55ae67d",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures4x8[kNumAv1IntraFuncs] = {
    "d9fbebdc85f71ab1e18461b2db4a2adc",
    "5ccb2a68284bc9714d94b8a06ccadbb2",
    "735d059abc2744f3ff3f9590f7191b37",
    "d9fbebdc85f71ab1e18461b2db4a2adc",
    "6819497c44cd0ace120add83672996ee",
    "7e3244f5a2d3edf81c7e962a842b97f9",
    "3fa52ee9acf5a25594cac684be263f32",
    "c18dd23d57def4df4c6147c572dfc827",
    "d007fbf7e43cb8f49702daa20f0c9153",
    "5c0226c44c5df285728296b80cc6de4b",
    "b55d7b558bebc8c2042dfac58b3c4688",
    "6549362baa389b8faa2d954926b64e2f",
    "809350f164cd4d1650850bb0f59c3260",
    "1b60a394331eeab6927a6f8aaff57040",
#if CONFIG_SMOOTH_HV
    "5307de1bd7329ba6b281d2c1b0b457f9",
    "24c58a8138339846d95568efb91751db",
#endif
  };
  if (!strcmp(block_name, "intra4x4")) {
    TestIntraPred(block_name, pred_funcs, kSignatures4x4, 4, 4);
  }
  if (!strcmp(block_name, "intra4x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures4x8, 4, 8);
  }
}

void TestIntraPred8(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures8x8[kNumAv1IntraFuncs] = {
    "d8bbae5d6547cfc17e4f5f44c8730e88",
    "373bab6d931868d41a601d9d88ce9ac3",
    "6fdd5ff4ff79656c14747598ca9e3706",
    "d9661c2811d6a73674f40ffb2b841847",
    "7c722d10b19ccff0b8c171868e747385",
    "f81dd986eb2b50f750d3a7da716b7e27",
    "e0b1292448f3350bf1c92ca283ca872a",
    "0e3523f9cab2142dd37fd07ec0760bce",
    "79ac4efe907f0a0f1885d43066cfedee",
    "19ecf2432ac305057de3b6578474eec6",
    "7ae38292cbe47b4aa0807c3bd5a543df",
    "d0ecffec1bb01f4b61ab5738164695c4",
    "064404361748dd111a890a1470d7f0ea",
    "dc29b7e1f78cc8e7525d5ea4c0ab9b78",
#if CONFIG_SMOOTH_HV
    "97111eb1bc26bade6272015df829f1ae",
    "d19a8a73cc46b807f2c5e817576cc1e1",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures8x4[kNumAv1IntraFuncs] = {
    "23f9fc11344426c9bee2e06d57dfd628",
    "2d71a26d1bae1fb34734de7b42fc5eb7",
    "5af9c1b2fd9d5721fad67b67b3f7c816",
    "00d71b17be662753813d515f197d145e",
    "bef10ec984427e28f4390f43809d10af",
    "77773cdfb7ed6bc882ab202a64b0a470",
    "cba356970f6b9a1b6024e1dbe4a66f9b",
    "c58c21efc804242848e6f29a93a7984d",
    "dc92cc45a51c7a397506cab19f74e66d",
    "391f6a12224f81a3719ea09a2cf7a5ad",
    "b74b8b11f7eb2bbf723b25f381104ca9",
    "2234aaa06ca245624211cf53a0261017",
    "2cc48bd66d6b0121b5221d52ccd732af",
    "b302155e1c9eeeafe2ba2bf68e807a46",
#if CONFIG_SMOOTH_HV
    "561bc8d0e76d5041ebd5168fc6a115e1",
    "81d0113fb1d0a9a24ffd6f1987b77948",
#endif
  };
  static const char *const kSignatures8x16[kNumAv1IntraFuncs] = {
    "c849de88b24f773dfcdd1d48d1209796",
    "6cb807c1897b94866a0f3d3c56ed8695",
    "d56db05a8ac7981762f5b877f486c4ef",
    "b4bc01eb6e59a40922ad17715cafb04b",
    "09d178439534f4062ae687c351f66d64",
    "644501399cf73080ac606e5cef7ca09b",
    "0e8e968fa177204d7e73d7e04ce69ebb",
    "1d25f9287fdf7ba48a5105f1529b7e75",
    "02cacccf3752451763a6a6e2e784494f",
    "6044a1416d53e324ddc012d2e7763339",
    "57ac6e8f3ab5e943c9280043eeb174b8",
    "d51b9d65471194d9caebc7d67e75ef10",
    "278076495180e17c065a95ab7278539a",
    "9dd7f324816f242be408ffeb0c673732",
#if CONFIG_SMOOTH_HV
    "f520c4a20acfa0bea1d253c6f0f040fd",
    "85f38df809df2c2d7c8b4a157a65cd44",
#endif
  };
  if (!strcmp(block_name, "intra8x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x8, 8, 8);
  }
  if (!strcmp(block_name, "intra8x4")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x4, 8, 4);
  }
  if (!strcmp(block_name, "intra8x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x16, 8, 16);
  }
}

void TestIntraPred16(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures16x16[kNumAv1IntraFuncs] = {
    "50971c07ce26977d30298538fffec619",
    "527a6b9e0dc5b21b98cf276305432bef",
    "7eff2868f80ebc2c43a4f367281d80f7",
    "67cd60512b54964ef6aff1bd4816d922",
    "48371c87dc95c08a33b2048f89cf6468",
    "b0acf2872ee411d7530af6d2625a7084",
    "31d901ab2289d1e61e704e40240382a7",
    "dae208f3dca583529cff49b73f7c4183",
    "7af66a2f4c8e0b4908e40f047e60c47c",
    "125e3ab6ab9bc961f183ec366a7afa88",
    "ff230677e800977757d14b85a9eba404",
    "eb42dc39140515dd4f3ab1afe6c3e71b",
    "93d6b5352b571805ab16a55e1bbed86a",
    "03764e4c0aebbc180e4e2c68fb06df2b",
#if CONFIG_SMOOTH_HV
    "bb6c74c9076c9f266ab11fb57060d8e6",
    "0c5162bc28489756ddb847b5678e6f07",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures16x8[kNumAv1IntraFuncs] = {
    "b4cbdbdf10ce13300b4063a3daf99e04",
    "3731e1e6202064a9d0604d7c293ecee4",
    "6c856188c4256a06452f0d5d70cac436",
    "1f2192b4c8c497589484ea7bf9c944e8",
    "84011bd4b7f565119d06787840e333a0",
    "0e48949f7a6aa36f0d76b5d01f91124a",
    "58114c06f6b9d8285e5020c7afd834ab",
    "e37afe84a8b3c5e0f048d4652ecbe09e",
    "c216348473fb029b45f8fb4f2862a7bd",
    "0b7385155dcef742cc456d5741ae93a3",
    "d55fadb221f0ea20266e57cd413e7b94",
    "9bd6eb226c7e169b8d53cf70aea98b3a",
    "60eff8064634b6c73b10681356baeee9",
    "1559aeb081a9c0c71111d6093c2ff9fd",
#if CONFIG_SMOOTH_HV
    "c15479b739713773e5cabb748451987b",
    "72e33ec12c9b67aea26d8d005fb82de2",
#endif
  };
  static const char *const kSignatures16x32[kNumAv1IntraFuncs] = {
    "abe5233d189cdbf79424721571bbaa7b",
    "282759f81e3cfb2e2d396fe406b72a8b",
    "e2224926c264f6f174cbc3167a233168",
    "6814e85c2b33f8c9415d62e80394b47b",
    "99cbbb60459c08a3061d72c4e4f6276a",
    "1d1567d40b8e816f8c1f71e576fe0f87",
    "5e989f9c748a0d2cd8c4ebf9d3fe1278",
    "7135a2f419452a3a192a35156f68b019",
    "06e10af5a726d2c81b8f8c708204f9fb",
    "c0882f0e7ba1ffa0aeef6d5c751df6de",
    "8477429e17d39a423f30e2082f651549",
    "ba35068a30c2d1d10901e4bfabd02a11",
    "36fdd371b624a075814d497c4832ec85",
    "8ab8da61b727442b6ff692b40d0df018",
#if CONFIG_SMOOTH_HV
    "e35a10ad7fdf2327e821504a90f6a6eb",
    "1f7211e727dc1de7d6a55d082fbdd821",
#endif
  };
  if (!strcmp(block_name, "intra16x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x16, 16, 16);
  }
  if (!strcmp(block_name, "intra16x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x8, 16, 8);
  }
  if (!strcmp(block_name, "intra16x32")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x32, 16, 32);
  }
}

void TestIntraPred32(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures32x32[kNumAv1IntraFuncs] = {
    "a0a618c900e65ae521ccc8af789729f2",
    "985aaa7c72b4a6c2fb431d32100cf13a",
    "10662d09febc3ca13ee4e700120daeb5",
    "b3b01379ba08916ef6b1b35f7d9ad51c",
    "9f4261755795af97e34679c333ec7004",
    "bc2c9da91ad97ef0d1610fb0a9041657",
    "f524b1a7e31c7bb9bfb2487fac3e16d8",
    "4039bb7da0f6860090d3c57b5c85468f",
    "b29fff7b61804e68383e3a609b33da58",
    "e1aa5e49067fd8dba66c2eb8d07b7a89",
    "db217e7891581cf93895ef5974bebb21",
    "beb6cdc52b52c8976b4d2407ec8d2313",
    "ef1653982b69e1f64bee3759f3e1ec45",
    "1a51a675deba2c83282142eb48d3dc3d",
#if CONFIG_SMOOTH_HV
    "866c224746dc260cda861a7b1b383fb3",
    "cea23799fc3526e1b6a6ff02b42b82af",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures32x16[kNumAv1IntraFuncs] = {
    "d1aeb8d5fdcfd3307922af01a798a4dc",
    "b0bcb514ebfbee065faea9d34c12ae75",
    "d6a18c63b4e909871c0137ca652fad23",
    "fd047f2fc1b8ffb95d0eeef3e8796a45",
    "645ab60779ea348fd93c81561c31bab9",
    "4409633c9db8dff41ade4292a3a56e7f",
    "b9b2935b2287a9a461ac5c11251ac706",
    "43b05f808c0ac4fe8accd84d293b0488",
    "1d2cb43872d20c205ffb185102bcd22a",
    "2c1551b5e99592fd21053b5d14e397d9",
    "cd499ef0dd41e2e38d5dac3319dfdd97",
    "cd2610426637003f3b5d3984cb3320d5",
    "5e36a11e069b31c2a739f3a9c7b37c24",
    "e83b9483d702cfae496991c3c7fa92c0",
#if CONFIG_SMOOTH_HV
    "12f6ddf98c7f30a277307f1ea935b030",
    "354321d6c32bbdb0739e4fa2acbf41e1",
#endif
  };
  if (!strcmp(block_name, "intra32x32")) {
    TestIntraPred(block_name, pred_funcs, kSignatures32x32, 32, 32);
  }
  if (!strcmp(block_name, "intra32x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures32x16, 32, 16);
  }
}

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to |test_func|. The test name is 'arch.test_func', e.g., C.TestIntraPred4.
#define INTRA_PRED_TEST(arch, test_func, blk, dc, dc_left, dc_top, dc_128, v, \
                        h, d45e, d135, d117, d153, d207e, d63e, tm, smooth,   \
                        smooth_v, smooth_h)                                   \
  TEST(arch, DISABLED_##test_func) {                                          \
    static const AvxPredFunc aom_intra_pred[] = {                             \
      dc,   dc_left, dc_top, dc_128, v,  h,      d45e,     d135,              \
      d117, d153,    d207e,  d63e,   tm, smooth, smooth_v, smooth_h           \
    };                                                                        \
    test_func(blk, aom_intra_pred);                                           \
  }

// -----------------------------------------------------------------------------
// 4x4

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_4x4_c
#define smooth_h_pred_func aom_smooth_h_predictor_4x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

INTRA_PRED_TEST(C_1, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_c,
                aom_dc_left_predictor_4x4_c, aom_dc_top_predictor_4x4_c,
                aom_dc_128_predictor_4x4_c, aom_v_predictor_4x4_c,
                aom_h_predictor_4x4_c, aom_d45e_predictor_4x4_c,
                aom_d135_predictor_4x4_c, aom_d117_predictor_4x4_c,
                aom_d153_predictor_4x4_c, aom_d207e_predictor_4x4_c,
                aom_d63e_predictor_4x4_c, aom_paeth_predictor_4x4_c,
                aom_smooth_predictor_4x4_c, smooth_v_pred_func,
                smooth_h_pred_func)

#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_4x8_c
#define smooth_h_pred_func aom_smooth_h_predictor_4x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

INTRA_PRED_TEST(C_2, TestIntraPred4, "intra4x8", aom_dc_predictor_4x8_c,
                aom_dc_left_predictor_4x8_c, aom_dc_top_predictor_4x8_c,
                aom_dc_128_predictor_4x8_c, aom_v_predictor_4x8_c,
                aom_h_predictor_4x8_c, aom_d45e_predictor_4x8_c,
                aom_d135_predictor_4x8_c, aom_d117_predictor_4x8_c,
                aom_d153_predictor_4x8_c, aom_d207e_predictor_4x8_c,
                aom_d63e_predictor_4x8_c, aom_paeth_predictor_4x8_c,
                aom_smooth_predictor_4x8_c, smooth_v_pred_func,
                smooth_h_pred_func)

#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_sse2,
                aom_dc_left_predictor_4x4_sse2, aom_dc_top_predictor_4x4_sse2,
                aom_dc_128_predictor_4x4_sse2, aom_v_predictor_4x4_sse2,
                aom_h_predictor_4x4_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred4, "intra4x8", aom_dc_predictor_4x8_sse2,
                aom_dc_left_predictor_4x8_sse2, aom_dc_top_predictor_4x8_sse2,
                aom_dc_128_predictor_4x8_sse2, aom_v_predictor_4x8_sse2,
                aom_h_predictor_4x8_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred4, "intra4x4", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, aom_d153_predictor_4x4_ssse3,
                NULL, aom_d63e_predictor_4x4_ssse3,
                aom_paeth_predictor_4x4_ssse3, aom_smooth_predictor_4x4_ssse3,
                NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred4, "intra4x8", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_4x8_ssse3, aom_smooth_predictor_4x8_ssse3,
                NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_dspr2,
                NULL, NULL, NULL, NULL, aom_h_predictor_4x4_dspr2, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_neon,
                aom_dc_left_predictor_4x4_neon, aom_dc_top_predictor_4x4_neon,
                aom_dc_128_predictor_4x4_neon, aom_v_predictor_4x4_neon,
                aom_h_predictor_4x4_neon, NULL, aom_d135_predictor_4x4_neon,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_msa,
                aom_dc_left_predictor_4x4_msa, aom_dc_top_predictor_4x4_msa,
                aom_dc_128_predictor_4x4_msa, aom_v_predictor_4x4_msa,
                aom_h_predictor_4x4_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 8x8

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_8x8_c
#define smooth_h_pred_func aom_smooth_h_predictor_8x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_1, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_c,
                aom_dc_left_predictor_8x8_c, aom_dc_top_predictor_8x8_c,
                aom_dc_128_predictor_8x8_c, aom_v_predictor_8x8_c,
                aom_h_predictor_8x8_c, aom_d45e_predictor_8x8_c,
                aom_d135_predictor_8x8_c, aom_d117_predictor_8x8_c,
                aom_d153_predictor_8x8_c, aom_d207e_predictor_8x8_c,
                aom_d63e_predictor_8x8_c, aom_paeth_predictor_8x8_c,
                aom_smooth_predictor_8x8_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_8x4_c
#define smooth_h_pred_func aom_smooth_h_predictor_8x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_2, TestIntraPred8, "intra8x4", aom_dc_predictor_8x4_c,
                aom_dc_left_predictor_8x4_c, aom_dc_top_predictor_8x4_c,
                aom_dc_128_predictor_8x4_c, aom_v_predictor_8x4_c,
                aom_h_predictor_8x4_c, aom_d45e_predictor_8x4_c,
                aom_d135_predictor_8x4_c, aom_d117_predictor_8x4_c,
                aom_d153_predictor_8x4_c, aom_d207e_predictor_8x4_c,
                aom_d63e_predictor_8x4_c, aom_paeth_predictor_8x4_c,
                aom_smooth_predictor_8x4_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_8x16_c
#define smooth_h_pred_func aom_smooth_h_predictor_8x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_3, TestIntraPred8, "intra8x16", aom_dc_predictor_8x16_c,
                aom_dc_left_predictor_8x16_c, aom_dc_top_predictor_8x16_c,
                aom_dc_128_predictor_8x16_c, aom_v_predictor_8x16_c,
                aom_h_predictor_8x16_c, aom_d45e_predictor_8x16_c,
                aom_d135_predictor_8x16_c, aom_d117_predictor_8x16_c,
                aom_d153_predictor_8x16_c, aom_d207e_predictor_8x16_c,
                aom_d63e_predictor_8x16_c, aom_paeth_predictor_8x16_c,
                aom_smooth_predictor_8x16_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_sse2,
                aom_dc_left_predictor_8x8_sse2, aom_dc_top_predictor_8x8_sse2,
                aom_dc_128_predictor_8x8_sse2, aom_v_predictor_8x8_sse2,
                aom_h_predictor_8x8_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred8, "intra8x4", aom_dc_predictor_8x4_sse2,
                aom_dc_left_predictor_8x4_sse2, aom_dc_top_predictor_8x4_sse2,
                aom_dc_128_predictor_8x4_sse2, aom_v_predictor_8x4_sse2,
                aom_h_predictor_8x4_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_3, TestIntraPred8, "intra8x16", aom_dc_predictor_8x16_sse2,
                aom_dc_left_predictor_8x16_sse2, aom_dc_top_predictor_8x16_sse2,
                aom_dc_128_predictor_8x16_sse2, aom_v_predictor_8x16_sse2,
                aom_h_predictor_8x16_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred8, "intra8x8", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, aom_d153_predictor_8x8_ssse3,
                NULL, NULL, aom_paeth_predictor_8x8_ssse3,
                aom_smooth_predictor_8x8_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred8, "intra8x4", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_8x4_ssse3, aom_smooth_predictor_8x4_ssse3,
                NULL, NULL)
INTRA_PRED_TEST(SSSE3_3, TestIntraPred8, "intra8x16", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_8x16_ssse3, aom_smooth_predictor_8x16_ssse3,
                NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_dspr2,
                NULL, NULL, NULL, NULL, aom_h_predictor_8x8_dspr2, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_neon,
                aom_dc_left_predictor_8x8_neon, aom_dc_top_predictor_8x8_neon,
                aom_dc_128_predictor_8x8_neon, aom_v_predictor_8x8_neon,
                aom_h_predictor_8x8_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_msa,
                aom_dc_left_predictor_8x8_msa, aom_dc_top_predictor_8x8_msa,
                aom_dc_128_predictor_8x8_msa, aom_v_predictor_8x8_msa,
                aom_h_predictor_8x8_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 16x16

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_16x16_c
#define smooth_h_pred_func aom_smooth_h_predictor_16x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_1, TestIntraPred16, "intra16x16", aom_dc_predictor_16x16_c,
                aom_dc_left_predictor_16x16_c, aom_dc_top_predictor_16x16_c,
                aom_dc_128_predictor_16x16_c, aom_v_predictor_16x16_c,
                aom_h_predictor_16x16_c, aom_d45e_predictor_16x16_c,
                aom_d135_predictor_16x16_c, aom_d117_predictor_16x16_c,
                aom_d153_predictor_16x16_c, aom_d207e_predictor_16x16_c,
                aom_d63e_predictor_16x16_c, aom_paeth_predictor_16x16_c,
                aom_smooth_predictor_16x16_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_16x8_c
#define smooth_h_pred_func aom_smooth_h_predictor_16x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_2, TestIntraPred16, "intra16x8", aom_dc_predictor_16x8_c,
                aom_dc_left_predictor_16x8_c, aom_dc_top_predictor_16x8_c,
                aom_dc_128_predictor_16x8_c, aom_v_predictor_16x8_c,
                aom_h_predictor_16x8_c, aom_d45e_predictor_16x8_c,
                aom_d135_predictor_16x8_c, aom_d117_predictor_16x8_c,
                aom_d153_predictor_16x8_c, aom_d207e_predictor_16x8_c,
                aom_d63e_predictor_16x8_c, aom_paeth_predictor_16x8_c,
                aom_smooth_predictor_16x8_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_16x32_c
#define smooth_h_pred_func aom_smooth_h_predictor_16x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_3, TestIntraPred16, "intra16x32", aom_dc_predictor_16x32_c,
                aom_dc_left_predictor_16x32_c, aom_dc_top_predictor_16x32_c,
                aom_dc_128_predictor_16x32_c, aom_v_predictor_16x32_c,
                aom_h_predictor_16x32_c, aom_d45e_predictor_16x32_c,
                aom_d135_predictor_16x32_c, aom_d117_predictor_16x32_c,
                aom_d153_predictor_16x32_c, aom_d207e_predictor_16x32_c,
                aom_d63e_predictor_16x32_c, aom_paeth_predictor_16x32_c,
                aom_smooth_predictor_16x32_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_sse2, aom_dc_left_predictor_16x16_sse2,
                aom_dc_top_predictor_16x16_sse2,
                aom_dc_128_predictor_16x16_sse2, aom_v_predictor_16x16_sse2,
                aom_h_predictor_16x16_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred16, "intra16x8",
                aom_dc_predictor_16x8_sse2, aom_dc_left_predictor_16x8_sse2,
                aom_dc_top_predictor_16x8_sse2, aom_dc_128_predictor_16x8_sse2,
                aom_v_predictor_16x8_sse2, aom_h_predictor_16x8_sse2, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_3, TestIntraPred16, "intra16x32",
                aom_dc_predictor_16x32_sse2, aom_dc_left_predictor_16x32_sse2,
                aom_dc_top_predictor_16x32_sse2,
                aom_dc_128_predictor_16x32_sse2, aom_v_predictor_16x32_sse2,
                aom_h_predictor_16x32_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred16, "intra16x16", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, aom_d153_predictor_16x16_ssse3,
                NULL, NULL, aom_paeth_predictor_16x16_ssse3,
                aom_smooth_predictor_16x16_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred16, "intra16x8", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_16x8_ssse3, aom_smooth_predictor_16x8_ssse3,
                NULL, NULL)
INTRA_PRED_TEST(SSSE3_3, TestIntraPred16, "intra16x32", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_16x32_ssse3,
                aom_smooth_predictor_16x32_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2_1, TestIntraPred16, "intra16x16", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_16x16_avx2, NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_2, TestIntraPred16, "intra16x8", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_16x8_avx2, NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_3, TestIntraPred16, "intra16x32", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_16x32_avx2, NULL, NULL, NULL)
#endif  // HAVE_AVX2

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_dspr2, NULL, NULL, NULL, NULL,
                aom_h_predictor_16x16_dspr2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_neon, aom_dc_left_predictor_16x16_neon,
                aom_dc_top_predictor_16x16_neon,
                aom_dc_128_predictor_16x16_neon, aom_v_predictor_16x16_neon,
                aom_h_predictor_16x16_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred16, "intra16x16", aom_dc_predictor_16x16_msa,
                aom_dc_left_predictor_16x16_msa, aom_dc_top_predictor_16x16_msa,
                aom_dc_128_predictor_16x16_msa, aom_v_predictor_16x16_msa,
                aom_h_predictor_16x16_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 32x32

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_32x32_c
#define smooth_h_pred_func aom_smooth_h_predictor_32x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_1, TestIntraPred32, "intra32x32", aom_dc_predictor_32x32_c,
                aom_dc_left_predictor_32x32_c, aom_dc_top_predictor_32x32_c,
                aom_dc_128_predictor_32x32_c, aom_v_predictor_32x32_c,
                aom_h_predictor_32x32_c, aom_d45e_predictor_32x32_c,
                aom_d135_predictor_32x32_c, aom_d117_predictor_32x32_c,
                aom_d153_predictor_32x32_c, aom_d207e_predictor_32x32_c,
                aom_d63e_predictor_32x32_c, aom_paeth_predictor_32x32_c,
                aom_smooth_predictor_32x32_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_32x16_c
#define smooth_h_pred_func aom_smooth_h_predictor_32x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C_2, TestIntraPred32, "intra32x16", aom_dc_predictor_32x16_c,
                aom_dc_left_predictor_32x16_c, aom_dc_top_predictor_32x16_c,
                aom_dc_128_predictor_32x16_c, aom_v_predictor_32x16_c,
                aom_h_predictor_32x16_c, aom_d45e_predictor_32x16_c,
                aom_d135_predictor_32x16_c, aom_d117_predictor_32x16_c,
                aom_d153_predictor_32x16_c, aom_d207e_predictor_32x16_c,
                aom_d63e_predictor_32x16_c, aom_paeth_predictor_32x16_c,
                aom_smooth_predictor_32x16_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_sse2, aom_dc_left_predictor_32x32_sse2,
                aom_dc_top_predictor_32x32_sse2,
                aom_dc_128_predictor_32x32_sse2, aom_v_predictor_32x32_sse2,
                aom_h_predictor_32x32_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred32, "intra32x16",
                aom_dc_predictor_32x16_sse2, aom_dc_left_predictor_32x16_sse2,
                aom_dc_top_predictor_32x16_sse2,
                aom_dc_128_predictor_32x16_sse2, aom_v_predictor_32x16_sse2,
                aom_h_predictor_32x16_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred32, "intra32x32", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, aom_d153_predictor_32x32_ssse3,
                NULL, NULL, aom_paeth_predictor_32x32_ssse3,
                aom_smooth_predictor_32x32_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred32, "intra32x16", NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_32x16_ssse3,
                aom_smooth_predictor_32x16_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2_1, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_avx2, aom_dc_left_predictor_32x32_avx2,
                aom_dc_top_predictor_32x32_avx2,
                aom_dc_128_predictor_32x32_avx2, aom_v_predictor_32x32_avx2,
                aom_h_predictor_32x32_avx2, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_32x32_avx2, NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_2, TestIntraPred32, "intra32x16",
                aom_dc_predictor_32x16_avx2, aom_dc_left_predictor_32x16_avx2,
                aom_dc_top_predictor_32x16_avx2,
                aom_dc_128_predictor_32x16_avx2, aom_v_predictor_32x16_avx2,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                aom_paeth_predictor_32x16_avx2, NULL, NULL, NULL)
#endif  // HAVE_AVX2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_neon, aom_dc_left_predictor_32x32_neon,
                aom_dc_top_predictor_32x32_neon,
                aom_dc_128_predictor_32x32_neon, aom_v_predictor_32x32_neon,
                aom_h_predictor_32x32_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred32, "intra32x32", aom_dc_predictor_32x32_msa,
                aom_dc_left_predictor_32x32_msa, aom_dc_top_predictor_32x32_msa,
                aom_dc_128_predictor_32x32_msa, aom_v_predictor_32x32_msa,
                aom_h_predictor_32x32_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// High Bitdepth
#if CONFIG_HIGHBITDEPTH
namespace {

typedef void (*AvxHighbdPredFunc)(uint16_t *dst, ptrdiff_t y_stride,
                                  const uint16_t *above, const uint16_t *left,
                                  int bd);

typedef IntraPredTestMem<uint16_t> Av1HighbdIntraPredTestMem;

void TestHighbdIntraPred(const char name[], AvxHighbdPredFunc const *pred_funcs,
                         const char *const signatures[], int block_width,
                         int block_height) {
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1HighbdIntraPredTestMem intra_pred_test_mem;
  const uint16_t *const above = intra_pred_test_mem.above_mem + 16;
  const int bd = 12;

  intra_pred_test_mem.Init(block_width, bd);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, kBPS, above,
                    intra_pred_test_mem.left, bd);
    }
    libaom_test::ClearSystemState();
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(name, signatures, intra_pred_test_mem.src,
                      sizeof(intra_pred_test_mem.src), elapsed_time, k);
  }
}

void TestHighbdIntraPred4(const char *block_name,
                          AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures4x4[kNumAv1IntraFuncs] = {
    "11f74af6c5737df472f3275cbde062fa",
    "51bea056b6447c93f6eb8f6b7e8f6f71",
    "27e97f946766331795886f4de04c5594",
    "53ab15974b049111fb596c5168ec7e3f",
    "f0b640bb176fbe4584cf3d32a9b0320a",
    "729783ca909e03afd4b47111c80d967b",
    "d631a8544ccc87702db3e98fac494657",
    "293fc903254a33754133314c6cdba81f",
    "f8074d704233e73dfd35b458c6092374",
    "aa6363d08544a1ec4da33d7a0be5640d",
    "0bdc21a3acdebc393bc2c22e71bbeada",
    "a48f7a484ba4ad3916055c7160665b56",
    "6e30009c45474a22032678b1bd579c8f",
    "e57cba016d808aa8a35619df2a65f049",
#if CONFIG_SMOOTH_HV
    "55a6c37f39afcbbf5abca4a985b96459",
    "a623d45b37dafec1f8a75c4c5218913d",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures4x8[kNumAv1IntraFuncs] = {
    "22d519b796d59644043466320e4ccd14",
    "09513a738c49b3f9542d27f34abbe1d5",
    "807ae5e8813443ff01e71be6efacfb69",
    "cbfa18d0293430b6e9708b0be1fd2394",
    "346c354c34ec7fa780b576db355dab88",
    "f97dae85c35359632380b09ca98d611e",
    "aed1beef71de33856c814ff7d63dd9db",
    "49c47c04dd3d23d6fc5cc32bf9d40ae4",
    "a24aade6e22b323ee28c8bf08aa2d234",
    "aefef502f9e144e71cd27dc7383b3c28",
    "b284ae5277b85ebdd16b5952149f7458",
    "8dc5791167271f6f347582e07379f580",
    "698ae351d8896d89ed9e4e67b6e53eda",
    "dcc197034a9c45a3d8238bf085835f4e",
#if CONFIG_SMOOTH_HV
    "7a35e2c42ffdc2efc2d6d1d75a100fc7",
    "41ab6cebd4516c87a91b2a593e2c2506",
#endif
  };

  if (!strcmp(block_name, "Hbd Intra4x4")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures4x4, 4, 4);
  }
  if (!strcmp(block_name, "Hbd Intra4x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures4x8, 4, 8);
  }
}

void TestHighbdIntraPred8(const char *block_name,
                          AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures8x8[kNumAv1IntraFuncs] = {
    "03da8829fe94663047fd108c5fcaa71d",
    "ecdb37b8120a2d3a4c706b016bd1bfd7",
    "1d4543ed8d2b9368cb96898095fe8a75",
    "f791c9a67b913cbd82d9da8ecede30e2",
    "065c70646f4dbaff913282f55a45a441",
    "51f87123616662ef7c35691497dfd0ba",
    "4f53cf8e5f43894dc0759f43c7081f60",
    "9ffe186a6bc7db95275f1bbddd6f7aba",
    "a3258a2eae2e2bd55cb8f71351b22998",
    "8d909f0a2066e39b3216092c6289ece4",
    "6751f60655aba44aff78aaaf4e967377",
    "d31a449872fab968a8d41de578338780",
    "85c01ba03df68f9ece7bd3fa0f8980e6",
    "ad19b7dac092f56df6d054e1f67f21e7",
#if CONFIG_SMOOTH_HV
    "0edc415b5dd7299f7a34fb9f71d31d78",
    "2bc8ec19e9f4b77a64b8a0a1f6aec7e7",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures8x4[kNumAv1IntraFuncs] = {
    "d58cd4c4bf3b7bbaa5db5e1a5622ec78",
    "6e572c35aa782d00cafcb99e9ea047ea",
    "e8c22a3702b416dc9ab974505afbed09",
    "aaa4e4762a795aad7ad74de0c662c4e4",
    "a19f9101967383c3dcbd516dc317a291",
    "9ab8cb91f1a595b9ebe3fe8de58031aa",
    "c6c7d65264397d4d31e378e1f1cfd921",
    "5804158e463ff794b6b8a623f5d2c10d",
    "c342cdeb39aae4c4f7be10e057029298",
    "c1bbbcfe4b25f6b8eca6ad2f7ee793d3",
    "98d1dab8b949859b9c65298ee9f105f8",
    "396e803aaf6d7a03a231edc48b396051",
    "2cf9021d5f1169268699807ee118b65f",
    "ee9605fcbd6fb871f1c5cd81a6989327",
#if CONFIG_SMOOTH_HV
    "0edc415b5dd7299f7a34fb9f71d31d78",
    "2bc8ec19e9f4b77a64b8a0a1f6aec7e7",
#endif
  };
  static const char *const kSignatures8x16[kNumAv1IntraFuncs] = {
    "4562de1d0336610880fdd5685498a9ec",
    "16310fa7076394f16fc85c4b149d89c9",
    "0e94af88e1dc573b6f0f499cddd1f530",
    "dfd245ee20d091c67809160340365aa9",
    "d3562504327f70c096c5be23fd8a3747",
    "601b853558502acbb5135eadd2da117a",
    "e83f9a8bc16b507d2ed0b6b31a25d6f5",
    "fc8427d942246e8cba81247bb294afb5",
    "89cde712e4c1ef675ea156ad679c62c7",
    "0a68c2b28c3b171ad797cf76a7058f10",
    "e70724010e12d8f374cedd3910ceb0d5",
    "ad7987e91267503ba6fd3e8be42eb48c",
    "3c624345a723a1b2b1bea05a6a08bc99",
    "2a9c781de609e0184cc7ab442050f4e5",
#if CONFIG_SMOOTH_HV
    "0ddc5035c22252747126b61fc238c74d",
    "e43f5d83bab759af69c7b6773fc8f9b2",
#endif
  };
  if (!strcmp(block_name, "Hbd Intra8x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x8, 8, 8);
  }
  if (!strcmp(block_name, "Hbd Intra8x4")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x4, 8, 4);
  }
  if (!strcmp(block_name, "Hbd Intra8x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x16, 8, 16);
  }
}

void TestHighbdIntraPred16(const char *block_name,
                           AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures16x16[kNumAv1IntraFuncs] = {
    "e33cb3f56a878e2fddb1b2fc51cdd275",
    "c7bff6f04b6052c8ab335d726dbbd52d",
    "d0b0b47b654a9bcc5c6008110a44589b",
    "78f5da7b10b2b9ab39f114a33b6254e9",
    "c78e31d23831abb40d6271a318fdd6f3",
    "90d1347f4ec9198a0320daecb6ff90b8",
    "e38e12830e2ee5a01a064ec5998d5948",
    "cf28bd387b81ad3e5f1a1c779a4b70a0",
    "24c304330431ddeaf630f6ce94af2eac",
    "91a329798036bf64e8e00a87b131b8b1",
    "e536338d1a8ee192b9e591855db1a222",
    "54ecd47737f71c62d24e3779585113f2",
    "e63ded54ab3d0e8728b6f24d4f01e53f",
    "35ce21fbe0ea114c089fc3489a78155d",
#if CONFIG_SMOOTH_HV
    "f277f6ef8e4d717f1f0dfe2706ac197d",
    "e8014d3f41256976c02e0f1e622ba2b9",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures16x8[kNumAv1IntraFuncs] = {
    "a57d6b5a9bfd30c29591d8717ace9c51",
    "f5907ba97ee6c53e339e953fc8d845ee",
    "ea3aa727913ce45af06f89dd1808db5f",
    "408af4f23e48d14b48ee35ae094fcd18",
    "85c41cbcb5d744f7961e8950026fbffe",
    "8a4e588a837638887ba671f8d4910485",
    "caae3cc3d419bbd28aa389dbe4febee1",
    "ea67fb80d71b6471467c79662af1186c",
    "c83f7252412dd1ad2fc6af848e7f6be8",
    "f45af3d697f42f1b9b8def4e46bac78c",
    "dca4a2aaf5f63db387e264ba5963943a",
    "d01b1bcc50b4b66c1231142eae628cd3",
    "b792d8826b67a21757ea7097cff9e05b",
    "f94ce7101bb87fd3bb9312112527dbf4",
#if CONFIG_SMOOTH_HV
    "688c6660a6dc6fa61fa1aa38e708c209",
    "0cdf641b4f81d69509c92ae0b93ef5ff",
#endif
  };
  static const char *const kSignatures16x32[kNumAv1IntraFuncs] = {
    "aee4b3b0e3cc02d48e2c40d77f807927",
    "8baef2b2e789f79c8df9d90ad10f34a4",
    "038c38ee3c4f090bb8d736eab136aafc",
    "1a3de2aaeaffd68a9fd6c7f6557b83f3",
    "385c6e0ea29421dd81011a2934641e26",
    "6cf96c285d1a2d4787f955dad715b08c",
    "21f82421fda1c3afca8baca0dc048a52",
    "eac3734852c99a051f6d15a921d9e7b9",
    "c81f7ffec79508bf78d0f2c67d8abe96",
    "14b8c62304f65a06653b9b35dfe12d97",
    "e0893310042511275ae04e5186ee5326",
    "b4f05903a6191093be719794417ac6fd",
    "2d7f75dcd73b9528c8396279ff09ff3a",
    "5a63cd1841e4ed470e4ca5ef845f2281",
#if CONFIG_SMOOTH_HV
    "610d899ca945fbead33287d4335a8b32",
    "6bafaad81fce37be46730187e78d8b11",
#endif
  };
  if (!strcmp(block_name, "Hbd Intra16x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x16, 16, 16);
  }
  if (!strcmp(block_name, "Hbd Intra16x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x8, 16, 8);
  }
  if (!strcmp(block_name, "Hbd Intra16x32")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x32, 16, 32);
  }
}

void TestHighbdIntraPred32(const char *block_name,
                           AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures32x32[kNumAv1IntraFuncs] = {
    "a3e8056ba7e36628cce4917cd956fedd",
    "cc7d3024fe8748b512407edee045377e",
    "2aab0a0f330a1d3e19b8ecb8f06387a3",
    "a547bc3fb7b06910bf3973122a426661",
    "26f712514da95042f93d6e8dc8e431dc",
    "bb08c6e16177081daa3d936538dbc2e3",
    "4e10f10b082a5b4265080c102d34eb47",
    "42867c8553285e94ee8e4df7abafbda8",
    "6496bdee96100667833f546e1be3d640",
    "2ebfa25bf981377e682e580208504300",
    "1788695b10a6f82ae1a56686dcbcd0a9",
    "c3b9c506604a7132bbb5f4e97bdb03f0",
    "84bf83f94a51b33654ca940c6f8bc057",
    "7168b03fc31bf29596a344d6a35d007c",
#if CONFIG_SMOOTH_HV
    "b073a70d3672f1282236994f5d12e94b",
    "c51607aebad5dcb3c1e3b58ef9e5b84e",
#endif  // CONFIG_SMOOTH_HV
  };
  static const char *const kSignatures32x16[kNumAv1IntraFuncs] = {
    "290b23c9f5a1de7905bfa71a942da29b",
    "701e7b82593c66da5052fc4b6afd79ce",
    "4da828c5455cd246735a663fbb204989",
    "e3fbeaf234efece8dbd752b77226200c",
    "4d1d8c969f05155a7e7e84cf7aad021b",
    "c22e4877c2c946d5bdc0d542e29e70cf",
    "ffd86b234d65c2e1386a5b5b5c188a69",
    "50aaaa7d90e300b635ab18cdd73e189b",
    "a945dc7429df168e2169d81b58a15859",
    "66725070d7fad02dee78730ba0843e19",
    "33d873cb05d45df2af4ff59033833db7",
    "0dd783695b69271f65d56f5516fa6dc0",
    "8ac1ce815e7780500f842b0beb0bb980",
    "9fee2e2502b507f25bfad30a55b0b610",
#if CONFIG_SMOOTH_HV
    "4ced9c212ec6f9956e27f68a91b59fef",
    "4a7a0b93f138bb0863e4e465b01ec0b1",
#endif
  };
  if (!strcmp(block_name, "Hbd Intra32x32")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures32x32, 32, 32);
  }
  if (!strcmp(block_name, "Hbd Intra32x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures32x16, 32, 16);
  }
}

}  // namespace

#define HIGHBD_INTRA_PRED_TEST(arch, test_func, block_size, dc, dc_left,     \
                               dc_top, dc_128, v, h, d45e, d135, d117, d153, \
                               d207e, d63e, tm, smooth, smooth_v, smooth_h)  \
  TEST(arch, DISABLED_##test_func) {                                         \
    static const AvxHighbdPredFunc aom_intra_pred[] = {                      \
      dc,   dc_left, dc_top, dc_128, v,  h,      d45e,     d135,             \
      d117, d153,    d207e,  d63e,   tm, smooth, smooth_v, smooth_h          \
    };                                                                       \
    test_func(block_size, aom_intra_pred);                                   \
  }

// -----------------------------------------------------------------------------
// 4x4

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_4x4_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_4x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred4, "Hbd Intra4x4", aom_highbd_dc_predictor_4x4_c,
    aom_highbd_dc_left_predictor_4x4_c, aom_highbd_dc_top_predictor_4x4_c,
    aom_highbd_dc_128_predictor_4x4_c, aom_highbd_v_predictor_4x4_c,
    aom_highbd_h_predictor_4x4_c, aom_highbd_d45e_predictor_4x4_c,
    aom_highbd_d135_predictor_4x4_c, aom_highbd_d117_predictor_4x4_c,
    aom_highbd_d153_predictor_4x4_c, aom_highbd_d207e_predictor_4x4_c,
    aom_highbd_d63e_predictor_4x4_c, aom_highbd_paeth_predictor_4x4_c,
    aom_highbd_smooth_predictor_4x4_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(
    SSE2_1, TestHighbdIntraPred4, "Hbd Intra4x4",
    aom_highbd_dc_predictor_4x4_sse2, aom_highbd_dc_left_predictor_4x4_sse2,
    aom_highbd_dc_top_predictor_4x4_sse2, aom_highbd_dc_128_predictor_4x4_sse2,
    aom_highbd_v_predictor_4x4_sse2, aom_highbd_h_predictor_4x4_sse2,
    aom_highbd_d45e_predictor_4x4_sse2, aom_highbd_d135_predictor_4x4_sse2,
    aom_highbd_d117_predictor_4x4_sse2, aom_highbd_d153_predictor_4x4_sse2,
    NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred4, "Hbd Intra4x8",
                       aom_highbd_dc_predictor_4x8_sse2,
                       aom_highbd_dc_left_predictor_4x8_sse2,
                       aom_highbd_dc_top_predictor_4x8_sse2,
                       aom_highbd_dc_128_predictor_4x8_sse2,
                       aom_highbd_v_predictor_4x8_sse2,
                       aom_highbd_h_predictor_4x8_sse2,
                       aom_highbd_d45e_predictor_4x8_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_4x8_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_4x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred4, "Hbd Intra4x8", aom_highbd_dc_predictor_4x8_c,
    aom_highbd_dc_left_predictor_4x8_c, aom_highbd_dc_top_predictor_4x8_c,
    aom_highbd_dc_128_predictor_4x8_c, aom_highbd_v_predictor_4x8_c,
    aom_highbd_h_predictor_4x8_c, aom_highbd_d45e_predictor_4x8_c,
    aom_highbd_d135_predictor_4x8_c, aom_highbd_d117_predictor_4x8_c,
    aom_highbd_d153_predictor_4x8_c, aom_highbd_d207e_predictor_4x8_c,
    aom_highbd_d63e_predictor_4x8_c, aom_highbd_paeth_predictor_4x8_c,
    aom_highbd_smooth_predictor_4x8_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

// -----------------------------------------------------------------------------
// 8x8

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_8x8_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_8x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred8, "Hbd Intra8x8", aom_highbd_dc_predictor_8x8_c,
    aom_highbd_dc_left_predictor_8x8_c, aom_highbd_dc_top_predictor_8x8_c,
    aom_highbd_dc_128_predictor_8x8_c, aom_highbd_v_predictor_8x8_c,
    aom_highbd_h_predictor_8x8_c, aom_highbd_d45e_predictor_8x8_c,
    aom_highbd_d135_predictor_8x8_c, aom_highbd_d117_predictor_8x8_c,
    aom_highbd_d153_predictor_8x8_c, aom_highbd_d207e_predictor_8x8_c,
    aom_highbd_d63e_predictor_8x8_c, aom_highbd_paeth_predictor_8x8_c,
    aom_highbd_smooth_predictor_8x8_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred8, "Hbd Intra8x8",
                       aom_highbd_dc_predictor_8x8_sse2,
                       aom_highbd_dc_left_predictor_8x8_sse2,
                       aom_highbd_dc_top_predictor_8x8_sse2,
                       aom_highbd_dc_128_predictor_8x8_sse2,
                       aom_highbd_v_predictor_8x8_sse2,
                       aom_highbd_h_predictor_8x8_sse2,
                       aom_highbd_d45e_predictor_8x8_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred8, "Hbd Intra8x4",
                       aom_highbd_dc_predictor_8x4_sse2,
                       aom_highbd_dc_left_predictor_8x4_sse2,
                       aom_highbd_dc_top_predictor_8x4_sse2,
                       aom_highbd_dc_128_predictor_8x4_sse2,
                       aom_highbd_v_predictor_8x4_sse2,
                       aom_highbd_h_predictor_8x4_sse2,
                       aom_highbd_d45e_predictor_8x4_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_3, TestHighbdIntraPred8, "Hbd Intra8x16",
                       aom_highbd_dc_predictor_8x16_sse2,
                       aom_highbd_dc_left_predictor_8x16_sse2,
                       aom_highbd_dc_top_predictor_8x16_sse2,
                       aom_highbd_dc_128_predictor_8x16_sse2,
                       aom_highbd_v_predictor_8x16_sse2,
                       aom_highbd_h_predictor_8x16_sse2,
                       aom_highbd_d45e_predictor_8x16_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3, TestHighbdIntraPred8, "Hbd Intra8x8", NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d135_predictor_8x8_ssse3,
                       aom_highbd_d117_predictor_8x8_ssse3,
                       aom_highbd_d153_predictor_8x8_ssse3, NULL, NULL, NULL,
                       NULL, NULL, NULL)
#endif

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_8x4_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_8x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred8, "Hbd Intra8x4", aom_highbd_dc_predictor_8x4_c,
    aom_highbd_dc_left_predictor_8x4_c, aom_highbd_dc_top_predictor_8x4_c,
    aom_highbd_dc_128_predictor_8x4_c, aom_highbd_v_predictor_8x4_c,
    aom_highbd_h_predictor_8x4_c, aom_highbd_d45e_predictor_8x4_c,
    aom_highbd_d135_predictor_8x4_c, aom_highbd_d117_predictor_8x4_c,
    aom_highbd_d153_predictor_8x4_c, aom_highbd_d207e_predictor_8x4_c,
    aom_highbd_d63e_predictor_8x4_c, aom_highbd_paeth_predictor_8x4_c,
    aom_highbd_smooth_predictor_8x4_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_8x16_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_8x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_3, TestHighbdIntraPred8, "Hbd Intra8x16", aom_highbd_dc_predictor_8x16_c,
    aom_highbd_dc_left_predictor_8x16_c, aom_highbd_dc_top_predictor_8x16_c,
    aom_highbd_dc_128_predictor_8x16_c, aom_highbd_v_predictor_8x16_c,
    aom_highbd_h_predictor_8x16_c, aom_highbd_d45e_predictor_8x16_c,
    aom_highbd_d135_predictor_8x16_c, aom_highbd_d117_predictor_8x16_c,
    aom_highbd_d153_predictor_8x16_c, aom_highbd_d207e_predictor_8x16_c,
    aom_highbd_d63e_predictor_8x16_c, aom_highbd_paeth_predictor_8x16_c,
    aom_highbd_smooth_predictor_8x16_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

// -----------------------------------------------------------------------------
// 16x16

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_16x16_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_16x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred16, "Hbd Intra16x16",
    aom_highbd_dc_predictor_16x16_c, aom_highbd_dc_left_predictor_16x16_c,
    aom_highbd_dc_top_predictor_16x16_c, aom_highbd_dc_128_predictor_16x16_c,
    aom_highbd_v_predictor_16x16_c, aom_highbd_h_predictor_16x16_c,
    aom_highbd_d45e_predictor_16x16_c, aom_highbd_d135_predictor_16x16_c,
    aom_highbd_d117_predictor_16x16_c, aom_highbd_d153_predictor_16x16_c,
    aom_highbd_d207e_predictor_16x16_c, aom_highbd_d63e_predictor_16x16_c,
    aom_highbd_paeth_predictor_16x16_c, aom_highbd_smooth_predictor_16x16_c,
    smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred16, "Hbd Intra16x16",
                       aom_highbd_dc_predictor_16x16_sse2,
                       aom_highbd_dc_left_predictor_16x16_sse2,
                       aom_highbd_dc_top_predictor_16x16_sse2,
                       aom_highbd_dc_128_predictor_16x16_sse2,
                       aom_highbd_v_predictor_16x16_sse2,
                       aom_highbd_h_predictor_16x16_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred16, "Hbd Intra16x8",
                       aom_highbd_dc_predictor_16x8_sse2,
                       aom_highbd_dc_left_predictor_16x8_sse2,
                       aom_highbd_dc_top_predictor_16x8_sse2,
                       aom_highbd_dc_128_predictor_16x8_sse2,
                       aom_highbd_v_predictor_16x8_sse2,
                       aom_highbd_h_predictor_16x8_sse2, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_3, TestHighbdIntraPred16, "Hbd Intra16x32",
                       aom_highbd_dc_predictor_16x32_sse2,
                       aom_highbd_dc_left_predictor_16x32_sse2,
                       aom_highbd_dc_top_predictor_16x32_sse2,
                       aom_highbd_dc_128_predictor_16x32_sse2,
                       aom_highbd_v_predictor_16x32_sse2,
                       aom_highbd_h_predictor_16x32_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3_1, TestHighbdIntraPred16, "Hbd Intra16x16", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d135_predictor_16x16_ssse3,
                       aom_highbd_d117_predictor_16x16_ssse3,
                       aom_highbd_d153_predictor_16x16_ssse3, NULL, NULL, NULL,
                       NULL, NULL, NULL)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2_1, TestHighbdIntraPred16, "Hbd Intra16x16", NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d45e_predictor_16x16_avx2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_2, TestHighbdIntraPred16, "Hbd Intra16x8", NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d45e_predictor_16x8_avx2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_3, TestHighbdIntraPred16, "Hbd Intra16x32", NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d45e_predictor_16x32_avx2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_16x8_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_16x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred16, "Hbd Intra16x8", aom_highbd_dc_predictor_16x8_c,
    aom_highbd_dc_left_predictor_16x8_c, aom_highbd_dc_top_predictor_16x8_c,
    aom_highbd_dc_128_predictor_16x8_c, aom_highbd_v_predictor_16x8_c,
    aom_highbd_h_predictor_16x8_c, aom_highbd_d45e_predictor_16x8_c,
    aom_highbd_d135_predictor_16x8_c, aom_highbd_d117_predictor_16x8_c,
    aom_highbd_d153_predictor_16x8_c, aom_highbd_d207e_predictor_16x8_c,
    aom_highbd_d63e_predictor_16x8_c, aom_highbd_paeth_predictor_16x8_c,
    aom_highbd_smooth_predictor_16x8_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_16x32_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_16x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_3, TestHighbdIntraPred16, "Hbd Intra16x32",
    aom_highbd_dc_predictor_16x32_c, aom_highbd_dc_left_predictor_16x32_c,
    aom_highbd_dc_top_predictor_16x32_c, aom_highbd_dc_128_predictor_16x32_c,
    aom_highbd_v_predictor_16x32_c, aom_highbd_h_predictor_16x32_c,
    aom_highbd_d45e_predictor_16x32_c, aom_highbd_d135_predictor_16x32_c,
    aom_highbd_d117_predictor_16x32_c, aom_highbd_d153_predictor_16x32_c,
    aom_highbd_d207e_predictor_16x32_c, aom_highbd_d63e_predictor_16x32_c,
    aom_highbd_paeth_predictor_16x32_c, aom_highbd_smooth_predictor_16x32_c,
    smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

// -----------------------------------------------------------------------------
// 32x32

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_32x32_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_32x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred32, "Hbd Intra32x32",
    aom_highbd_dc_predictor_32x32_c, aom_highbd_dc_left_predictor_32x32_c,
    aom_highbd_dc_top_predictor_32x32_c, aom_highbd_dc_128_predictor_32x32_c,
    aom_highbd_v_predictor_32x32_c, aom_highbd_h_predictor_32x32_c,
    aom_highbd_d45e_predictor_32x32_c, aom_highbd_d135_predictor_32x32_c,
    aom_highbd_d117_predictor_32x32_c, aom_highbd_d153_predictor_32x32_c,
    aom_highbd_d207e_predictor_32x32_c, aom_highbd_d63e_predictor_32x32_c,
    aom_highbd_paeth_predictor_32x32_c, aom_highbd_smooth_predictor_32x32_c,
    smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred32, "Hbd Intra32x32",
                       aom_highbd_dc_predictor_32x32_sse2,
                       aom_highbd_dc_left_predictor_32x32_sse2,
                       aom_highbd_dc_top_predictor_32x32_sse2,
                       aom_highbd_dc_128_predictor_32x32_sse2,
                       aom_highbd_v_predictor_32x32_sse2,
                       aom_highbd_h_predictor_32x32_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred32, "Hbd Intra32x16",
                       aom_highbd_dc_predictor_32x16_sse2,
                       aom_highbd_dc_left_predictor_32x16_sse2,
                       aom_highbd_dc_top_predictor_32x16_sse2,
                       aom_highbd_dc_128_predictor_32x16_sse2,
                       aom_highbd_v_predictor_32x16_sse2,
                       aom_highbd_h_predictor_32x16_sse2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3_1, TestHighbdIntraPred32, "Hbd Intra32x32", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d135_predictor_32x32_ssse3,
                       aom_highbd_d117_predictor_32x32_ssse3,
                       aom_highbd_d153_predictor_32x32_ssse3, NULL, NULL, NULL,
                       NULL, NULL, NULL)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2_1, TestHighbdIntraPred32, "Hbd Intra32x32", NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d45e_predictor_32x32_avx2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_2, TestHighbdIntraPred32, "Hbd Intra32x16", NULL,
                       NULL, NULL, NULL, NULL, NULL,
                       aom_highbd_d45e_predictor_32x16_avx2, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_32x16_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_32x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred32, "Hbd Intra32x16",
    aom_highbd_dc_predictor_32x16_c, aom_highbd_dc_left_predictor_32x16_c,
    aom_highbd_dc_top_predictor_32x16_c, aom_highbd_dc_128_predictor_32x16_c,
    aom_highbd_v_predictor_32x16_c, aom_highbd_h_predictor_32x16_c,
    aom_highbd_d45e_predictor_32x16_c, aom_highbd_d135_predictor_32x16_c,
    aom_highbd_d117_predictor_32x16_c, aom_highbd_d153_predictor_32x16_c,
    aom_highbd_d207e_predictor_32x16_c, aom_highbd_d63e_predictor_32x16_c,
    aom_highbd_paeth_predictor_32x16_c, aom_highbd_smooth_predictor_32x16_c,
    smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func
#endif  // CONFIG_HIGHBITDEPTH

#include "test/test_libaom.cc"
