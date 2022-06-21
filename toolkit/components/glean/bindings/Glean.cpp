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
#include "mozilla/glean/bindings/jog/jog_ffi_generated.h"
#include "mozilla/glean/bindings/jog/JOG.h"
#include "MainThreadUtils.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::glean {

// Whether the runtime-registered metrics should be treated as comprehensive,
// or additive. If comprehensive, a metric not registered at runtime is a
// metric that doesn't exist. If additive, a metric not registered at runtime
// may still exist if it was registered at compile time.
// If we're supporting Artefact Builds, we treat them as comprehensive.
// Threading: Must only be read or written to on the main thread.
static bool gRuntimeMetricsComprehensive = false;

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
  MOZ_ASSERT(NS_IsMainThread());
#ifndef MOZILLA_OFFICIAL
  // We only entertain the idea this might be an artefact build if
  // !MOZILLA_OFFICAL to avoid doing _main thread I/O_ (dun dun dunnn) on
  // important builds.
  static bool sRuntimeRegistrarRan = false;
  if (!sRuntimeRegistrarRan) {
    sRuntimeRegistrarRan = true;

    // Run the runtime metrics registrar.
    gRuntimeMetricsComprehensive = jog::jog_runtime_registrar();
  }
#endif  // MOZILLA_OFFICIAL

  NS_ConvertUTF16toUTF8 categoryName(aName);
  if (JOG::HasCategory(categoryName)) {
    aFound = true;
    return MakeAndAddRef<Category>(std::move(categoryName));
  }

  if (gRuntimeMetricsComprehensive) {
    // This category might be built-in, but since the runtime metrics are
    // comprehensive, that just signals that the category was removed locally.
    aFound = false;
    return nullptr;
  }

  Maybe<uint32_t> categoryIdx = CategoryByNameLookup(categoryName);
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

// static
void Glean::TestSetRuntimeMetricsComprehensive(bool aIsComprehensive) {
  MOZ_ASSERT(NS_IsMainThread());
  gRuntimeMetricsComprehensive = aIsComprehensive;
}

}  // namespace mozilla::glean
