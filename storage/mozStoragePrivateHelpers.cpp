/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sqlite3.h"

#include "jsfriendapi.h"

#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsError.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"
#include "nsJSUtils.h"

#include "Variant.h"
#include "mozStoragePrivateHelpers.h"
#include "mozIStorageStatement.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageBindingParams.h"

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gStorageLog;

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
  nsAutoCString message;
  message.AppendLiteral("SQLite returned error code ");
  message.AppendInt(rc);
  message.AppendLiteral(" , Storage will convert it to NS_ERROR_FAILURE");
  NS_WARNING_ASSERTION(rc == SQLITE_ERROR, message.get());
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

  // CREATE INDEX always sorts (sorting is a necessary step in creating
  // an index).  So ignore the warning there.
  if (::strstr(sql, "CREATE INDEX") || ::strstr(sql, "CREATE UNIQUE INDEX"))
    return;

  nsAutoCString message("Suboptimal indexes for the SQL statement ");
#ifdef MOZ_STORAGE_SORTWARNING_SQL_DUMP
  message.Append('`');
  message.Append(sql);
  message.AppendLiteral("` [");
  message.AppendInt(count);
  message.AppendLiteral(" sort operation(s)]");
#else
  nsPrintfCString address("0x%p", aStatement);
  message.Append(address);
#endif
  message.AppendLiteral(" (http://mzl.la/1FuID0j).");
  NS_WARNING(message.get());
}

nsIVariant *
convertJSValToVariant(
  JSContext *aCtx,
  const JS::Value& aValue)
{
  if (aValue.isInt32())
    return new IntegerVariant(aValue.toInt32());

  if (aValue.isDouble())
    return new FloatVariant(aValue.toDouble());

  if (aValue.isString()) {
    nsAutoJSString value;
    if (!value.init(aCtx, aValue.toString()))
        return nullptr;
    return new TextVariant(value);
  }

  if (aValue.isBoolean())
    return new IntegerVariant(aValue.isTrue() ? 1 : 0);

  if (aValue.isNull())
    return new NullVariant();

  if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCtx, &aValue.toObject());
    // We only support Date instances, all others fail.
    bool valid;
    if (!js::DateIsValid(aCtx, obj, &valid) || !valid)
      return nullptr;

    double msecd;
    if (!js::DateGetMsecSinceEpoch(aCtx, obj, &msecd))
      return nullptr;

    msecd *= 1000.0;
    int64_t msec = msecd;

    return new IntegerVariant(msec);
  }

  return nullptr;
}

Variant_base *
convertVariantToStorageVariant(nsIVariant* aVariant)
{
  RefPtr<Variant_base> variant = do_QueryObject(aVariant);
  if (variant) {
    // JS helpers already convert the JS representation to a Storage Variant,
    // in such a case there's nothing left to do here, so just pass-through.
    return variant;
  }

  if (!aVariant)
    return new NullVariant();

  uint16_t dataType;
  nsresult rv = aVariant->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, nullptr);

  switch (dataType) {
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64: {
      int64_t v;
      rv = aVariant->GetAsInt64(&v);
      NS_ENSURE_SUCCESS(rv, nullptr);
      return new IntegerVariant(v);
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE: {
      double v;
      rv = aVariant->GetAsDouble(&v);
      NS_ENSURE_SUCCESS(rv, nullptr);
      return new FloatVariant(v);
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING: {
      nsCString v;
      rv = aVariant->GetAsAUTF8String(v);
      NS_ENSURE_SUCCESS(rv, nullptr);
      return new UTF8TextVariant(v);
    }
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_ASTRING: {
      nsString v;
      rv = aVariant->GetAsAString(v);
      NS_ENSURE_SUCCESS(rv, nullptr);
      return new TextVariant(v);
    }
    case nsIDataType::VTYPE_ARRAY: {
      uint16_t type;
      nsIID iid;
      uint32_t len;
      void *rawArray;
      // Note this copies the array data.
      rv = aVariant->GetAsArray(&type, &iid, &len, &rawArray);
      NS_ENSURE_SUCCESS(rv, nullptr);
      if (type == nsIDataType::VTYPE_UINT8) {
        std::pair<uint8_t *, int> v(static_cast<uint8_t *>(rawArray), len);
        // Take ownership of the data avoiding a further copy.
        return new AdoptedBlobVariant(v);
      }
      MOZ_FALLTHROUGH;
    }
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_VOID:
      return new NullVariant();
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    default:
      NS_WARNING("Unsupported variant type");
      return nullptr;
  }

  return nullptr;
}

namespace {
class CallbackEvent : public Runnable
{
public:
  explicit CallbackEvent(mozIStorageCompletionCallback* aCallback)
    : Runnable("storage::CallbackEvent")
    , mCallback(aCallback)
  {
  }

  NS_IMETHOD Run() override
  {
    (void)mCallback->Complete(NS_OK, nullptr);
    return NS_OK;
  }
private:
  nsCOMPtr<mozIStorageCompletionCallback> mCallback;
};
} // namespace
already_AddRefed<nsIRunnable>
newCompletionEvent(mozIStorageCompletionCallback *aCallback)
{
  NS_ASSERTION(aCallback, "Passing a null callback is a no-no!");
  nsCOMPtr<nsIRunnable> event = new CallbackEvent(aCallback);
  return event.forget();
}

} // namespace storage
} // namespace mozilla
