/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemory.h"
#include "nsString.h"

#include "mozilla/dom/MozStorageStatementRowBinding.h"
#include "mozStorageStatementRow.h"
#include "mozStorageStatement.h"

#include "jsapi.h"

#include "xpc_make_class.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementRow

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(StatementRow, mWindow)

NS_INTERFACE_TABLE_HEAD(StatementRow)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(StatementRow, nsISupports)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(StatementRow)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(StatementRow)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StatementRow)

StatementRow::StatementRow(nsPIDOMWindowInner* aWindow, Statement *aStatement)
: mWindow(aWindow),
  mStatement(aStatement)
{
}

JSObject*
StatementRow::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::MozStorageStatementRow_Binding::Wrap(aCx, this, aGivenProto);
}

void
StatementRow::NamedGetter(JSContext* aCx,
                          const nsAString& aName,
                          bool& aFound,
                          JS::MutableHandle<JS::Value> aResult,
                          mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  nsCString name = NS_ConvertUTF16toUTF8(aName);

  uint32_t idx;
  {
    nsresult rv = mStatement->GetColumnIndex(name, &idx);
    if (NS_FAILED(rv)) {
      // It's highly likely that the name doesn't exist, so let the JS engine
      // check the prototype chain and throw if that doesn't have the property
      // either.
      aFound = false;
      return;
    }
  }

  int32_t type;
  aRv = mStatement->GetTypeOfIndex(idx, &type);
  if (aRv.Failed()) {
    return;
  }

  switch (type) {
  case mozIStorageValueArray::VALUE_TYPE_INTEGER:
  case mozIStorageValueArray::VALUE_TYPE_FLOAT: {
    double dval;
    aRv = mStatement->GetDouble(idx, &dval);
    if (aRv.Failed()) {
      return;
    }
    aResult.set(::JS_NumberValue(dval));
    break;
  }
  case mozIStorageValueArray::VALUE_TYPE_TEXT: {
    uint32_t bytes;
    const char16_t *sval = reinterpret_cast<const char16_t *>(
        static_cast<mozIStorageStatement *>(mStatement)->
          AsSharedWString(idx, &bytes)
      );
    JSString *str = ::JS_NewUCStringCopyN(aCx, sval, bytes / 2);
    if (!str) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    aResult.setString(str);
    break;
  }
  case mozIStorageValueArray::VALUE_TYPE_BLOB: {
    uint32_t length;
    const uint8_t *blob = static_cast<mozIStorageStatement *>(mStatement)->
      AsSharedBlob(idx, &length);
    JS::Rooted<JSObject*> obj(aCx, ::JS_NewArrayObject(aCx, length));
    if (!obj) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    aResult.setObject(*obj);

    // Copy the blob over to the JS array.
    for (uint32_t i = 0; i < length; i++) {
      if (!::JS_DefineElement(aCx, obj, i, blob[i], JSPROP_ENUMERATE)) {
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return;
      }
    }
    break;
  }
  case mozIStorageValueArray::VALUE_TYPE_NULL:
    aResult.setNull();
    break;
  default:
    NS_ERROR("unknown column type returned, what's going on?");
    break;
  }
  aFound = true;
}

void
StatementRow::GetSupportedNames(nsTArray<nsString>& aNames)
{
  if (!mStatement) {
    return;
  }

  uint32_t columnCount;
  nsresult rv = mStatement->GetColumnCount(&columnCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  for (uint32_t i = 0; i < columnCount; i++) {
    nsAutoCString name;
    nsresult rv = mStatement->GetColumnName(i, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    aNames.AppendElement(NS_ConvertUTF8toUTF16(name));
  }
}

} // namespace storage
} // namespace mozilla
