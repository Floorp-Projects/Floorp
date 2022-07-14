/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIXPConnect.h"
#include "mozStorageAsyncStatement.h"
#include "mozStorageService.h"

#include "nsMemory.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"

#include "mozStorageAsyncStatementJSHelper.h"

#include "mozStorageAsyncStatementParams.h"

#include "jsapi.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById

#include "xpc_make_class.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementJSHelper

nsresult AsyncStatementJSHelper::getParams(AsyncStatement* aStatement,
                                           JSContext* aCtx, JSObject* aScopeObj,
                                           JS::Value* _params) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageAsyncStatement::MOZ_STORAGE_STATEMENT_READY,
               "Invalid state to get the params object - all calls will fail!");
#endif

  JS::Rooted<JSObject*> scope(aCtx, aScopeObj);

  if (!aStatement->mStatementParamsHolder) {
    dom::GlobalObject global(aCtx, scope);
    if (global.Failed()) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(global.GetAsSupports());

    RefPtr<AsyncStatementParams> params(
        new AsyncStatementParams(window, aStatement));
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);

    RefPtr<AsyncStatementParamsHolder> paramsHolder =
        new AsyncStatementParamsHolder(params);
    NS_ENSURE_TRUE(paramsHolder, NS_ERROR_OUT_OF_MEMORY);

    aStatement->mStatementParamsHolder =
        new nsMainThreadPtrHolder<AsyncStatementParamsHolder>(
            "Statement::mStatementParamsHolder", paramsHolder);
  }

  RefPtr<AsyncStatementParams> params(
      aStatement->mStatementParamsHolder->Get());
  JSObject* obj = params->WrapObject(aCtx, nullptr);
  if (!obj) {
    return NS_ERROR_UNEXPECTED;
  }

  _params->setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementJSHelper::AddRef() {
  return 2;
}
NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementJSHelper::Release() {
  return 1;
}
NS_INTERFACE_MAP_BEGIN(AsyncStatementJSHelper)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME AsyncStatementJSHelper
#define XPC_MAP_QUOTED_CLASSNAME "AsyncStatementJSHelper"
#define XPC_MAP_FLAGS \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h"

NS_IMETHODIMP
AsyncStatementJSHelper::Resolve(nsIXPConnectWrappedNative* aWrapper,
                                JSContext* aCtx, JSObject* aScopeObj, jsid aId,
                                bool* resolvedp, bool* _retval) {
  if (!aId.isString()) return NS_OK;

  // Cast to async via mozI* since direct from nsISupports is ambiguous.
  JS::Rooted<JSObject*> scope(aCtx, aScopeObj);
  JS::Rooted<JS::PropertyKey> id(aCtx, aId);
  mozIStorageAsyncStatement* iAsyncStmt =
      static_cast<mozIStorageAsyncStatement*>(aWrapper->Native());
  AsyncStatement* stmt = static_cast<AsyncStatement*>(iAsyncStmt);

#ifdef DEBUG
  {
    nsISupports* supp = aWrapper->Native();
    nsCOMPtr<mozIStorageAsyncStatement> isStatement(do_QueryInterface(supp));
    NS_ASSERTION(isStatement, "How is this not an async statement?!");
  }
#endif

  if (::JS_LinearStringEqualsLiteral(id.toLinearString(), "params")) {
    JS::Rooted<JS::Value> val(aCtx);
    nsresult rv = getParams(stmt, aCtx, scope, val.address());
    NS_ENSURE_SUCCESS(rv, rv);
    *_retval = ::JS_DefinePropertyById(aCtx, scope, id, val, JSPROP_RESOLVING);
    *resolvedp = true;
    return NS_OK;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementParamsHolder

NS_IMPL_ISUPPORTS0(AsyncStatementParamsHolder);

AsyncStatementParamsHolder::~AsyncStatementParamsHolder() {
  MOZ_ASSERT(NS_IsMainThread());
  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to the statement.
  mParams->mStatement = nullptr;
}

}  // namespace storage
}  // namespace mozilla
