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

#include "xpc_make_class.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

static bool stepFunc(JSContext* aCtx, uint32_t argc, JS::Value* _vp) {
  JS::CallArgs args = CallArgsFromVp(argc, _vp);

  nsCOMPtr<nsIXPConnect> xpc(nsIXPConnect::XPConnect());
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;

  if (!args.thisv().isObject()) {
    ::JS_ReportErrorASCII(aCtx, "mozIStorageStatement::step() requires object");
    return false;
  }

  JSObject* obj = &args.thisv().toObject();
  nsresult rv =
      xpc->GetWrappedNativeOfJSObject(aCtx, obj, getter_AddRefs(wrapper));
  if (NS_FAILED(rv)) {
    ::JS_ReportErrorASCII(
        aCtx, "mozIStorageStatement::step() could not obtain native statement");
    return false;
  }

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> isStatement(
        do_QueryInterface(wrapper->Native()));
    NS_ASSERTION(isStatement, "How is this not a statement?!");
  }
#endif

  Statement* stmt = static_cast<Statement*>(
      static_cast<mozIStorageStatement*>(wrapper->Native()));

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  if (NS_SUCCEEDED(rv) && !hasMore) {
    args.rval().setBoolean(false);
    (void)stmt->Reset();
    return true;
  }

  if (NS_FAILED(rv)) {
    ::JS_ReportErrorASCII(aCtx,
                          "mozIStorageStatement::step() returned an error");
    return false;
  }

  args.rval().setBoolean(hasMore);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//// StatementJSHelper

nsresult StatementJSHelper::getRow(Statement* aStatement, JSContext* aCtx,
                                   JSObject* aScopeObj, JS::Value* _row) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING,
               "Invalid state to get the row object - all calls will fail!");
#endif

  JS::RootedObject scope(aCtx, aScopeObj);

  if (!aStatement->mStatementRowHolder) {
    dom::GlobalObject global(aCtx, scope);
    if (global.Failed()) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(global.GetAsSupports());

    RefPtr<StatementRow> row(new StatementRow(window, aStatement));
    NS_ENSURE_TRUE(row, NS_ERROR_OUT_OF_MEMORY);

    RefPtr<StatementRowHolder> rowHolder = new StatementRowHolder(row);
    NS_ENSURE_TRUE(rowHolder, NS_ERROR_OUT_OF_MEMORY);

    aStatement->mStatementRowHolder =
        new nsMainThreadPtrHolder<StatementRowHolder>(
            "Statement::mStatementRowHolder", rowHolder);
  }

  RefPtr<StatementRow> row(aStatement->mStatementRowHolder->Get());
  JSObject* obj = row->WrapObject(aCtx, nullptr);
  if (!obj) {
    return NS_ERROR_UNEXPECTED;
  }

  _row->setObject(*obj);
  return NS_OK;
}

nsresult StatementJSHelper::getParams(Statement* aStatement, JSContext* aCtx,
                                      JSObject* aScopeObj, JS::Value* _params) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY,
               "Invalid state to get the params object - all calls will fail!");
#endif

  JS::RootedObject scope(aCtx, aScopeObj);

  if (!aStatement->mStatementParamsHolder) {
    dom::GlobalObject global(aCtx, scope);
    if (global.Failed()) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(global.GetAsSupports());

    RefPtr<StatementParams> params(new StatementParams(window, aStatement));
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);

    RefPtr<StatementParamsHolder> paramsHolder =
        new StatementParamsHolder(params);
    NS_ENSURE_TRUE(paramsHolder, NS_ERROR_OUT_OF_MEMORY);

    aStatement->mStatementParamsHolder =
        new nsMainThreadPtrHolder<StatementParamsHolder>(
            "Statement::mStatementParamsHolder", paramsHolder);
  }

  RefPtr<StatementParams> params(aStatement->mStatementParamsHolder->Get());
  JSObject* obj = params->WrapObject(aCtx, nullptr);
  if (!obj) {
    return NS_ERROR_UNEXPECTED;
  }

  _params->setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP_(MozExternalRefCountType) StatementJSHelper::AddRef() {
  return 2;
}
NS_IMETHODIMP_(MozExternalRefCountType) StatementJSHelper::Release() {
  return 1;
}
NS_INTERFACE_MAP_BEGIN(StatementJSHelper)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementJSHelper
#define XPC_MAP_QUOTED_CLASSNAME "StatementJSHelper"
#define XPC_MAP_FLAGS \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementJSHelper::Resolve(nsIXPConnectWrappedNative* aWrapper, JSContext* aCtx,
                           JSObject* aScopeObj, jsid aId, bool* aResolvedp,
                           bool* _retval) {
  if (!JSID_IS_STRING(aId)) return NS_OK;

  JS::Rooted<JSObject*> scope(aCtx, aScopeObj);
  JS::Rooted<jsid> id(aCtx, aId);

#ifdef DEBUG
  {
    nsCOMPtr<mozIStorageStatement> isStatement(
        do_QueryInterface(aWrapper->Native()));
    NS_ASSERTION(isStatement, "How is this not a statement?!");
  }
#endif

  Statement* stmt = static_cast<Statement*>(
      static_cast<mozIStorageStatement*>(aWrapper->Native()));

  JSFlatString* str = JSID_TO_FLAT_STRING(id);
  if (::JS_FlatStringEqualsAscii(str, "step")) {
    *_retval = ::JS_DefineFunction(aCtx, scope, "step", stepFunc, 0,
                                   JSPROP_RESOLVING) != nullptr;
    *aResolvedp = true;
    return NS_OK;
  }

  JS::RootedValue val(aCtx);

  if (::JS_FlatStringEqualsAscii(str, "row")) {
    nsresult rv = getRow(stmt, aCtx, scope, val.address());
    NS_ENSURE_SUCCESS(rv, rv);
    *_retval = ::JS_DefinePropertyById(aCtx, scope, id, val, JSPROP_RESOLVING);
    *aResolvedp = true;
    return NS_OK;
  }

  if (::JS_FlatStringEqualsAscii(str, "params")) {
    nsresult rv = getParams(stmt, aCtx, scope, val.address());
    NS_ENSURE_SUCCESS(rv, rv);
    *_retval = ::JS_DefinePropertyById(aCtx, scope, id, val, JSPROP_RESOLVING);
    *aResolvedp = true;
    return NS_OK;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS0(StatementParamsHolder);

StatementParamsHolder::~StatementParamsHolder() {
  MOZ_ASSERT(NS_IsMainThread());
  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to the statement.
  mParams->mStatement = nullptr;
}

NS_IMPL_ISUPPORTS0(StatementRowHolder);

StatementRowHolder::~StatementRowHolder() {
  MOZ_ASSERT(NS_IsMainThread());
  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to the statement.
  mRow->mStatement = nullptr;
}

}  // namespace storage
}  // namespace mozilla
