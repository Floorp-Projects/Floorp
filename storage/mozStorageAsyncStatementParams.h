/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_mozStorageAsyncStatementParams_h_
#define mozilla_storage_mozStorageAsyncStatementParams_h_

#include "mozilla/Attributes.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace storage {

class AsyncStatement;

class AsyncStatementParams final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AsyncStatementParams)

  explicit AsyncStatementParams(nsPIDOMWindowInner* aWindow,
                                AsyncStatement* aStatement);

  void NamedGetter(JSContext* aCx, const nsAString& aName, bool& aFound,
                   JS::MutableHandle<JS::Value> aResult,
                   mozilla::ErrorResult& aRv);

  void NamedSetter(JSContext* aCx, const nsAString& aName,
                   JS::Handle<JS::Value> aValue, mozilla::ErrorResult& aRv);

  uint32_t Length() const {
    // WebIDL requires a .length property when there's an indexed getter.
    // Unfortunately we don't know how many params there are in the async case,
    // so we have to lie.
    return UINT16_MAX;
  }

  void IndexedGetter(JSContext* aCx, uint32_t aIndex, bool& aFound,
                     JS::MutableHandle<JS::Value> aResult,
                     mozilla::ErrorResult& aRv);

  void IndexedSetter(JSContext* aCx, uint32_t aIndex,
                     JS::Handle<JS::Value> aValue, mozilla::ErrorResult& aRv);

  void GetSupportedNames(nsTArray<nsString>& aNames);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

 private:
  virtual ~AsyncStatementParams() {}

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  AsyncStatement* mStatement;

  friend class AsyncStatementParamsHolder;
};

}  // namespace storage
}  // namespace mozilla

#endif  // mozilla_storage_mozStorageAsyncStatementParams_h_
