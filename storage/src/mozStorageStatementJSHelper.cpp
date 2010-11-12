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
 * The Original Code is mozStorage code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#include "nsIXPConnect.h"
#include "mozStorageStatement.h"
#include "mozStorageService.h"

#include "nsMemory.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"

#include "mozStorageStatementJSHelper.h"

#include "mozStorageStatementRow.h"
#include "mozStorageStatementParams.h"

#include "jsapi.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

static
JSBool
stepFunc(JSContext *aCtx,
         PRUint32,
         jsval *_vp)
{
  nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsresult rv = xpc->GetWrappedNativeOfJSObject(
    aCtx, JS_THIS_OBJECT(aCtx, _vp), getter_AddRefs(wrapper)
  );
  if (NS_FAILED(rv)) {
    ::JS_ReportError(aCtx, "mozIStorageStatement::step() could not obtain native statement");
    return JS_FALSE;
  }

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> isStatement(
      do_QueryInterface(wrapper->Native())
    );
    NS_ASSERTION(isStatement, "How is this not a statement?!");
  }
#endif

  Statement *stmt = static_cast<Statement *>(
    static_cast<mozIStorageStatement *>(wrapper->Native())
  );

  PRBool hasMore = PR_FALSE;
  rv = stmt->ExecuteStep(&hasMore);
  if (NS_SUCCEEDED(rv) && !hasMore) {
    *_vp = JSVAL_FALSE;
    (void)stmt->Reset();
    return JS_TRUE;
  }

  if (NS_FAILED(rv)) {
    ::JS_ReportError(aCtx, "mozIStorageStatement::step() returned an error");
    return JS_FALSE;
  }

  *_vp = BOOLEAN_TO_JSVAL(hasMore);
  return JS_TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//// StatementJSHelper

nsresult
StatementJSHelper::getRow(Statement *aStatement,
                          JSContext *aCtx,
                          JSObject *aScopeObj,
                          jsval *_row)
{
  nsresult rv;

#ifdef DEBUG
  PRInt32 state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING,
               "Invalid state to get the row object - all calls will fail!");
#endif

  if (!aStatement->mStatementRowHolder) {
    nsCOMPtr<mozIStorageStatementRow> row(new StatementRow(aStatement));
    NS_ENSURE_TRUE(row, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
    rv = xpc->WrapNative(
      aCtx,
      ::JS_GetGlobalForObject(aCtx, aScopeObj),
      row,
      NS_GET_IID(mozIStorageStatementRow),
      getter_AddRefs(aStatement->mStatementRowHolder)
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JSObject *obj = nsnull;
  rv = aStatement->mStatementRowHolder->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  *_row = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

nsresult
StatementJSHelper::getParams(Statement *aStatement,
                             JSContext *aCtx,
                             JSObject *aScopeObj,
                             jsval *_params)
{
  nsresult rv;

#ifdef DEBUG
  PRInt32 state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY,
               "Invalid state to get the params object - all calls will fail!");
#endif

  if (!aStatement->mStatementParamsHolder) {
    nsCOMPtr<mozIStorageStatementParams> params =
      new StatementParams(aStatement);
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
    rv = xpc->WrapNative(
      aCtx,
      ::JS_GetGlobalForObject(aCtx, aScopeObj),
      params,
      NS_GET_IID(mozIStorageStatementParams),
      getter_AddRefs(aStatement->mStatementParamsHolder)
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JSObject *obj = nsnull;
  rv = aStatement->mStatementParamsHolder->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  *_params = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) StatementJSHelper::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) StatementJSHelper::Release() { return 1; }
NS_INTERFACE_MAP_BEGIN(StatementJSHelper)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementJSHelper
#define XPC_MAP_QUOTED_CLASSNAME "StatementJSHelper"
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementJSHelper::GetProperty(nsIXPConnectWrappedNative *aWrapper,
                               JSContext *aCtx,
                               JSObject *aScopeObj,
                               jsid aId,
                               jsval *_result,
                               PRBool *_retval)
{
  if (!JSID_IS_STRING(aId))
    return NS_OK;

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> isStatement(
                                     do_QueryInterface(aWrapper->Native()));
    NS_ASSERTION(isStatement, "How is this not a statement?!");
  }
#endif

  Statement *stmt = static_cast<Statement *>(
    static_cast<mozIStorageStatement *>(aWrapper->Native())
  );

  JSString *str = JSID_TO_STRING(aId);
  if (::JS_MatchStringAndAscii(str, "row"))
    return getRow(stmt, aCtx, aScopeObj, _result);

  if (::JS_MatchStringAndAscii(str, "params"))
    return getParams(stmt, aCtx, aScopeObj, _result);

  return NS_OK;
}


NS_IMETHODIMP
StatementJSHelper::NewResolve(nsIXPConnectWrappedNative *aWrapper,
                              JSContext *aCtx,
                              JSObject *aScopeObj,
                              jsid aId,
                              PRUint32 aFlags,
                              JSObject **_objp,
                              PRBool *_retval)
{
  if (!JSID_IS_STRING(aId))
    return NS_OK;

  if (::JS_MatchStringAndAscii(JSID_TO_STRING(aId), "step")) {
    *_retval = ::JS_DefineFunction(aCtx, aScopeObj, "step", stepFunc,
                                   0, 0) != nsnull;
    *_objp = aScopeObj;
    return NS_OK;
  }
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
