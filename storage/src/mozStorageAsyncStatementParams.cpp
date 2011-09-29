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
#include "nsCOMPtr.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageAsyncStatement.h"
#include "mozStorageAsyncStatementParams.h"
#include "mozIStorageStatement.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementParams

AsyncStatementParams::AsyncStatementParams(AsyncStatement *aStatement)
: mStatement(aStatement)
{
  NS_ASSERTION(mStatement != nsnull, "mStatement is null");
}

NS_IMPL_ISUPPORTS2(
  AsyncStatementParams
, mozIStorageStatementParams
, nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME AsyncStatementParams
#define XPC_MAP_QUOTED_CLASSNAME "AsyncStatementParams"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
AsyncStatementParams::SetProperty(
  nsIXPConnectWrappedNative *aWrapper,
  JSContext *aCtx,
  JSObject *aScopeObj,
  jsid aId,
  jsval *_vp,
  bool *_retval
)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

  if (JSID_IS_INT(aId)) {
    int idx = JSID_TO_INT(aId);

    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByIndex(idx, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (JSID_IS_STRING(aId)) {
    JSString *str = JSID_TO_STRING(aId);
    size_t length;
    const jschar *chars = JS_GetInternedStringCharsAndLength(str, &length);
    NS_ConvertUTF16toUTF8 name(chars, length);

    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByName(name, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementParams::NewResolve(
  nsIXPConnectWrappedNative *aWrapper,
  JSContext *aCtx,
  JSObject *aScopeObj,
  jsid aId,
  PRUint32 aFlags,
  JSObject **_objp,
  bool *_retval
)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  // We do not throw at any point after this because we want to allow the
  // prototype chain to be checked for the property.

  bool resolved = false;
  bool ok = true;
  if (JSID_IS_INT(aId)) {
    PRUint32 idx = JSID_TO_INT(aId);
    // All indexes are good because we don't know how many parameters there
    // really are.
    ok = ::JS_DefineElement(aCtx, aScopeObj, idx, JSVAL_VOID, nsnull,
                            nsnull, 0);
    resolved = true;
  }
  else if (JSID_IS_STRING(aId)) {
    // We are unable to tell if there's a parameter with this name and so
    // we must assume that there is.  This screws the rest of the prototype
    // chain, but people really shouldn't be depending on this anyways.
    ok = ::JS_DefinePropertyById(aCtx, aScopeObj, aId, JSVAL_VOID, nsnull,
                                 nsnull, 0);
    resolved = true;
  }

  *_retval = ok;
  *_objp = resolved && ok ? aScopeObj : nsnull;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
