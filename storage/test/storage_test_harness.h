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
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
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

  PRUint16 completionReason;

protected:
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
  PRInt32 result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString warnMsg;
  warnMsg.Append("An error occurred while executing an async statement: ");
  warnMsg.AppendInt(result);
  warnMsg.Append(" ");
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());

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
