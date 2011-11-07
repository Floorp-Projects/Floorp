/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is places test code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWidgetsCID.h"
#include "nsIComponentRegistrar.h"

#ifndef TEST_NAME
#error "Must #define TEST_NAME before including places_test_harness_tail.h"
#endif

#ifndef TEST_FILE
#error "Must #define TEST_FILE before include places_test_harness_tail.h"
#endif

int gTestsIndex = 0;

#define TEST_INFO_STR "TEST-INFO | (%s) | "

class RunNextTest : public nsRunnable
{
public:
  NS_IMETHOD Run()
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
  ProfileScopedXPCOM xpcom(TEST_NAME);
  nsCOMPtr<nsIFile> profile = xpcom.GetProfileDirectory();

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
  (void)NS_ProcessPendingEvents(nsnull);

  // Check that we have passed all of our tests, and output accordingly.
  if (gPassedTests == gTotalTests) {
    passed(TEST_FILE);
  }

  (void)fprintf(stderr, TEST_INFO_STR  "%u of %u tests passed\n",
                TEST_FILE, unsigned(gPassedTests), unsigned(gTotalTests));

  return gPassedTests == gTotalTests ? 0 : -1;
}
