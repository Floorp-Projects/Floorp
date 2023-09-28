/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSimpleEnumerator.h"

#include "mozilla/dom/IteratorResultBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

class JSEnumerator final : public nsIJSEnumerator {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSENUMERATOR

  explicit JSEnumerator(nsISimpleEnumerator* aEnumerator, const nsID& aIID)
      : mEnumerator(aEnumerator), mIID(aIID) {}

 private:
  ~JSEnumerator() = default;

  nsCOMPtr<nsISimpleEnumerator> mEnumerator;
  const nsID mIID;
};

}  // anonymous namespace

nsresult JSEnumerator::Iterator(nsIJSEnumerator** aResult) {
  RefPtr<JSEnumerator> result(this);
  result.forget(aResult);
  return NS_OK;
}

nsresult JSEnumerator::Next(JSContext* aCx, JS::MutableHandleValue aResult) {
  RootedDictionary<IteratorResult> result(aCx);

  nsCOMPtr<nsISupports> elem;
  if (NS_FAILED(mEnumerator->GetNext(getter_AddRefs(elem)))) {
    result.mDone = true;
  } else {
    result.mDone = false;

    JS::RootedValue value(aCx);
    MOZ_TRY(nsContentUtils::WrapNative(aCx, elem, &mIID, &value));
    result.mValue = value;
  }

  if (!ToJSValue(aCx, result, aResult)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(JSEnumerator, nsIJSEnumerator)

nsresult nsSimpleEnumerator::Iterator(nsIJSEnumerator** aResult) {
  auto result = MakeRefPtr<JSEnumerator>(this, DefaultInterface());
  result.forget(aResult);
  return NS_OK;
}

nsresult nsSimpleEnumerator::Entries(const nsIID& aIface,
                                     nsIJSEnumerator** aResult) {
  auto result = MakeRefPtr<JSEnumerator>(this, aIface);
  result.forget(aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsSimpleEnumerator, nsISimpleEnumerator,
                  nsISimpleEnumeratorBase)
