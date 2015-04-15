/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Atomics.h"
#include "LulMain.h"
#include "GeckoProfiler.h"       // for TracingMetadata
#include "platform-linux-lul.h"  // for read_procmaps

// Set this to 0 to make LUL be completely silent during tests.
// Set it to 1 to get logging output from LUL, presumably for
// the purpose of debugging it.
#define DEBUG_LUL_TEST 0

// LUL needs a callback for its logging sink.
static void
gtest_logging_sink_for_LUL(const char* str) {
  if (DEBUG_LUL_TEST == 0) {
    return;
  }
  // Ignore any trailing \n, since LOG will add one anyway.
  size_t n = strlen(str);
  if (n > 0 && str[n-1] == '\n') {
    char* tmp = strdup(str);
    tmp[n-1] = 0;
    fprintf(stderr, "LUL-in-gtest: %s\n", tmp);
    free(tmp);
  } else {
    fprintf(stderr, "LUL-in-gtest: %s\n", str);
  }
}

TEST(LUL, unwind_consistency) {
  // Set up LUL and get it to read unwind info for libxul.so, which is
  // all we care about here, plus (incidentally) practically every
  // other object in the process too.
  lul::LUL* lul = new lul::LUL(gtest_logging_sink_for_LUL);
  read_procmaps(lul);

  // Run unwind tests and receive information about how many there
  // were and how many were successful.
  lul->EnableUnwinding();
  int nTests = 0, nTestsPassed = 0;
  RunLulUnitTests(&nTests, &nTestsPassed, lul);
  EXPECT_TRUE(nTests == 6) << "Unexpected number of tests";
  EXPECT_TRUE(nTestsPassed == nTests) << "Not all tests passed";

  delete lul;
}
