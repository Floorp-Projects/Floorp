/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"
#include "prinrval.h"

/**
 * Helper to verify that the event loop was spun.  As long as this is dispatched
 * prior to a call to Close()/SpinningSynchronousClose() we are guaranteed this
 * will be run if the event loop is spun to perform a close.  This is because
 * SpinningSynchronousClose must spin the event loop to realize the close
 * completed and our runnable will already be enqueued and therefore run before
 * the AsyncCloseConnection's callback.  Note that this invariant may be
 * violated if our runnables end up in different queues thanks to Quantum
 * changes, so this test may need to be updated if the close dispatch changes.
 */
class CompletionRunnable final : public Runnable
{
public:
  explicit CompletionRunnable()
    : mDone(false)
  {
  }

  NS_IMETHOD Run() override
  {
    mDone = true;
    return NS_OK;
  }

  bool mDone;
};

// Can only run in optimized builds, or it would assert.
#ifndef DEBUG
TEST(storage_spinningSynchronousClose, CloseOnAsync)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  // Run an async statement.
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  do_check_success(db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE test (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(stmt)
  ));
  nsCOMPtr<mozIStoragePendingStatement> p;
  do_check_success(stmt->ExecuteAsync(nullptr, getter_AddRefs(p)));
  do_check_success(stmt->Finalize());

  // Wrongly use Close() instead of AsyncClose().
  RefPtr<CompletionRunnable> event = new CompletionRunnable();
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  do_check_false(NS_SUCCEEDED(db->Close()));
  do_check_true(event->mDone);
}
#endif

TEST(storage_spinningSynchronousClose, spinningSynchronousCloseOnAsync)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  // Run an async statement.
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  do_check_success(db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE test (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(stmt)
  ));
  nsCOMPtr<mozIStoragePendingStatement> p;
  do_check_success(stmt->ExecuteAsync(nullptr, getter_AddRefs(p)));
  do_check_success(stmt->Finalize());

  // Use the spinning close API.
  RefPtr<CompletionRunnable> event = new CompletionRunnable();
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  do_check_success(db->SpinningSynchronousClose());
  do_check_true(event->mDone);
}
