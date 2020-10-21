/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/Glean.h"
#include "mozilla/glean/Category.h"
#include "mozilla/dom/GleanBinding.h"

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Category)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Category)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Category)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Category)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* Category::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanCategory_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsISupports> Category::NamedGetter(const nsAString& aName,
                                                    bool& aFound) {
  aFound = false;
  return nullptr;
}

bool Category::NameIsEnumerable(const nsAString& aName) { return false; }

void Category::GetSupportedNames(nsTArray<nsString>& aNames) {}

}  // namespace mozilla::glean
