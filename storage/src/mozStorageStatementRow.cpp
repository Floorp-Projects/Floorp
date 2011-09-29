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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
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

  if (JSID_IS_STRING(aId)) {
    ::JSAutoByteString idBytes(aCtx, JSID_TO_STRING(aId));
    NS_ENSURE_TRUE(!!idBytes, NS_ERROR_OUT_OF_MEMORY);
    nsDependentCString jsid(idBytes.ptr());

    PRUint32 idx;
    nsresult rv = mStatement->GetColumnIndex(jsid, &idx);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 type;
    rv = mStatement->GetTypeOfIndex(idx, &type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type == mozIStorageValueArray::VALUE_TYPE_INTEGER ||
        type == mozIStorageValueArray::VALUE_TYPE_FLOAT) {
      double dval;
      rv = mStatement->GetDouble(idx, &dval);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!::JS_NewNumberValue(aCtx, dval, _vp)) {
        *_retval = PR_FALSE;
        return NS_OK;
      }
    }
    else if (type == mozIStorageValueArray::VALUE_TYPE_TEXT) {
      PRUint32 bytes;
      const jschar *sval = reinterpret_cast<const jschar *>(
        static_cast<mozIStorageStatement *>(mStatement)->
          AsSharedWString(idx, &bytes)
      );
      JSString *str = ::JS_NewUCStringCopyN(aCtx, sval, bytes / 2);
      if (!str) {
        *_retval = PR_FALSE;
        return NS_OK;
      }
      *_vp = STRING_TO_JSVAL(str);
    }
    else if (type == mozIStorageValueArray::VALUE_TYPE_BLOB) {
      PRUint32 length;
      const PRUint8 *blob = static_cast<mozIStorageStatement *>(mStatement)->
        AsSharedBlob(idx, &length);
      JSObject *obj = ::JS_NewArrayObject(aCtx, length, nsnull);
      if (!obj) {
        *_retval = PR_FALSE;
        return NS_OK;
      }
      *_vp = OBJECT_TO_JSVAL(obj);

      // Copy the blob over to the JS array.
      for (PRUint32 i = 0; i < length; i++) {
        jsval val = INT_TO_JSVAL(blob[i]);
        if (!::JS_SetElement(aCtx, aScopeObj, i, &val)) {
          *_retval = PR_FALSE;
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
                         PRUint32 aFlags,
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

    PRUint32 idx;
    nsresult rv = mStatement->GetColumnIndex(name, &idx);
    if (NS_FAILED(rv)) {
      // It's highly likely that the name doesn't exist, so let the JS engine
      // check the prototype chain and throw if that doesn't have the property
      // either.
      *_objp = NULL;
      return NS_OK;
    }

    *_retval = ::JS_DefinePropertyById(aCtx, aScopeObj, aId, JSVAL_VOID,
                                     nsnull, nsnull, 0);
    *_objp = aScopeObj;
    return NS_OK;
  }

  return NS_OK;
}

} // namespace storage
} // namescape mozilla
