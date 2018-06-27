/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJSUtils.h"
#include "nsMemory.h"
#include "nsString.h"

#include "jsapi.h"

#include "mozilla/dom/MozStorageAsyncStatementParamsBinding.h"
#include "mozStoragePrivateHelpers.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementParams

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AsyncStatementParams, mWindow)

NS_INTERFACE_TABLE_HEAD(AsyncStatementParams)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(AsyncStatementParams, nsISupports)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(AsyncStatementParams)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AsyncStatementParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AsyncStatementParams)

AsyncStatementParams::AsyncStatementParams(nsPIDOMWindowInner* aWindow, AsyncStatement *aStatement)
: mWindow(aWindow),
  mStatement(aStatement)
{
  NS_ASSERTION(mStatement != nullptr, "mStatement is null");
}

JSObject*
AsyncStatementParams::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::MozStorageAsyncStatementParams_Binding::Wrap(aCx, this, aGivenProto);
}

void
AsyncStatementParams::NamedGetter(JSContext* aCx,
                                  const nsAString& aName,
                                  bool& aFound,
                                  JS::MutableHandle<JS::Value> aResult,
                                  mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  // Unfortunately there's no API that lets us return the parameter value.
  aFound = false;
}

void
AsyncStatementParams::NamedSetter(JSContext* aCx,
                                  const nsAString& aName,
                                  JS::Handle<JS::Value> aValue,
                                  mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  NS_ConvertUTF16toUTF8 name(aName);

  nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCx, aValue));
  if (!variant) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = mStatement->BindByName(name, variant);
}

void
AsyncStatementParams::GetSupportedNames(nsTArray<nsString>& aNames)
{
  // We don't know how many params there are, so we can't implement this for
  // AsyncStatementParams.
}

void
AsyncStatementParams::IndexedGetter(JSContext* aCx,
                                    uint32_t aIndex,
                                    bool& aFound,
                                    JS::MutableHandle<JS::Value> aResult,
                                    mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  // Unfortunately there's no API that lets us return the parameter value.
  aFound = false;
}

void
AsyncStatementParams::IndexedSetter(JSContext* aCx,
                                    uint32_t aIndex,
                                    JS::Handle<JS::Value> aValue,
                                    mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCx, aValue));
  if (!variant) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = mStatement->BindByIndex(aIndex, variant);
}

} // namespace storage
} // namespace mozilla
