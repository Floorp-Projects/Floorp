/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "secoid.h"

#include <cstdlib>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (SECOID_Init() != SECSuccess) {
    return 1;
  }
  int rv = RUN_ALL_TESTS();

  if (SECOID_Shutdown() != SECSuccess) {
    return 1;
  }

  return rv;
}
