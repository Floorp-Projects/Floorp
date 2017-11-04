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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/acm_random.h"
#include "aom/aom_integer.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitwriter.h"

using libaom_test::ACMRandom;

namespace {
const int num_tests = 10;
}  // namespace

TEST(AV1, TestBitIO) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int n = 0; n < num_tests; ++n) {
    for (int method = 0; method <= 7; ++method) {  // we generate various proba
      const int kBitsToTest = 1000;
      uint8_t probas[kBitsToTest];

      for (int i = 0; i < kBitsToTest; ++i) {
        const int parity = i & 1;
        /* clang-format off */
        probas[i] =
          (method == 0) ? 0 : (method == 1) ? 255 :
          (method == 2) ? 128 :
          (method == 3) ? rnd.Rand8() :
          (method == 4) ? (parity ? 0 : 255) :
            // alternate between low and high proba:
            (method == 5) ? (parity ? rnd(128) : 255 - rnd(128)) :
            (method == 6) ?
            (parity ? rnd(64) : 255 - rnd(64)) :
            (parity ? rnd(32) : 255 - rnd(32));
        /* clang-format on */
      }
      for (int bit_method = 0; bit_method <= 3; ++bit_method) {
        const int random_seed = 6432;
        const int kBufferSize = 10000;
        ACMRandom bit_rnd(random_seed);
        aom_writer bw;
        uint8_t bw_buffer[kBufferSize];
        aom_start_encode(&bw, bw_buffer);

        int bit = (bit_method == 0) ? 0 : (bit_method == 1) ? 1 : 0;
        for (int i = 0; i < kBitsToTest; ++i) {
          if (bit_method == 2) {
            bit = (i & 1);
          } else if (bit_method == 3) {
            bit = bit_rnd(2);
          }
          aom_write(&bw, bit, static_cast<int>(probas[i]));
        }

        aom_stop_encode(&bw);

        aom_reader br;
        aom_reader_init(&br, bw_buffer, bw.pos, NULL, NULL);
        bit_rnd.Reset(random_seed);
        for (int i = 0; i < kBitsToTest; ++i) {
          if (bit_method == 2) {
            bit = (i & 1);
          } else if (bit_method == 3) {
            bit = bit_rnd(2);
          }
          GTEST_ASSERT_EQ(aom_read(&br, probas[i], NULL), bit)
              << "pos: " << i << " / " << kBitsToTest
              << " bit_method: " << bit_method << " method: " << method;
        }
      }
    }
  }
}

#define FRAC_DIFF_TOTAL_ERROR 0.16

TEST(AV1, TestTell) {
  const int kBufferSize = 10000;
  aom_writer bw;
  uint8_t bw_buffer[kBufferSize];
  const int kSymbols = 1024;
  // Coders are noisier at low probabilities, so we start at p = 4.
  for (int p = 4; p < 256; p++) {
    double probability = p / 256.;
    aom_start_encode(&bw, bw_buffer);
    for (int i = 0; i < kSymbols; i++) {
      aom_write(&bw, 0, p);
    }
    aom_stop_encode(&bw);
    aom_reader br;
    aom_reader_init(&br, bw_buffer, bw.pos, NULL, NULL);
    uint32_t last_tell = aom_reader_tell(&br);
    uint32_t last_tell_frac = aom_reader_tell_frac(&br);
    double frac_diff_total = 0;
    GTEST_ASSERT_GE(aom_reader_tell(&br), 0u);
    GTEST_ASSERT_LE(aom_reader_tell(&br), 1u);
    for (int i = 0; i < kSymbols; i++) {
      aom_read(&br, p, NULL);
      uint32_t tell = aom_reader_tell(&br);
      uint32_t tell_frac = aom_reader_tell_frac(&br);
      GTEST_ASSERT_GE(tell, last_tell)
          << "tell: " << tell << ", last_tell: " << last_tell;
      GTEST_ASSERT_GE(tell_frac, last_tell_frac)
          << "tell_frac: " << tell_frac
          << ", last_tell_frac: " << last_tell_frac;
      // Frac tell should round up to tell.
      GTEST_ASSERT_EQ(tell, (tell_frac + 7) >> 3);
      last_tell = tell;
      frac_diff_total +=
          fabs(((tell_frac - last_tell_frac) / 8.0) + log2(probability));
      last_tell_frac = tell_frac;
    }
    const uint32_t expected = (uint32_t)(-kSymbols * log2(probability));
    // Last tell should be close to the expected value.
    GTEST_ASSERT_LE(last_tell, expected + 20) << " last_tell: " << last_tell;
    // The average frac_diff error should be pretty small.
    GTEST_ASSERT_LE(frac_diff_total / kSymbols, FRAC_DIFF_TOTAL_ERROR)
        << " frac_diff_total: " << frac_diff_total;
  }
}
