/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWidgetsCID.h"
#include "nsIComponentRegistrar.h"
#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

#ifndef TEST_NAME
#error "Must #define TEST_NAME before including places_test_harness_tail.h"
#endif

#ifndef TEST_FILE
#error "Must #define TEST_FILE before include places_test_harness_tail.h"
#endif

int gTestsIndex = 0;

#define TEST_INFO_STR "TEST-INFO | (%s) | "

class RunNextTest : public mozilla::Runnable
{
public:
  NS_IMETHOD Run() override
  {
    NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
    if (gTestsIndex < int(mozilla::ArrayLength(gTests))) {
      do_test_pending();
      Test &test = gTests[gTestsIndex++];
      (void)fprintf(stderr, TEST_INFO_STR "Running %s.\n", TEST_FILE,
                    test.name);
      test.func();
    }

    do_test_finished();
    return NS_OK;
  }
};

void
run_next_test()
{
  nsCOMPtr<nsIRunnable> event = new RunNextTest();
  do_check_success(NS_DispatchToCurrentThread(event));
}

int gPendingTests = 0;

void
do_test_pending()
{
  NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
  gPendingTests++;
}

void
do_test_finished()
{
  NS_ASSERTION(NS_IsMainThread(), "Not running on the main thread?");
  NS_ASSERTION(gPendingTests > 0, "Invalid pending test count!");
  gPendingTests--;
}

void
disable_idle_service()
{
  (void)fprintf(stderr, TEST_INFO_STR  "Disabling Idle Service.\n", TEST_FILE);
  static NS_DEFINE_IID(kIdleCID, NS_IDLE_SERVICE_CID);
  nsresult rv;
  nsCOMPtr<nsIFactory> idleFactory = do_GetClassObject(kIdleCID, &rv);
  do_check_success(rv);
  nsCOMPtr<nsIComponentRegistrar> registrar;
  rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
  do_check_success(rv);
  rv = registrar->UnregisterFactory(kIdleCID, idleFactory);
  do_check_success(rv);
}

int
main(int aArgc,
     char** aArgv)
{
  ScopedXPCOM xpcom(TEST_NAME);
  if (xpcom.failed())
    return -1;
  // Initialize a profile folder to ensure a clean shutdown.
  nsCOMPtr<nsIFile> profile = xpcom.GetProfileDirectory();
  if (!profile) {
    fail("Couldn't get the profile directory.");
    return -1;
  }

#ifdef MOZ_CRASHREPORTER
    char* enabled = PR_GetEnv("MOZ_CRASHREPORTER");
    if (enabled && !strcmp(enabled, "1")) {
      // bug 787458: move this to an even-more-common location to use in all
      // C++ unittests
      nsCOMPtr<nsICrashReporter> crashreporter =
        do_GetService("@mozilla.org/toolkit/crash-reporter;1");
      if (crashreporter) {
        fprintf(stderr, "Setting up crash reporting\n");

        nsCOMPtr<nsIProperties> dirsvc =
          do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
        if (!dirsvc)
          NS_RUNTIMEABORT("Couldn't get directory service");
        nsCOMPtr<nsIFile> cwd;
        nsresult rv = dirsvc->Get(NS_OS_CURRENT_WORKING_DIR,
                                  NS_GET_IID(nsIFile),
                                  getter_AddRefs(cwd));
        if (NS_FAILED(rv))
          NS_RUNTIMEABORT("Couldn't get CWD");
        crashreporter->SetEnabled(true);
        crashreporter->SetMinidumpPath(cwd);
      }
    }
#endif

  RefPtr<WaitForConnectionClosed> spinClose = new WaitForConnectionClosed();

  // Tinderboxes are constantly on idle.  Since idle tasks can interact with
  // tests, causing random failures, disable the idle service.
  disable_idle_service();

  do_test_pending();
  run_next_test();

  // Spin the event loop until we've run out of tests to run.
  while (gPendingTests) {
    (void)NS_ProcessNextEvent();
  }

  // And let any other events finish before we quit.
  (void)NS_ProcessPendingEvents(nullptr);

  // Check that we have passed all of our tests, and output accordingly.
  if (gPassedTests == gTotalTests) {
    passed(TEST_FILE);
  }

  (void)fprintf(stderr, TEST_INFO_STR  "%u of %u tests passed\n",
                TEST_FILE, unsigned(gPassedTests), unsigned(gTotalTests));

  return gPassedTests == gTotalTests ? 0 : -1;
}
