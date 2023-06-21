/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef storage_test_harness_
#define storage_test_harness_

#include "storage_test_harness.h"

already_AddRefed<mozIStorageService> getService() {
  nsCOMPtr<mozIStorageService> ss =
      do_CreateInstance("@mozilla.org/storage/service;1");
  do_check_true(ss);
  return ss.forget();
}

already_AddRefed<mozIStorageConnection> getMemoryDatabase() {
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  nsresult rv = ss->OpenSpecialDatabase(
      kMozStorageMemoryStorageKey, VoidCString(),
      mozIStorageService::CONNECTION_DEFAULT, getter_AddRefs(conn));
  do_check_success(rv);
  return conn.forget();
}

already_AddRefed<mozIStorageConnection> getDatabase(nsIFile* aDBFile,
                                                    uint32_t aConnectionFlags) {
  nsCOMPtr<nsIFile> dbFile;
  nsresult rv;
  if (!aDBFile) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Can't get tmp dir off mainthread.");
    (void)NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                 getter_AddRefs(dbFile));
    NS_ASSERTION(dbFile, "The directory doesn't exists?!");

    rv = dbFile->Append(u"storage_test_db.sqlite"_ns);
    do_check_success(rv);
  } else {
    dbFile = aDBFile;
  }

  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabase(dbFile, aConnectionFlags, getter_AddRefs(conn));
  do_check_success(rv);
  return conn.forget();
}

NS_IMPL_ISUPPORTS(AsyncStatementSpinner, mozIStorageStatementCallback,
                  mozIStorageCompletionCallback)

AsyncStatementSpinner::AsyncStatementSpinner()
    : completionReason(0), mCompleted(false) {}

NS_IMETHODIMP
AsyncStatementSpinner::HandleResult(mozIStorageResultSet* aResultSet) {
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleError(mozIStorageError* aError) {
  int32_t result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString warnMsg;
  warnMsg.AppendLiteral(
      "An error occurred while executing an async statement: ");
  warnMsg.AppendInt(result);
  warnMsg.Append(' ');
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());

  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleCompletion(uint16_t aReason) {
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::Complete(nsresult, nsISupports*) {
  mCompleted = true;
  return NS_OK;
}

void AsyncStatementSpinner::SpinUntilCompleted() {
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

#define NS_DECL_ASYNCSTATEMENTSPINNER \
  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet) override;

NS_IMPL_ISUPPORTS(AsyncCompletionSpinner, mozIStorageCompletionCallback)

AsyncCompletionSpinner::AsyncCompletionSpinner()
    : mCompletionReason(NS_OK), mCompleted(false) {}

NS_IMETHODIMP
AsyncCompletionSpinner::Complete(nsresult reason, nsISupports* value) {
  mCompleted = true;
  mCompletionReason = reason;
  mCompletionValue = value;
  return NS_OK;
}

void AsyncCompletionSpinner::SpinUntilCompleted() {
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * Execute an async statement, blocking the main thread until we get the
 * callback completion notification.
 */
void blocking_async_execute(mozIStorageBaseStatement* stmt) {
  RefPtr<AsyncStatementSpinner> spinner(new AsyncStatementSpinner());

  nsCOMPtr<mozIStoragePendingStatement> pendy;
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pendy));
  spinner->SpinUntilCompleted();
}

/**
 * Invoke AsyncClose on the given connection, blocking the main thread until we
 * get the completion notification.
 */
void blocking_async_close(mozIStorageConnection* db) {
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
PRThread* watched_thread = nullptr;
/**
 * Ugly hack to let us figure out what a connection's async thread is.  If we
 * were MOZILLA_INTERNAL_API and linked as such we could just include
 * mozStorageConnection.h and just ask Connection directly.  But that turns out
 * poorly.
 *
 * When the thread a mutex is invoked on isn't watched_thread we save it to this
 * variable.
 */
nsIThread* last_non_watched_thread = nullptr;

/**
 * Set a flag if the mutex is used on the thread we are watching, but always
 * call the real mutex function.
 */
extern "C" void wrapped_MutexEnter(sqlite3_mutex* mutex) {
  if (PR_GetCurrentThread() == watched_thread) {
    mutex_used_on_watched_thread = true;
  } else {
    last_non_watched_thread = NS_GetCurrentThread();
  }
  orig_mutex_methods.xMutexEnter(mutex);
}

extern "C" int wrapped_MutexTry(sqlite3_mutex* mutex) {
  if (::PR_GetCurrentThread() == watched_thread) {
    mutex_used_on_watched_thread = true;
  }
  return orig_mutex_methods.xMutexTry(mutex);
}

/**
 * Call to clear the watch state and to set the watching against this thread.
 *
 * Check |mutex_used_on_watched_thread| to see if the mutex has fired since
 * this method was last called.  Since we're talking about the current thread,
 * there are no race issues to be concerned about
 */
void watch_for_mutex_use_on_this_thread() {
  watched_thread = ::PR_GetCurrentThread();
  mutex_used_on_watched_thread = false;
}

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * A horrible hack to figure out what the connection's async thread is.  By
 * creating a statement and async dispatching we can tell from the mutex who
 * is the async thread, PRThread style.  Then we map that to an nsIThread.
 */
already_AddRefed<nsIThread> get_conn_async_thread(mozIStorageConnection* db) {
  // Make sure we are tracking the current thread as the watched thread
  watch_for_mutex_use_on_this_thread();

  // - statement with nothing to bind
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement("SELECT 1"_ns, getter_AddRefs(stmt));
  blocking_async_execute(stmt);
  stmt->Finalize();

  nsCOMPtr<nsIThread> asyncThread = last_non_watched_thread;

  // Additionally, check that the thread we get as the background thread is the
  // same one as the one we report from getInterface.
  nsCOMPtr<nsIEventTarget> target = do_GetInterface(db);
  nsCOMPtr<nsIThread> allegedAsyncThread = do_QueryInterface(target);
  do_check_eq(allegedAsyncThread, asyncThread);
  return asyncThread.forget();
}

#endif  // storage_test_harness_h__
