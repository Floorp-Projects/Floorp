/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

extern "C" void Rust_Future(bool* aItWorked);

TEST(RustMozTask, Future)
{
  bool itWorked = false;
  Rust_Future(&itWorked);
  EXPECT_TRUE(itWorked);
}
