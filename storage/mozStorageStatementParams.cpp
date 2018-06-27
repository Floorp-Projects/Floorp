/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJSUtils.h"
#include "nsMemory.h"
#include "nsString.h"

#include "jsapi.h"

#include "mozilla/dom/MozStorageStatementParamsBinding.h"
#include "mozStoragePrivateHelpers.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementParams

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(StatementParams, mWindow)

NS_INTERFACE_TABLE_HEAD(StatementParams)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(StatementParams, nsISupports)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(StatementParams)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(StatementParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StatementParams)

StatementParams::StatementParams(nsPIDOMWindowInner* aWindow, Statement *aStatement)
: mWindow(aWindow),
  mStatement(aStatement),
  mParamCount(0)
{
  NS_ASSERTION(mStatement != nullptr, "mStatement is null");
  (void)mStatement->GetParameterCount(&mParamCount);
}

JSObject*
StatementParams::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::MozStorageStatementParams_Binding::Wrap(aCx, this, aGivenProto);
}

void
StatementParams::NamedGetter(JSContext* aCx,
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
StatementParams::NamedSetter(JSContext* aCx,
                             const nsAString& aName,
                             JS::Handle<JS::Value> aValue,
                             mozilla::ErrorResult& aRv)
{
  if (!mStatement) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  NS_ConvertUTF16toUTF8 name(aName);

  // Check to see if there's a parameter with this name.
  nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCx, aValue));
  if (!variant) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = mStatement->BindByName(name, variant);
}

void
StatementParams::GetSupportedNames(nsTArray<nsString>& aNames)
{
  if (!mStatement) {
    return;
  }

  for (uint32_t i = 0; i < mParamCount; i++) {
    // Get the name of our parameter.
    nsAutoCString name;
    nsresult rv = mStatement->GetParameterName(i, name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // But drop the first character, which is going to be a ':'.
    name = Substring(name, 1);
    aNames.AppendElement(NS_ConvertUTF8toUTF16(name));
  }
}

void
StatementParams::IndexedGetter(JSContext* aCx,
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
StatementParams::IndexedSetter(JSContext* aCx,
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
