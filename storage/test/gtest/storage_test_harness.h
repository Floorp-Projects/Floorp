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
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsMemory.h"
#include "nsServiceManagerUtils.h"
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

#define do_check_true(aCondition) \
  EXPECT_TRUE(aCondition)

#define do_check_false(aCondition) \
  EXPECT_FALSE(aCondition)

#define do_check_success(aResult) \
  do_check_true(NS_SUCCEEDED(aResult))

#define do_check_eq(aExpected, aActual) \
  do_check_true(aExpected == aActual)

#define do_check_ok(aInvoc) \
  do_check_true((aInvoc) == SQLITE_OK)

already_AddRefed<mozIStorageService>
getService()
{
  nsCOMPtr<mozIStorageService> ss =
    do_CreateInstance("@mozilla.org/storage/service;1");
  do_check_true(ss);
  return ss.forget();
}

already_AddRefed<mozIStorageConnection>
getMemoryDatabase()
{
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  nsresult rv = ss->OpenSpecialDatabase("memory", getter_AddRefs(conn));
  do_check_success(rv);
  return conn.forget();
}

already_AddRefed<mozIStorageConnection>
getDatabase()
{
  nsCOMPtr<nsIFile> dbFile;
  (void)NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                               getter_AddRefs(dbFile));
  NS_ASSERTION(dbFile, "The directory doesn't exists?!");

  nsresult rv = dbFile->Append(NS_LITERAL_STRING("storage_test_db.sqlite"));
  do_check_success(rv);

  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabase(dbFile, getter_AddRefs(conn));
  do_check_success(rv);
  return conn.forget();
}


class AsyncStatementSpinner : public mozIStorageStatementCallback
                            , public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  AsyncStatementSpinner();

  void SpinUntilCompleted();

  uint16_t completionReason;

protected:
  virtual ~AsyncStatementSpinner() {}
  volatile bool mCompleted;
};

NS_IMPL_ISUPPORTS(AsyncStatementSpinner,
                  mozIStorageStatementCallback,
                  mozIStorageCompletionCallback)

AsyncStatementSpinner::AsyncStatementSpinner()
: completionReason(0)
, mCompleted(false)
{
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleResult(mozIStorageResultSet *aResultSet)
{
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleError(mozIStorageError *aError)
{
  int32_t result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString warnMsg;
  warnMsg.AppendLiteral("An error occurred while executing an async statement: ");
  warnMsg.AppendInt(result);
  warnMsg.Append(' ');
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());

  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleCompletion(uint16_t aReason)
{
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::Complete(nsresult, nsISupports*)
{
  mCompleted = true;
  return NS_OK;
}

void AsyncStatementSpinner::SpinUntilCompleted()
{
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

#define NS_DECL_ASYNCSTATEMENTSPINNER \
  NS_IMETHOD HandleResult(mozIStorageResultSet *aResultSet) override;

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * Execute an async statement, blocking the main thread until we get the
 * callback completion notification.
 */
void
blocking_async_execute(mozIStorageBaseStatement *stmt)
{
  RefPtr<AsyncStatementSpinner> spinner(new AsyncStatementSpinner());

  nsCOMPtr<mozIStoragePendingStatement> pendy;
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pendy));
  spinner->SpinUntilCompleted();
}

/**
 * Invoke AsyncClose on the given connection, blocking the main thread until we
 * get the completion notification.
 */
void
blocking_async_close(mozIStorageConnection *db)
{
  RefPtr<AsyncStatementSpinner> spinner(new AsyncStatementSpinner());

  db->AsyncClose(spinner);
  spinner->SpinUntilCompleted();
}

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

sqlite3_mutex_methods orig_mutex_methods;
sqlite3_mutex_methods wrapped_mutex_methods;

bool mutex_used_on_watched_thread = false;
PRThread *watched_thread = nullptr;
/**
 * Ugly hack to let us figure out what a connection's async thread is.  If we
 * were MOZILLA_INTERNAL_API and linked as such we could just include
 * mozStorageConnection.h and just ask Connection directly.  But that turns out
 * poorly.
 *
 * When the thread a mutex is invoked on isn't watched_thread we save it to this
 * variable.
 */
PRThread *last_non_watched_thread = nullptr;

/**
 * Set a flag if the mutex is used on the thread we are watching, but always
 * call the real mutex function.
 */
extern "C" void wrapped_MutexEnter(sqlite3_mutex *mutex)
{
  PRThread *curThread = ::PR_GetCurrentThread();
  if (curThread == watched_thread)
    mutex_used_on_watched_thread = true;
  else
    last_non_watched_thread = curThread;
  orig_mutex_methods.xMutexEnter(mutex);
}

extern "C" int wrapped_MutexTry(sqlite3_mutex *mutex)
{
  if (::PR_GetCurrentThread() == watched_thread)
    mutex_used_on_watched_thread = true;
  return orig_mutex_methods.xMutexTry(mutex);
}

class HookSqliteMutex
{
public:
  HookSqliteMutex()
  {
    // We need to initialize and teardown SQLite to get it to set up the
    // default mutex handlers for us so we can steal them and wrap them.
    do_check_ok(sqlite3_initialize());
    do_check_ok(sqlite3_shutdown());
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &orig_mutex_methods));
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &wrapped_mutex_methods));
    wrapped_mutex_methods.xMutexEnter = wrapped_MutexEnter;
    wrapped_mutex_methods.xMutexTry = wrapped_MutexTry;
    do_check_ok(::sqlite3_config(SQLITE_CONFIG_MUTEX, &wrapped_mutex_methods));
  }

  ~HookSqliteMutex()
  {
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
void watch_for_mutex_use_on_this_thread()
{
  watched_thread = ::PR_GetCurrentThread();
  mutex_used_on_watched_thread = false;
}


////////////////////////////////////////////////////////////////////////////////
//// Thread Wedgers

/**
 * A runnable that blocks until code on another thread invokes its unwedge
 * method.  By dispatching this to a thread you can ensure that no subsequent
 * runnables dispatched to the thread will execute until you invoke unwedge.
 *
 * The wedger is self-dispatching, just construct it with its target.
 */
class ThreadWedger : public mozilla::Runnable
{
public:
  explicit ThreadWedger(nsIEventTarget *aTarget)
  : mReentrantMonitor("thread wedger")
  , unwedged(false)
  {
    aTarget->Dispatch(this, aTarget->NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD Run() override
  {
    mozilla::ReentrantMonitorAutoEnter automon(mReentrantMonitor);

    if (!unwedged)
      automon.Wait();

    return NS_OK;
  }

  void unwedge()
  {
    mozilla::ReentrantMonitorAutoEnter automon(mReentrantMonitor);
    unwedged = true;
    automon.Notify();
  }

private:
  mozilla::ReentrantMonitor mReentrantMonitor;
  bool unwedged;
};

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * A horrible hack to figure out what the connection's async thread is.  By
 * creating a statement and async dispatching we can tell from the mutex who
 * is the async thread, PRThread style.  Then we map that to an nsIThread.
 */
already_AddRefed<nsIThread>
get_conn_async_thread(mozIStorageConnection *db)
{
  // Make sure we are tracking the current thread as the watched thread
  watch_for_mutex_use_on_this_thread();

  // - statement with nothing to bind
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("SELECT 1"),
    getter_AddRefs(stmt));
  blocking_async_execute(stmt);
  stmt->Finalize();

  nsCOMPtr<nsIThreadManager> threadMan =
    do_GetService("@mozilla.org/thread-manager;1");
  nsCOMPtr<nsIThread> asyncThread;
  threadMan->GetThreadFromPRThread(last_non_watched_thread,
                                   getter_AddRefs(asyncThread));

  // Additionally, check that the thread we get as the background thread is the
  // same one as the one we report from getInterface.
  nsCOMPtr<nsIEventTarget> target = do_GetInterface(db);
  nsCOMPtr<nsIThread> allegedAsyncThread = do_QueryInterface(target);
  PRThread *allegedPRThread;
  (void)allegedAsyncThread->GetPRThread(&allegedPRThread);
  do_check_eq(allegedPRThread, last_non_watched_thread);
  return asyncThread.forget();
}

#endif // storage_test_harness_h__

