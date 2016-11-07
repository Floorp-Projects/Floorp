/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslimpl.h"

#include "gtest/gtest.h"

namespace nss_test {

#ifdef UNSAFE_FUZZER_MODE

class TlsFuzzTest : public ::testing::Test {};

// Ensure that ssl_Time() returns a constant value.
TEST_F(TlsFuzzTest, Fuzz_SSL_Time_Constant) {
  PRInt32 now = ssl_Time();
  PR_Sleep(PR_SecondsToInterval(2));
  EXPECT_EQ(ssl_Time(), now);
}

#endif
}
