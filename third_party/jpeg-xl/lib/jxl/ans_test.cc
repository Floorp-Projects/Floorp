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

#include <stddef.h>
#include <stdint.h>

#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/ans_params.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {
namespace {

void RoundtripTestcase(int n_histograms, int alphabet_size,
                       const std::vector<Token>& input_values) {
  constexpr uint16_t kMagic1 = 0x9e33;
  constexpr uint16_t kMagic2 = 0x8b04;

  BitWriter writer;
  // Space for magic bytes.
  BitWriter::Allotment allotment_magic1(&writer, 16);
  writer.Write(16, kMagic1);
  ReclaimAndCharge(&writer, &allotment_magic1, 0, nullptr);

  std::vector<uint8_t> context_map;
  EntropyEncodingData codes;
  std::vector<std::vector<Token>> input_values_vec;
  input_values_vec.push_back(input_values);

  BuildAndEncodeHistograms(HistogramParams(), n_histograms, input_values_vec,
                           &codes, &context_map, &writer, 0, nullptr);
  WriteTokens(input_values_vec[0], codes, context_map, &writer, 0, nullptr);

  // Magic bytes + padding
  BitWriter::Allotment allotment_magic2(&writer, 24);
  writer.Write(16, kMagic2);
  writer.ZeroPadToByte();
  ReclaimAndCharge(&writer, &allotment_magic2, 0, nullptr);

  // We do not truncate the output. Reading past the end reads out zeroes
  // anyway.
  BitReader br(writer.GetSpan());

  ASSERT_EQ(br.ReadBits(16), kMagic1);

  std::vector<uint8_t> dec_context_map;
  ANSCode decoded_codes;
  ASSERT_TRUE(
      DecodeHistograms(&br, n_histograms, &decoded_codes, &dec_context_map));
  ASSERT_EQ(dec_context_map, context_map);
  ANSSymbolReader reader(&decoded_codes, &br);

  for (const Token& symbol : input_values) {
    uint32_t read_symbol =
        reader.ReadHybridUint(symbol.context, &br, dec_context_map);
    ASSERT_EQ(read_symbol, symbol.value);
  }
  ASSERT_TRUE(reader.CheckANSFinalState());

  ASSERT_EQ(br.ReadBits(16), kMagic2);
  EXPECT_TRUE(br.Close());
}

TEST(ANSTest, EmptyRoundtrip) {
  RoundtripTestcase(2, ANS_MAX_ALPHABET_SIZE, std::vector<Token>());
}

TEST(ANSTest, SingleSymbolRoundtrip) {
  for (uint32_t i = 0; i < ANS_MAX_ALPHABET_SIZE; i++) {
    RoundtripTestcase(2, ANS_MAX_ALPHABET_SIZE, {{0, i}});
  }
  for (uint32_t i = 0; i < ANS_MAX_ALPHABET_SIZE; i++) {
    RoundtripTestcase(2, ANS_MAX_ALPHABET_SIZE,
                      std::vector<Token>(1024, {0, i}));
  }
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
constexpr size_t kReps = 10;
#else
constexpr size_t kReps = 100;
#endif

void RoundtripRandomStream(int alphabet_size, size_t reps = kReps,
                           size_t num = 1 << 18) {
  constexpr int kNumHistograms = 3;
  std::mt19937_64 rng;
  for (size_t i = 0; i < reps; i++) {
    std::vector<Token> symbols;
    for (size_t j = 0; j < num; j++) {
      int context = std::uniform_int_distribution<>(0, kNumHistograms - 1)(rng);
      int value = std::uniform_int_distribution<>(0, alphabet_size - 1)(rng);
      symbols.emplace_back(context, value);
    }
    RoundtripTestcase(kNumHistograms, alphabet_size, symbols);
  }
}

void RoundtripRandomUnbalancedStream(int alphabet_size) {
  constexpr int kNumHistograms = 3;
  constexpr int kPrecision = 1 << 10;
  std::mt19937_64 rng;
  for (int i = 0; i < 100; i++) {
    std::vector<int> distributions[kNumHistograms];
    for (int j = 0; j < kNumHistograms; j++) {
      distributions[j].resize(kPrecision);
      int symbol = 0;
      int remaining = 1;
      for (int k = 0; k < kPrecision; k++) {
        if (remaining == 0) {
          if (symbol < alphabet_size - 1) symbol++;
          // There is no meaning behind this distribution: it's anything that
          // will create a nonuniform distribution and won't have too few
          // symbols usually. Also we want different distributions we get to be
          // sufficiently dissimilar.
          remaining =
              std::uniform_int_distribution<>(0, (kPrecision - k) / 1)(rng);
        }
        distributions[j][k] = symbol;
        remaining--;
      }
    }
    std::vector<Token> symbols;
    for (int j = 0; j < 1 << 18; j++) {
      int context = std::uniform_int_distribution<>(0, kNumHistograms - 1)(rng);
      int value = distributions[context][std::uniform_int_distribution<>(
          0, kPrecision - 1)(rng)];
      symbols.emplace_back(context, value);
    }
    RoundtripTestcase(kNumHistograms + 1, alphabet_size, symbols);
  }
}

TEST(ANSTest, RandomStreamRoundtrip3Small) { RoundtripRandomStream(3, 1, 16); }

TEST(ANSTest, RandomStreamRoundtrip3) { RoundtripRandomStream(3); }

TEST(ANSTest, RandomStreamRoundtripBig) {
  RoundtripRandomStream(ANS_MAX_ALPHABET_SIZE);
}

TEST(ANSTest, RandomUnbalancedStreamRoundtrip3) {
  RoundtripRandomUnbalancedStream(3);
}

TEST(ANSTest, RandomUnbalancedStreamRoundtripBig) {
  RoundtripRandomUnbalancedStream(ANS_MAX_ALPHABET_SIZE);
}

TEST(ANSTest, UintConfigRoundtrip) {
  for (size_t log_alpha_size = 5; log_alpha_size <= 8; log_alpha_size++) {
    std::vector<HybridUintConfig> uint_config, uint_config_dec;
    for (size_t i = 0; i < log_alpha_size; i++) {
      for (size_t j = 0; j <= i; j++) {
        for (size_t k = 0; k <= i - j; k++) {
          uint_config.emplace_back(i, j, k);
        }
      }
    }
    uint_config.emplace_back(log_alpha_size, 0, 0);
    uint_config_dec.resize(uint_config.size());
    BitWriter writer;
    BitWriter::Allotment allotment(&writer, 10 * uint_config.size());
    EncodeUintConfigs(uint_config, &writer, log_alpha_size);
    ReclaimAndCharge(&writer, &allotment, 0, nullptr);
    writer.ZeroPadToByte();
    BitReader br(writer.GetSpan());
    EXPECT_TRUE(DecodeUintConfigs(log_alpha_size, &uint_config_dec, &br));
    EXPECT_TRUE(br.Close());
    for (size_t i = 0; i < uint_config.size(); i++) {
      EXPECT_EQ(uint_config[i].split_token, uint_config_dec[i].split_token);
      EXPECT_EQ(uint_config[i].msb_in_token, uint_config_dec[i].msb_in_token);
      EXPECT_EQ(uint_config[i].lsb_in_token, uint_config_dec[i].lsb_in_token);
    }
  }
}

}  // namespace
}  // namespace jxl
