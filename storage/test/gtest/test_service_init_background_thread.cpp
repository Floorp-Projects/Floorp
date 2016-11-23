/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set:sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "nsThreadUtils.h"

/**
 * This file tests that the storage service cannot be initialized off the main
 * thread.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helpers

class ServiceInitializer : public mozilla::Runnable
{
public:
  NS_IMETHOD Run() override
  {
    // Use an explicit do_GetService instead of getService so that the check in
    // getService doesn't blow up.
    nsCOMPtr<mozIStorageService> service = do_GetService("@mozilla.org/storage/service;1");
    do_check_false(service);
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

// HACK ALERT: this test fails if it runs after any of the other storage tests
// because the storage service will have been initialized and the
// do_GetService() call in ServiceInitializer::Run will succeed. So we need a
// way to force it to run before all the other storage gtests. As it happens,
// gtest has a concept of "death tests" that, among other things, run before
// all non-death tests. By merely using a "DeathTest" suffix here this becomes
// a death test -- even though it doesn't use any of the normal death test
// features -- which ensures this is the first storage test to run.
TEST(storage_service_init_background_thread_DeathTest, Test)
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
