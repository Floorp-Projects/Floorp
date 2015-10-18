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

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementJSHelper

nsresult
AsyncStatementJSHelper::getParams(AsyncStatement *aStatement,
                                  JSContext *aCtx,
                                  JSObject *aScopeObj,
                                  JS::Value *_params)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;

#ifdef DEBUG
  int32_t state;
  (void)aStatement->GetState(&state);
  NS_ASSERTION(state == mozIStorageAsyncStatement::MOZ_STORAGE_STATEMENT_READY,
               "Invalid state to get the params object - all calls will fail!");
#endif

  if (!aStatement->mStatementParamsHolder) {
    nsCOMPtr<mozIStorageStatementParams> params =
      new AsyncStatementParams(aStatement);
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);

    JS::RootedObject scope(aCtx, aScopeObj);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsCOMPtr<nsIXPConnect> xpc(Service::getXPConnect());
    rv = xpc->WrapNativeHolder(
      aCtx,
      ::JS_GetGlobalForObject(aCtx, scope),
      params,
      NS_GET_IID(mozIStorageStatementParams),
      getter_AddRefs(holder)
    );
    NS_ENSURE_SUCCESS(rv, rv);
    RefPtr<AsyncStatementParamsHolder> paramsHolder =
      new AsyncStatementParamsHolder(holder);
    aStatement->mStatementParamsHolder =
      new nsMainThreadPtrHolder<nsIXPConnectJSObjectHolder>(paramsHolder);
  }

  JS::Rooted<JSObject*> obj(aCtx);
  obj = aStatement->mStatementParamsHolder->GetJSObject();
  NS_ENSURE_STATE(obj);

  _params->setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementJSHelper::AddRef() { return 2; }
NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementJSHelper::Release() { return 1; }
NS_INTERFACE_MAP_BEGIN(AsyncStatementJSHelper)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME AsyncStatementJSHelper
#define XPC_MAP_QUOTED_CLASSNAME "AsyncStatementJSHelper"
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
AsyncStatementJSHelper::GetProperty(nsIXPConnectWrappedNative *aWrapper,
                                    JSContext *aCtx,
                                    JSObject *aScopeObj,
                                    jsid aId,
                                    JS::Value *_result,
                                    bool *_retval)
{
  if (!JSID_IS_STRING(aId))
    return NS_OK;

  // Cast to async via mozI* since direct from nsISupports is ambiguous.
  JS::RootedObject scope(aCtx, aScopeObj);
  JS::RootedId id(aCtx, aId);
  mozIStorageAsyncStatement *iAsyncStmt =
    static_cast<mozIStorageAsyncStatement *>(aWrapper->Native());
  AsyncStatement *stmt = static_cast<AsyncStatement *>(iAsyncStmt);

#ifdef DEBUG
  {
    nsISupports *supp = aWrapper->Native();
    nsCOMPtr<mozIStorageAsyncStatement> isStatement(do_QueryInterface(supp));
    NS_ASSERTION(isStatement, "How is this not an async statement?!");
  }
#endif

  if (::JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(id), "params"))
    return getParams(stmt, aCtx, scope, _result);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementParamsHolder

NS_IMPL_ISUPPORTS(AsyncStatementParamsHolder, nsIXPConnectJSObjectHolder);

JSObject*
AsyncStatementParamsHolder::GetJSObject()
{
  return mHolder->GetJSObject();
}

AsyncStatementParamsHolder::AsyncStatementParamsHolder(nsIXPConnectJSObjectHolder* aHolder)
  : mHolder(aHolder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mHolder);
}

AsyncStatementParamsHolder::~AsyncStatementParamsHolder()
{
  MOZ_ASSERT(NS_IsMainThread());
  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to the statement.
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper = do_QueryInterface(mHolder);
  nsCOMPtr<mozIStorageStatementParams> iObj = do_QueryWrappedNative(wrapper);
  AsyncStatementParams *obj = static_cast<AsyncStatementParams *>(iObj.get());
  obj->mStatement = nullptr;
}

} // namespace storage
} // namespace mozilla
