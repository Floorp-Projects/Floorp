/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemory.h"
#include "nsString.h"

#include "mozStorageStatementRow.h"
#include "mozStorageStatement.h"

#include "jsapi.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementRow

StatementRow::StatementRow(Statement *aStatement)
: mStatement(aStatement)
{
}

NS_IMPL_ISUPPORTS2(
  StatementRow,
  mozIStorageStatementRow,
  nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementRow
#define XPC_MAP_QUOTED_CLASSNAME "StatementRow"
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementRow::GetProperty(nsIXPConnectWrappedNative *aWrapper,
                          JSContext *aCtx,
                          JSObject *aScopeObj,
                          jsid aId,
                          jsval *_vp,
                          bool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

  JS::RootedObject scope(aCtx, aScopeObj);
  if (JSID_IS_STRING(aId)) {
    ::JSAutoByteString idBytes(aCtx, JSID_TO_STRING(aId));
    NS_ENSURE_TRUE(!!idBytes, NS_ERROR_OUT_OF_MEMORY);
    nsDependentCString jsid(idBytes.ptr());

    uint32_t idx;
    nsresult rv = mStatement->GetColumnIndex(jsid, &idx);
    NS_ENSURE_SUCCESS(rv, rv);
    int32_t type;
    rv = mStatement->GetTypeOfIndex(idx, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type == mozIStorageValueArray::VALUE_TYPE_INTEGER ||
        type == mozIStorageValueArray::VALUE_TYPE_FLOAT) {
      double dval;
      rv = mStatement->GetDouble(idx, &dval);
      NS_ENSURE_SUCCESS(rv, rv);
      *_vp = ::JS_NumberValue(dval);
    }
    else if (type == mozIStorageValueArray::VALUE_TYPE_TEXT) {
      uint32_t bytes;
      const jschar *sval = reinterpret_cast<const jschar *>(
        static_cast<mozIStorageStatement *>(mStatement)->
          AsSharedWString(idx, &bytes)
      );
      JSString *str = ::JS_NewUCStringCopyN(aCtx, sval, bytes / 2);
      if (!str) {
        *_retval = false;
        return NS_OK;
      }
      *_vp = STRING_TO_JSVAL(str);
    }
    else if (type == mozIStorageValueArray::VALUE_TYPE_BLOB) {
      uint32_t length;
      const uint8_t *blob = static_cast<mozIStorageStatement *>(mStatement)->
        AsSharedBlob(idx, &length);
      JSObject *obj = ::JS_NewArrayObject(aCtx, length, nullptr);
      if (!obj) {
        *_retval = false;
        return NS_OK;
      }
      *_vp = OBJECT_TO_JSVAL(obj);

      // Copy the blob over to the JS array.
      for (uint32_t i = 0; i < length; i++) {
        JS::Rooted<JS::Value> val(aCtx, INT_TO_JSVAL(blob[i]));
        if (!::JS_SetElement(aCtx, scope, i, val.address())) {
          *_retval = false;
          return NS_OK;
        }
      }
    }
    else if (type == mozIStorageValueArray::VALUE_TYPE_NULL) {
      *_vp = JSVAL_NULL;
    }
    else {
      NS_ERROR("unknown column type returned, what's going on?");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
StatementRow::NewResolve(nsIXPConnectWrappedNative *aWrapper,
                         JSContext *aCtx,
                         JSObject *aScopeObj,
                         jsid aId,
                         uint32_t aFlags,
                         JSObject **_objp,
                         bool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  // We do not throw at any point after this because we want to allow the
  // prototype chain to be checked for the property.

  if (JSID_IS_STRING(aId)) {
    ::JSAutoByteString idBytes(aCtx, JSID_TO_STRING(aId));
    NS_ENSURE_TRUE(!!idBytes, NS_ERROR_OUT_OF_MEMORY);
    nsDependentCString name(idBytes.ptr());

    uint32_t idx;
    nsresult rv = mStatement->GetColumnIndex(name, &idx);
    if (NS_FAILED(rv)) {
      // It's highly likely that the name doesn't exist, so let the JS engine
      // check the prototype chain and throw if that doesn't have the property
      // either.
      *_objp = NULL;
      return NS_OK;
    }

    *_retval = ::JS_DefinePropertyById(aCtx, aScopeObj, aId, JSVAL_VOID,
                                     nullptr, nullptr, 0);
    *_objp = aScopeObj;
    return NS_OK;
  }

  return NS_OK;
}

} // namespace storage
} // namescape mozilla
