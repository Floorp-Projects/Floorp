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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "sqlite3.h"

#include "jsapi.h"
#include "jsdate.h"

#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsError.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "nsThreadUtils.h"
#include "nsJSUtils.h"

#include "Variant.h"
#include "mozStoragePrivateHelpers.h"
#include "mozIStorageStatement.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageBindingParams.h"

namespace mozilla {
namespace storage {

nsresult
convertResultCode(int aSQLiteResultCode)
{
  // Drop off the extended result bits of the result code.
  int rc = aSQLiteResultCode & 0xFF;

  switch (rc) {
    case SQLITE_OK:
    case SQLITE_ROW:
    case SQLITE_DONE:
      return NS_OK;
    case SQLITE_CORRUPT:
    case SQLITE_NOTADB:
      return NS_ERROR_FILE_CORRUPTED;
    case SQLITE_PERM:
    case SQLITE_CANTOPEN:
      return NS_ERROR_FILE_ACCESS_DENIED;
    case SQLITE_BUSY:
      return NS_ERROR_STORAGE_BUSY;
    case SQLITE_LOCKED:
      return NS_ERROR_FILE_IS_LOCKED;
    case SQLITE_READONLY:
      return NS_ERROR_FILE_READ_ONLY;
    case SQLITE_IOERR:
      return NS_ERROR_STORAGE_IOERR;
    case SQLITE_FULL:
    case SQLITE_TOOBIG:
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    case SQLITE_NOMEM:
      return NS_ERROR_OUT_OF_MEMORY;
    case SQLITE_MISUSE:
      return NS_ERROR_UNEXPECTED;
    case SQLITE_ABORT:
    case SQLITE_INTERRUPT:
      return NS_ERROR_ABORT;
    case SQLITE_CONSTRAINT:
      return NS_ERROR_STORAGE_CONSTRAINT;
  }

  // generic error
#ifdef DEBUG
  nsCAutoString message;
  message.AppendLiteral("SQLite returned error code ");
  message.AppendInt(rc);
  message.AppendLiteral(" , Storage will convert it to NS_ERROR_FAILURE");
  NS_WARNING(message.get());
#endif
  return NS_ERROR_FAILURE;
}

void
checkAndLogStatementPerformance(sqlite3_stmt *aStatement)
{
  // Check to see if the query performed sorting operations or not.  If it
  // did, it may need to be optimized!
  int count = ::sqlite3_stmt_status(aStatement, SQLITE_STMTSTATUS_SORT, 1);
  if (count <= 0)
    return;

  const char *sql = ::sqlite3_sql(aStatement);

  // Check to see if this is marked to not warn
  if (::strstr(sql, "/* do not warn (bug "))
    return;

  nsCAutoString message;
  message.AppendInt(count);
  if (count == 1)
    message.Append(" sort operation has ");
  else
    message.Append(" sort operations have ");
  message.Append("occurred for the SQL statement '");
  nsPrintfCString address("0x%p", aStatement);
  message.Append(address);
  message.Append("'.  See https://developer.mozilla.org/En/Storage/Warnings "
                 "details.");
  NS_WARNING(message.get());
}

nsIVariant *
convertJSValToVariant(
  JSContext *aCtx,
  jsval aValue)
{
  if (JSVAL_IS_INT(aValue))
    return new IntegerVariant(JSVAL_TO_INT(aValue));

  if (JSVAL_IS_DOUBLE(aValue))
    return new FloatVariant(JSVAL_TO_DOUBLE(aValue));

  if (JSVAL_IS_STRING(aValue)) {
    JSString *str = JSVAL_TO_STRING(aValue);
    nsDependentJSString value;
    if (!value.init(aCtx, str))
        return nsnull;
    return new TextVariant(value);
  }

  if (JSVAL_IS_BOOLEAN(aValue))
    return new IntegerVariant((aValue == JSVAL_TRUE) ? 1 : 0);

  if (JSVAL_IS_NULL(aValue))
    return new NullVariant();

  if (JSVAL_IS_OBJECT(aValue)) {
    JSObject *obj = JSVAL_TO_OBJECT(aValue);
    // We only support Date instances, all others fail.
    if (!::js_DateIsValid(aCtx, obj))
      return nsnull;

    double msecd = ::js_DateGetMsecSinceEpoch(aCtx, obj);
    msecd *= 1000.0;
    PRInt64 msec;
    LL_D2L(msec, msecd);

    return new IntegerVariant(msec);
  }

  return nsnull;
}


namespace {
class CallbackEvent : public nsRunnable
{
public:
  CallbackEvent(mozIStorageCompletionCallback *aCallback)
  : mCallback(aCallback)
  {
  }

  NS_IMETHOD Run()
  {
    (void)mCallback->Complete();
    return NS_OK;
  }
private:
  nsCOMPtr<mozIStorageCompletionCallback> mCallback;
};
} // anonymous namespace
already_AddRefed<nsIRunnable>
newCompletionEvent(mozIStorageCompletionCallback *aCallback)
{
  NS_ASSERTION(aCallback, "Passing a null callback is a no-no!");
  nsCOMPtr<nsIRunnable> event = new CallbackEvent(aCallback);
  return event.forget();
}

/**
 * This code is heavily based on the sample at:
 *   http://www.sqlite.org/unlock_notify.html
 */
namespace {

class UnlockNotification
{
public:
  UnlockNotification()
  : mMutex("UnlockNotification mMutex")
  , mCondVar(mMutex, "UnlockNotification condVar")
  , mSignaled(false)
  {
  }

  void Wait()
  {
    mozilla::MutexAutoLock lock(mMutex);
    while (!mSignaled) {
      (void)mCondVar.Wait();
    }
  }

  void Signal()
  {
    mozilla::MutexAutoLock lock(mMutex);
    mSignaled = true;
    (void)mCondVar.Notify();
  }

private:
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  bool mSignaled;
};

void
UnlockNotifyCallback(void **aArgs,
                     int aArgsSize)
{
  for (int i = 0; i < aArgsSize; i++) {
    UnlockNotification *notification =
      static_cast<UnlockNotification *>(aArgs[i]);
    notification->Signal();
  }
}

int
WaitForUnlockNotify(sqlite3* aDatabase)
{
  UnlockNotification notification;
  int srv = ::sqlite3_unlock_notify(aDatabase, UnlockNotifyCallback,
                                    &notification);
  NS_ASSERTION(srv == SQLITE_LOCKED || srv == SQLITE_OK, "Bad result!");
  if (srv == SQLITE_OK)
    notification.Wait();

  return srv;
}

} // anonymous namespace

int
stepStmt(sqlite3_stmt* aStatement)
{
  bool checkedMainThread = false;

  sqlite3* db = ::sqlite3_db_handle(aStatement);
  (void)::sqlite3_extended_result_codes(db, 1);

  int srv;
  while ((srv = ::sqlite3_step(aStatement)) == SQLITE_LOCKED_SHAREDCACHE) {
    if (!checkedMainThread) {
      checkedMainThread = true;
      if (NS_IsMainThread()) {
        NS_WARNING("We won't allow blocking on the main thread!");
        break;
      }
    }

    srv = WaitForUnlockNotify(sqlite3_db_handle(aStatement));
    if (srv != SQLITE_OK)
      break;

    ::sqlite3_reset(aStatement);
  }

  (void)::sqlite3_extended_result_codes(db, 0);
  // Drop off the extended result bits of the result code.
  return srv & 0xFF;
}

int
prepareStmt(sqlite3* aDatabase,
            const nsCString &aSQL,
            sqlite3_stmt **_stmt)
{
  bool checkedMainThread = false;

  (void)::sqlite3_extended_result_codes(aDatabase, 1);

  int srv;
  while((srv = ::sqlite3_prepare_v2(aDatabase, aSQL.get(), -1, _stmt, NULL)) ==
        SQLITE_LOCKED_SHAREDCACHE) {
    if (!checkedMainThread) {
      checkedMainThread = true;
      if (NS_IsMainThread()) {
        NS_WARNING("We won't allow blocking on the main thread!");
        break;
      }
    }

    srv = WaitForUnlockNotify(aDatabase);
    if (srv != SQLITE_OK)
      break;
  }

  (void)::sqlite3_extended_result_codes(aDatabase, 0);
  // Drop off the extended result bits of the result code.
  return srv & 0xFF;
}

} // namespace storage
} // namespace mozilla
