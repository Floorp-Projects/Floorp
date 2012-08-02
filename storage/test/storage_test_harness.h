/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsMemory.h"
#include "nsThreadUtils.h"
#include "nsDirectoryServiceDefs.h"
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
#include "nsThreadUtils.h"

static int gTotalTests = 0;
static int gPassedTests = 0;

#define do_check_true(aCondition) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (aCondition) { \
      gPassedTests++; \
    } else { \
      fail("%s | Expected true, got false at line %d", __FILE__, __LINE__); \
    } \
  PR_END_MACRO

#define do_check_false(aCondition) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (!aCondition) { \
      gPassedTests++; \
    } else { \
      fail("%s | Expected false, got true at line %d", __FILE__, __LINE__); \
    } \
  PR_END_MACRO

#define do_check_success(aResult) \
  do_check_true(NS_SUCCEEDED(aResult))

#ifdef LINUX
// XXX Linux opt builds on tinderbox are orange due to linking with stdlib.
// This is sad and annoying, but it's a workaround that works.
#define do_check_eq(aExpected, aActual) \
  do_check_true(aExpected == aActual)
#else
#include <sstream>

// Print nsresult as uint32_t
std::ostream& operator<<(std::ostream& aStream, const nsresult aInput)
{
  return aStream << static_cast<uint32_t>(aInput);
}

#define do_check_eq(aExpected, aActual) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (aExpected == aActual) { \
      gPassedTests++; \
    } else { \
      std::ostringstream temp; \
      temp << __FILE__ << " | Expected '" << aExpected << "', got '"; \
      temp << aActual <<"' at line " << __LINE__; \
      fail(temp.str().c_str()); \
    } \
  PR_END_MACRO
#endif

already_AddRefed<mozIStorageService>
getService()
{
  nsCOMPtr<mozIStorageService> ss =
    do_GetService("@mozilla.org/storage/service;1");
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
  int32_t result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString warnMsg;
  warnMsg.Append("An error occurred while executing an async statement: ");
  warnMsg.AppendInt(result);
  warnMsg.Append(" ");
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
AsyncStatementSpinner::Complete()
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
  NS_IMETHOD HandleResult(mozIStorageResultSet *aResultSet);

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
