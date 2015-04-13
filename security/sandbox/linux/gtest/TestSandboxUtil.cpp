/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "SandboxUtil.h"

#include <pthread.h>

namespace mozilla {

// In order to test IsSingleThreaded when the test-running process is
// single-threaded, before assorted XPCOM components have created many
// additional threads, a static initializer is used.

namespace {

struct EarlyTest {
  bool mWasSingleThreaded;

  EarlyTest()
  : mWasSingleThreaded(IsSingleThreaded())
  { }
};

static const EarlyTest gEarlyTest;

} // namespace

TEST(SandboxUtil, IsSingleThreaded)
{
  EXPECT_TRUE(gEarlyTest.mWasSingleThreaded);
  EXPECT_FALSE(IsSingleThreaded());
}

} // namespace mozilla
