/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set:sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "nsThreadUtils.h"

/**
 * This file tests that the storage service can be initialized off of the main
 * thread without issue.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helpers

class ServiceInitializer : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    nsCOMPtr<mozIStorageService> service = getService();
    do_check_true(service);
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

void
test_service_initialization_on_background_thread()
{
  nsCOMPtr<nsIRunnable> event = new ServiceInitializer();
  do_check_true(event);

  nsCOMPtr<nsIThread> thread;
  do_check_success(NS_NewThread(getter_AddRefs(thread)));

  do_check_success(thread->Dispatch(event, NS_DISPATCH_NORMAL));

  // Shutting down the thread will spin the event loop until all work in its
  // event queue is completed.  This will act as our thread synchronization.
  do_check_success(thread->Shutdown());
}

void (*gTests[])(void) = {
  test_service_initialization_on_background_thread,
};

const char *file = __FILE__;
#define TEST_NAME "Background Thread Initialization"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
