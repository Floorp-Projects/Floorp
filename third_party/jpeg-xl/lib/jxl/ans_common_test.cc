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

#include "lib/jxl/ans_common.h"

#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/ans_params.h"

namespace jxl {
namespace {

void VerifyAliasDistribution(const std::vector<int>& distribution,
                             uint32_t range) {
  constexpr size_t log_alpha_size = 8;
  AliasTable::Entry table[1 << log_alpha_size];
  InitAliasTable(distribution, range, log_alpha_size, table);
  std::vector<std::vector<uint32_t>> offsets(distribution.size());
  for (uint32_t i = 0; i < range; i++) {
    AliasTable::Symbol s = AliasTable::Lookup(
        table, i, ANS_LOG_TAB_SIZE - 8, (1 << (ANS_LOG_TAB_SIZE - 8)) - 1);
    offsets[s.value].push_back(s.offset);
  }
  for (uint32_t i = 0; i < distribution.size(); i++) {
    ASSERT_EQ(distribution[i], offsets[i].size());
    std::sort(offsets[i].begin(), offsets[i].end());
    for (uint32_t j = 0; j < offsets[i].size(); j++) {
      ASSERT_EQ(offsets[i][j], j);
    }
  }
}

TEST(ANSCommonTest, AliasDistributionSmoke) {
  VerifyAliasDistribution({ANS_TAB_SIZE / 2, ANS_TAB_SIZE / 2}, ANS_TAB_SIZE);
  VerifyAliasDistribution({ANS_TAB_SIZE}, ANS_TAB_SIZE);
  VerifyAliasDistribution({0, 0, 0, ANS_TAB_SIZE, 0}, ANS_TAB_SIZE);
}

}  // namespace
}  // namespace jxl
