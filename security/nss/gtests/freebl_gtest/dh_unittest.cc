// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blapi.h"
#include "gtest/gtest.h"

namespace nss_test {

class DHTest : public ::testing::Test {
 protected:
  void TestGenParamSuccess(int size) {
    DHParams *params;
    for (int i = 0; i < 10; i++) {
      EXPECT_EQ(SECSuccess, DH_GenParam(size, &params));
      PORT_FreeArena(params->arena, PR_TRUE);
    }
  }
};

// Test parameter generation for minimum and some common key sizes
TEST_F(DHTest, DhGenParamSuccessTest16) { TestGenParamSuccess(16); }
TEST_F(DHTest, DhGenParamSuccessTest224) { TestGenParamSuccess(224); }
TEST_F(DHTest, DhGenParamSuccessTest256) { TestGenParamSuccess(256); }

}  // namespace nss_test
