/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJSUtils.h"
#include "nsMemory.h"
#include "nsString.h"

#include "jsapi.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementParams.h"
#include "mozIStorageStatement.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementParams

StatementParams::StatementParams(mozIStorageStatement *aStatement) :
    mStatement(aStatement),
    mParamCount(0)
{
  NS_ASSERTION(mStatement != nullptr, "mStatement is null");
  (void)mStatement->GetParameterCount(&mParamCount);
}

NS_IMPL_ISUPPORTS(
  StatementParams,
  mozIStorageStatementParams,
  nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementParams
#define XPC_MAP_QUOTED_CLASSNAME "StatementParams"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_WANT_RESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementParams::SetProperty(nsIXPConnectWrappedNative *aWrapper,
                             JSContext *aCtx,
                             JSObject *aScopeObj,
                             jsid aId,
                             JS::Value *_vp,
                             bool *_retval)
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
    nsAutoJSString autoStr;
    if (!autoStr.init(aCtx, str)) {
      return NS_ERROR_FAILURE;
    }

    NS_ConvertUTF16toUTF8 name(autoStr);

    // check to see if there's a parameter with this name
    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByName(name, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
StatementParams::NewEnumerate(nsIXPConnectWrappedNative *aWrapper,
                              JSContext *aCtx,
                              JSObject *aScopeObj,
                              JS::AutoIdVector &aProperties,
                              bool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  JS::RootedObject scope(aCtx, aScopeObj);

  if (!aProperties.reserve(mParamCount)) {
    *_retval = false;
    return NS_OK;
  }

  for (uint32_t i = 0; i < mParamCount; i++) {
    // Get the name of our parameter.
    nsAutoCString name;
    nsresult rv = mStatement->GetParameterName(i, name);
    NS_ENSURE_SUCCESS(rv, rv);

    // But drop the first character, which is going to be a ':'.
    JS::RootedString jsname(aCtx, ::JS_NewStringCopyN(aCtx, &(name.get()[1]),
                                                      name.Length() - 1));
    NS_ENSURE_TRUE(jsname, NS_ERROR_OUT_OF_MEMORY);

    // Set our name.
    JS::Rooted<jsid> id(aCtx);
    if (!::JS_StringToId(aCtx, jsname, &id)) {
      *_retval = false;
      return NS_OK;
    }

    aProperties.infallibleAppend(id);
  }

  return NS_OK;
}

NS_IMETHODIMP
StatementParams::Resolve(nsIXPConnectWrappedNative *aWrapper,
                         JSContext *aCtx,
                         JSObject *aScopeObj,
                         jsid aId,
                         bool *resolvedp,
                         bool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  // We do not throw at any point after this unless our index is out of range
  // because we want to allow the prototype chain to be checked for the
  // property.

  JS::RootedObject scope(aCtx, aScopeObj);
  JS::RootedId id(aCtx, aId);
  bool resolved = false;
  bool ok = true;
  if (JSID_IS_INT(id)) {
    uint32_t idx = JSID_TO_INT(id);

    // Ensure that our index is within range.  We do not care about the
    // prototype chain being checked here.
    if (idx >= mParamCount)
      return NS_ERROR_INVALID_ARG;

    ok = ::JS_DefineElement(aCtx, scope, idx, JS::UndefinedHandleValue,
                            JSPROP_ENUMERATE | JSPROP_RESOLVING);
    resolved = true;
  }
  else if (JSID_IS_STRING(id)) {
    JSString *str = JSID_TO_STRING(id);
    nsAutoJSString autoStr;
    if (!autoStr.init(aCtx, str)) {
      return NS_ERROR_FAILURE;
    }

    // Check to see if there's a parameter with this name, and if not, let
    // the rest of the prototype chain be checked.
    NS_ConvertUTF16toUTF8 name(autoStr);
    uint32_t idx;
    nsresult rv = mStatement->GetParameterIndex(name, &idx);
    if (NS_SUCCEEDED(rv)) {
      ok = ::JS_DefinePropertyById(aCtx, scope, id, JS::UndefinedHandleValue,
                                   JSPROP_ENUMERATE | JSPROP_RESOLVING);
      resolved = true;
    }
  }

  *_retval = ok;
  *resolvedp = resolved && ok;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
