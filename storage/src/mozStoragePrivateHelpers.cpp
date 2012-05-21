/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sqlite3.h"

#include "jsapi.h"
#include "jsfriendapi.h"

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

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gStorageLog;
#endif

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
  JS::Value aValue)
{
  if (aValue.isInt32())
    return new IntegerVariant(aValue.toInt32());

  if (aValue.isDouble())
    return new FloatVariant(aValue.toDouble());

  if (aValue.isString()) {
    nsDependentJSString value;
    if (!value.init(aCtx, aValue))
        return nsnull;
    return new TextVariant(value);
  }

  if (aValue.isBoolean())
    return new IntegerVariant(aValue.isTrue() ? 1 : 0);

  if (aValue.isNull())
    return new NullVariant();

  if (aValue.isObject()) {
    JSObject* obj = &aValue.toObject();
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



} // namespace storage
} // namespace mozilla
