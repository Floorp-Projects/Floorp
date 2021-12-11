/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWidgetsCID.h"
#include "nsIUserIdleService.h"
#include "mozilla/StackWalk.h"

#ifndef TEST_NAME
#  error "Must #define TEST_NAME before including places_test_harness_tail.h"
#endif

int gTestsIndex = 0;

#define TEST_INFO_STR "TEST-INFO | "

class RunNextTest : public mozilla::Runnable {
 public:
  RunNextTest() : mozilla::Runnable("RunNextTest") {}
  NS_IMETHOD Run() override {
    NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
    if (gTestsIndex < int(mozilla::ArrayLength(gTests))) {
      do_test_pending();
      Test& test = gTests[gTestsIndex++];
      (void)fprintf(stderr, TEST_INFO_STR "Running %s.\n", test.name);
      test.func();
    }

    do_test_finished();
    return NS_OK;
  }
};

static const bool kDebugRunNextTest = false;

void run_next_test() {
  if (kDebugRunNextTest) {
    printf_stderr("run_next_test()\n");
    MozWalkTheStack(stderr);
  }
  nsCOMPtr<nsIRunnable> event = new RunNextTest();
  do_check_success(NS_DispatchToCurrentThread(event));
}

int gPendingTests = 0;

void do_test_pending() {
  NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
  if (kDebugRunNextTest) {
    printf_stderr("do_test_pending()\n");
    MozWalkTheStack(stderr);
  }
  gPendingTests++;
}

void do_test_finished() {
  NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
  NS_ASSERTION(gPendingTests > 0, "Invalid pending test count!");
  gPendingTests--;
}

void disable_idle_service() {
  (void)fprintf(stderr, TEST_INFO_STR "Disabling Idle Service.\n");

  nsCOMPtr<nsIUserIdleService> idle =
      do_GetService("@mozilla.org/widget/useridleservice;1");
  idle->SetDisabled(true);
}

TEST(IHistory, Test)
{
  RefPtr<WaitForConnectionClosed> spinClose = new WaitForConnectionClosed();

  // Tinderboxes are constantly on idle.  Since idle tasks can interact with
  // tests, causing random failures, disable the idle service.
  disable_idle_service();

  do_test_pending();
  run_next_test();

  // Spin the event loop until we've run out of tests to run.
  mozilla::SpinEventLoopUntil("places:TEST(IHistory, Test)"_ns,
                              [&]() { return !gPendingTests; });

  // And let any other events finish before we quit.
  (void)NS_ProcessPendingEvents(nullptr);
}
