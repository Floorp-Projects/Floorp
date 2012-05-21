/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_NAME
#error "Must #define TEST_NAME before including storage_test_harness_tail.h"
#endif

#ifndef TEST_FILE
#error "Must #define TEST_FILE before include storage_test_harness_tail.h"
#endif

int
main(int aArgc,
     char **aArgv)
{
  ScopedXPCOM xpcom(TEST_NAME);

  for (size_t i = 0; i < mozilla::ArrayLength(gTests); i++)
    gTests[i]();

  if (gPassedTests == gTotalTests)
    passed(TEST_FILE);

  (void)printf("%i of %i tests passed\n", gPassedTests, gTotalTests);
}
