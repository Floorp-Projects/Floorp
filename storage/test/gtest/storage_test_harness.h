/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef storage_test_harness_h__
#define storage_test_harness_h__

#include "gtest/gtest.h"

#include "prthread.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "mozilla/ReentrantMonitor.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageStatement.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageError.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEventTarget.h"

#include "sqlite3.h"

#define do_check_true(aCondition) EXPECT_TRUE(aCondition)

#define do_check_false(aCondition) EXPECT_FALSE(aCondition)

#define do_check_success(aResult) do_check_true(NS_SUCCEEDED(aResult))

#define do_check_eq(aExpected, aActual) do_check_true(aExpected == aActual)

#define do_check_ok(aInvoc) do_check_true((aInvoc) == SQLITE_OK)

already_AddRefed<mozIStorageService> getService();

already_AddRefed<mozIStorageConnection> getMemoryDatabase();

already_AddRefed<mozIStorageConnection> getDatabase(
    nsIFile* aDBFile = nullptr,
    uint32_t aConnectionFlags = mozIStorageService::CONNECTION_DEFAULT);

class AsyncStatementSpinner : public mozIStorageStatementCallback,
                              public mozIStorageCompletionCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  AsyncStatementSpinner();

  void SpinUntilCompleted();

  uint16_t completionReason;

 protected:
  virtual ~AsyncStatementSpinner() = default;
  volatile bool mCompleted;
};

class AsyncCompletionSpinner : public mozIStorageCompletionCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  AsyncCompletionSpinner();

  void SpinUntilCompleted();

  nsresult mCompletionReason;
  nsCOMPtr<nsISupports> mCompletionValue;

 protected:
  virtual ~AsyncCompletionSpinner() = default;
  volatile bool mCompleted;
};

#define NS_DECL_ASYNCSTATEMENTSPINNER \
  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet) override;

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * Execute an async statement, blocking the main thread until we get the
 * callback completion notification.
 */
void blocking_async_execute(mozIStorageBaseStatement* stmt);

/**
 * Invoke AsyncClose on the given connection, blocking the main thread until we
 * get the completion notification.
 */
void blocking_async_close(mozIStorageConnection* db);

////////////////////////////////////////////////////////////////////////////////
//// Mutex Watching

/**
 * Verify that mozIStorageAsyncStatement's life-cycle never triggers a mutex on
 * the caller (generally main) thread.  We do this by decorating the sqlite
 * mutex logic with our own code that checks what thread it is being invoked on
 * and sets a flag if it is invoked on the main thread.  We are able to easily
 * decorate the SQLite mutex logic because SQLite allows us to retrieve the
 * current function pointers being used and then provide a new set.
 */

extern sqlite3_mutex_methods orig_mutex_methods;
extern sqlite3_mutex_methods wrapped_mutex_methods;

extern bool mutex_used_on_watched_thread;
extern PRThread* watched_thread;
/**
 * Ugly hack to let us figure out what a connection's async thread is.  If we
 * were MOZILLA_INTERNAL_API and linked as such we could just include
 * mozStorageConnection.h and just ask Connection directly.  But that turns out
 * poorly.
 *
 * When the thread a mutex is invoked on isn't watched_thread we save it to this
 * variable.
 */
extern nsIThread* last_non_watched_thread;

/**
 * Set a flag if the mutex is used on the thread we are watching, but always
 * call the real mutex function.
 */
extern "C" void wrapped_MutexEnter(sqlite3_mutex* mutex);

extern "C" int wrapped_MutexTry(sqlite3_mutex* mutex);

class HookSqliteMutex {
 public:
  HookSqliteMutex() {
    // We need to initialize and teardown SQLite to get it to set up the
    // default mutex handlers for us so we can steal them and wrap them.
    do_check_ok(sqlite3_initialize());
    do_check_ok(sqlite3_shutdown());
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &orig_mutex_methods));
    do_check_ok(
        ::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &wrapped_mutex_methods));
    wrapped_mutex_methods.xMutexEnter = wrapped_MutexEnter;
    wrapped_mutex_methods.xMutexTry = wrapped_MutexTry;
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_MUTEX, &wrapped_mutex_methods));
  }

  ~HookSqliteMutex() {
    do_check_ok(sqlite3_shutdown());
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_MUTEX, &orig_mutex_methods));
    do_check_ok(sqlite3_initialize());
  }
};

/**
 * Call to clear the watch state and to set the watching against this thread.
 *
 * Check |mutex_used_on_watched_thread| to see if the mutex has fired since
 * this method was last called.  Since we're talking about the current thread,
 * there are no race issues to be concerned about
 */
void watch_for_mutex_use_on_this_thread();

////////////////////////////////////////////////////////////////////////////////
//// Thread Wedgers

/**
 * A runnable that blocks until code on another thread invokes its unwedge
 * method.  By dispatching this to a thread you can ensure that no subsequent
 * runnables dispatched to the thread will execute until you invoke unwedge.
 *
 * The wedger is self-dispatching, just construct it with its target.
 */
class ThreadWedger : public mozilla::Runnable {
 public:
  explicit ThreadWedger(nsIEventTarget* aTarget)
      : mozilla::Runnable("ThreadWedger"),
        mReentrantMonitor("thread wedger"),
        unwedged(false) {
    aTarget->Dispatch(this, aTarget->NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD Run() override {
    mozilla::ReentrantMonitorAutoEnter automon(mReentrantMonitor);

    if (!unwedged) automon.Wait();

    return NS_OK;
  }

  void unwedge() {
    mozilla::ReentrantMonitorAutoEnter automon(mReentrantMonitor);
    unwedged = true;
    automon.Notify();
  }

 private:
  mozilla::ReentrantMonitor mReentrantMonitor MOZ_UNANNOTATED;
  bool unwedged;
};

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * A horrible hack to figure out what the connection's async thread is.  By
 * creating a statement and async dispatching we can tell from the mutex who
 * is the async thread, PRThread style.  Then we map that to an nsIThread.
 */
already_AddRefed<nsIThread> get_conn_async_thread(mozIStorageConnection* db);

#endif  // storage_test_harness_h__
