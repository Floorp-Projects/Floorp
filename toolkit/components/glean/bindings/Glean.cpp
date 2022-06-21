/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/GleanBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/glean/bindings/Glean.h"
#include "mozilla/glean/bindings/Category.h"
#include "mozilla/glean/bindings/GleanJSMetricsLookup.h"
#include "MainThreadUtils.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Glean)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Glean)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Glean)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Glean)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* Glean::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanImpl_Binding::Wrap(aCx, this, aGivenProto);
}

// static
bool Glean::DefineGlean(JSContext* aCx, JS::Handle<JSObject*> aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(JS::GetClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
             "Passed object is not a global object!");

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return false;
  }

  JS::Rooted<JS::Value> glean(aCx);
  js::AssertSameCompartment(aCx, aGlobal);

  auto impl = MakeRefPtr<Glean>();
  if (!dom::GetOrCreateDOMReflector(aCx, impl.get(), &glean)) {
    return false;
  }

  return JS_DefineProperty(aCx, aGlobal, "Glean", glean, JSPROP_ENUMERATE);
}

already_AddRefed<Category> Glean::NamedGetter(const nsAString& aName,
                                              bool& aFound) {
  Maybe<uint32_t> categoryIdx =
      CategoryByNameLookup(NS_ConvertUTF16toUTF8(aName));
  if (categoryIdx.isNothing()) {
    aFound = false;
    return nullptr;
  }

  aFound = true;
  nsDependentCString name(&gCategoryStringTable[categoryIdx.value()]);
  return MakeAndAddRef<Category>(std::move(name));
}

bool Glean::NameIsEnumerable(const nsAString& aName) { return false; }

void Glean::GetSupportedNames(nsTArray<nsString>& aNames) {
  for (category_entry_t entry : sCategoryByNameLookupEntries) {
    const char* categoryName = GetCategoryName(entry);
    aNames.AppendElement()->AssignASCII(categoryName);
  }
}

}  // namespace mozilla::glean
