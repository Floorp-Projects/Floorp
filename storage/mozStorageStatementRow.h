/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESTATEMENTROW_H
#define MOZSTORAGESTATEMENTROW_H

#include "mozilla/Attributes.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace storage {

class Statement;

class StatementRow final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(StatementRow)

  explicit StatementRow(nsPIDOMWindowInner* aWindow, Statement* aStatement);

  void NamedGetter(JSContext* aCx, const nsAString& aName, bool& aFound,
                   JS::MutableHandle<JS::Value> aResult,
                   mozilla::ErrorResult& aRv);
  void GetSupportedNames(nsTArray<nsString>& aNames);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

 private:
  ~StatementRow() {}

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  Statement* mStatement;

  friend class StatementRowHolder;
};

}  // namespace storage
}  // namespace mozilla

#endif /* MOZSTORAGESTATEMENTROW_H */
