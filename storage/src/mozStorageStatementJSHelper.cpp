/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
bool
stepFunc(JSContext *aCtx,
         uint32_t,
         jsval *_vp)
{
  nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  JSObject *obj = JS_THIS_OBJECT(aCtx, _vp);
  if (!obj) {
    return false;
  }

  nsresult rv =
    xpc->GetWrappedNativeOfJSObject(aCtx, obj, getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    ::JS_ReportError(aCtx, "mozIStorageStatement::step() could not obtain native statement");
    return false;
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

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  if (NS_SUCCEEDED(rv) && !hasMore) {
    *_vp = JSVAL_FALSE;
    (void)stmt->Reset();
    return true;
  }

  if (NS_FAILED(rv)) {
    ::JS_ReportError(aCtx, "mozIStorageStatement::step() returned an error");
    return false;
  }

  *_vp = BOOLEAN_TO_JSVAL(hasMore);
  return true;
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
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING,
               "Invalid state to get the row object - all calls will fail!");
#endif

  if (!aStatement->mStatementRowHolder) {
    JS::RootedObject scope(aCtx, aScopeObj);
    nsCOMPtr<mozIStorageStatementRow> row(new StatementRow(aStatement));
    NS_ENSURE_TRUE(row, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
    rv = xpc->WrapNative(
      aCtx,
      ::JS_GetGlobalForObject(aCtx, scope),
      row,
      NS_GET_IID(mozIStorageStatementRow),
      getter_AddRefs(aStatement->mStatementRowHolder)
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::Rooted<JSObject*> obj(aCtx);
  obj = aStatement->mStatementRowHolder->GetJSObject();
  NS_ENSURE_STATE(obj);

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
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY,
               "Invalid state to get the params object - all calls will fail!");
#endif

  if (!aStatement->mStatementParamsHolder) {
    JS::RootedObject scope(aCtx, aScopeObj);
    nsCOMPtr<mozIStorageStatementParams> params =
      new StatementParams(aStatement);
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
    rv = xpc->WrapNative(
      aCtx,
      ::JS_GetGlobalForObject(aCtx, scope),
      params,
      NS_GET_IID(mozIStorageStatementParams),
      getter_AddRefs(aStatement->mStatementParamsHolder)
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::Rooted<JSObject*> obj(aCtx);
  obj = aStatement->mStatementParamsHolder->GetJSObject();
  NS_ENSURE_STATE(obj);

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
                               bool *_retval)
{
  if (!JSID_IS_STRING(aId))
    return NS_OK;

  JS::Rooted<JSObject*> scope(aCtx, aScopeObj);
  JS::Rooted<jsid> id(aCtx, aId);

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

  JSFlatString *str = JSID_TO_FLAT_STRING(id);
  if (::JS_FlatStringEqualsAscii(str, "row"))
    return getRow(stmt, aCtx, scope, _result);

  if (::JS_FlatStringEqualsAscii(str, "params"))
    return getParams(stmt, aCtx, scope, _result);

  return NS_OK;
}


NS_IMETHODIMP
StatementJSHelper::NewResolve(nsIXPConnectWrappedNative *aWrapper,
                              JSContext *aCtx,
                              JSObject *aScopeObj,
                              jsid aId,
                              uint32_t aFlags,
                              JSObject **_objp,
                              bool *_retval)
{
  if (!JSID_IS_STRING(aId))
    return NS_OK;

  JS::RootedObject scope(aCtx, aScopeObj);
  if (::JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(aId), "step")) {
    *_retval = ::JS_DefineFunction(aCtx, scope, "step", stepFunc,
                                   0, 0) != nullptr;
    *_objp = scope.get();
    return NS_OK;
  }
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
