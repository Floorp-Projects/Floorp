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
 * The Original Code is storage test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Sutherland <asutherland@asutherland.org> (Original Author)
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

#include "storage_test_harness.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsIEventTarget.h"

#include "sqlite3.h"

#include "mozilla/Monitor.h"

#include "mozIStorageStatementCallback.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageStatement.h"
#include "mozIStoragePendingStatement.h"

using mozilla::Monitor;
using mozilla::MonitorAutoEnter;

/**
 * Verify that mozIStorageAsyncStatement's life-cycle never triggers a mutex on
 * the caller (generally main) thread.  We do this by decorating the sqlite
 * mutex logic with our own code that checks what thread it is being invoked on
 * and sets a flag if it is invoked on the main thread.  We are able to easily
 * decorate the SQLite mutex logic because SQLite allows us to retrieve the
 * current function pointers being used and then provide a new set.
 */

/* ===== Mutex Watching ===== */

sqlite3_mutex_methods orig_mutex_methods;
sqlite3_mutex_methods wrapped_mutex_methods;

bool mutex_used_on_watched_thread = false;
PRThread *watched_thread = NULL;
/**
 * Ugly hack to let us figure out what a connection's async thread is.  If we
 * were MOZILLA_INTERNAL_API and linked as such we could just include
 * mozStorageConnection.h and just ask Connection directly.  But that turns out
 * poorly.
 *
 * When the thread a mutex is invoked on isn't watched_thread we save it to this
 * variable.
 */
PRThread *last_non_watched_thread = NULL;

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


#define do_check_ok(aInvoc) do_check_true((aInvoc) == SQLITE_OK)

void hook_sqlite_mutex()
{
  // We need to initialize and teardown SQLite to get it to set up the
  // default mutex handlers for us so we can steal them and wrap them.
  sqlite3_initialize();
  sqlite3_shutdown();
  do_check_ok(::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &orig_mutex_methods));
  do_check_ok(::sqlite3_config(SQLITE_CONFIG_GETMUTEX, &wrapped_mutex_methods));
  wrapped_mutex_methods.xMutexEnter = wrapped_MutexEnter;
  wrapped_mutex_methods.xMutexTry = wrapped_MutexTry;
  do_check_ok(::sqlite3_config(SQLITE_CONFIG_MUTEX, &wrapped_mutex_methods));
}

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
//// Event Loop Spinning

class AsyncStatementSpinner : public mozIStorageStatementCallback,
                              public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  AsyncStatementSpinner();

  void SpinUntilCompleted();

  PRUint16 completionReason;

private:
  ~AsyncStatementSpinner() {}
  volatile bool mCompleted;
};

NS_IMPL_ISUPPORTS2(AsyncStatementSpinner,
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
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleCompletion(PRUint16 aReason)
{
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::Complete()
{
  mCompleted = true;
  return NS_OK;
}

void AsyncStatementSpinner::SpinUntilCompleted()
{
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  PRBool processed = PR_TRUE;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
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
class ThreadWedger : public nsRunnable
{
public:
  ThreadWedger(nsIEventTarget *aTarget)
  : mMonitor("thread wedger")
  , unwedged(false)
  {
    aTarget->Dispatch(this, aTarget->NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD Run()
  {
    MonitorAutoEnter automon(mMonitor);

    if (!unwedged)
      automon.Wait();

    return NS_OK;
  }

  void unwedge()
  {
    MonitorAutoEnter automon(mMonitor);
    unwedged = true;
    automon.Notify();
  }

private:
  Monitor mMonitor;
  bool unwedged;
};

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * Execute an async statement, blocking the main thread until we get the
 * callback completion notification.
 */
void
blocking_async_execute(mozIStorageBaseStatement *stmt)
{
  nsRefPtr<AsyncStatementSpinner> spinner(new AsyncStatementSpinner());

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
  nsRefPtr<AsyncStatementSpinner> spinner(new AsyncStatementSpinner());

  db->AsyncClose(spinner);
  spinner->SpinUntilCompleted();
}

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
  return asyncThread.forget();
}


////////////////////////////////////////////////////////////////////////////////
//// Tests

void
test_TrueAsyncStatement()
{
  hook_sqlite_mutex();

  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Start watching for forbidden mutex usage.
  watch_for_mutex_use_on_this_thread();

  // - statement with nothing to bind
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE test (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(stmt)
  );
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);

  // - statement with something to bind ordinally
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("INSERT INTO test (id) VALUES (?)"),
    getter_AddRefs(stmt)
  );
  stmt->BindInt32Parameter(0, 1);
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);
  
  // - statement with something to bind by name
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("INSERT INTO test (id) VALUES (:id)"),
    getter_AddRefs(stmt)
  );
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  nsCOMPtr<mozIStorageBindingParams> params;
  paramsArray->NewBindingParams(getter_AddRefs(params));
  params->BindInt32ByName(NS_LITERAL_CSTRING("id"), 2);
  paramsArray->AddParams(params);
  params = nsnull;
  stmt->BindParameters(paramsArray);
  paramsArray = nsnull;
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);

  // - now, make sure creating a sync statement does trigger our guard.
  // (If this doesn't happen, our test is bunk and it's important to know that.)
  nsCOMPtr<mozIStorageStatement> syncStmt;
  db->CreateStatement(NS_LITERAL_CSTRING("SELECT * FROM test"),
                      getter_AddRefs(syncStmt));
  syncStmt->Finalize();
  do_check_true(mutex_used_on_watched_thread);

  blocking_async_close(db);
}

/**
 * Test that cancellation before a statement is run successfully stops the
 * statement from executing.
 */
void
test_AsyncCancellation()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- wedge the thread
  nsCOMPtr<nsIThread> target(get_conn_async_thread(db));
  do_check_true(target);
  nsRefPtr<ThreadWedger> wedger (new ThreadWedger(target));

  // -- create statements and cancel them
  // - async
  nsCOMPtr<mozIStorageAsyncStatement> asyncStmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE asyncTable (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(asyncStmt)
  );

  nsRefPtr<AsyncStatementSpinner> asyncSpin(new AsyncStatementSpinner());
  nsCOMPtr<mozIStoragePendingStatement> asyncPend;
  (void)asyncStmt->ExecuteAsync(asyncSpin, getter_AddRefs(asyncPend));
  do_check_true(asyncPend);
  asyncPend->Cancel();

  // - sync
  nsCOMPtr<mozIStorageStatement> syncStmt;
  db->CreateStatement(
    NS_LITERAL_CSTRING("CREATE TABLE syncTable (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(syncStmt)
  );

  nsRefPtr<AsyncStatementSpinner> syncSpin(new AsyncStatementSpinner());
  nsCOMPtr<mozIStoragePendingStatement> syncPend;
  (void)syncStmt->ExecuteAsync(syncSpin, getter_AddRefs(syncPend));
  do_check_true(syncPend);
  syncPend->Cancel();

  // -- unwedge the async thread
  wedger->unwedge();

  // -- verify that both statements report they were canceled
  asyncSpin->SpinUntilCompleted();
  do_check_true(asyncSpin->completionReason ==
                mozIStorageStatementCallback::REASON_CANCELED);

  syncSpin->SpinUntilCompleted();
  do_check_true(syncSpin->completionReason ==
                mozIStorageStatementCallback::REASON_CANCELED);

  // -- verify that neither statement constructed their tables
  nsresult rv;
  PRBool exists;
  rv = db->TableExists(NS_LITERAL_CSTRING("asyncTable"), &exists);
  do_check_true(rv == NS_OK);
  do_check_false(exists);
  rv = db->TableExists(NS_LITERAL_CSTRING("syncTable"), &exists);
  do_check_true(rv == NS_OK);
  do_check_false(exists);

  // -- cleanup
  asyncStmt->Finalize();
  syncStmt->Finalize();
  blocking_async_close(db);
}

void (*gTests[])(void) = {
  // this test must be first because it hooks the mutex mechanics
  test_TrueAsyncStatement,
  test_AsyncCancellation,
};

const char *file = __FILE__;
#define TEST_NAME "true async statement"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
