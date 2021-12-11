/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Category_h
#define mozilla_glean_Category_h

#include "js/TypeDecls.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

namespace mozilla::glean {

class Category final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Category)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return nullptr; }

  Category(uint32_t id, uint32_t length) : mId(id), mLength(length) {}

  already_AddRefed<nsISupports> NamedGetter(const nsAString& aName,
                                            bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(nsTArray<nsString>& aNames);

 private:
  uint32_t mId;
  uint32_t mLength;

 protected:
  virtual ~Category() = default;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_Category_h */
