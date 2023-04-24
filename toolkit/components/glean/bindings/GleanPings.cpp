/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/GleanPings.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/GleanPingsBinding.h"
#include "mozilla/glean/bindings/GleanJSPingsLookup.h"
#include "mozilla/glean/bindings/jog/JOG.h"
#include "mozilla/glean/bindings/Ping.h"
#include "MainThreadUtils.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(GleanPings)

NS_IMPL_CYCLE_COLLECTING_ADDREF(GleanPings)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GleanPings)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GleanPings)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* GleanPings::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanPingsImpl_Binding::Wrap(aCx, this, aGivenProto);
}

// static
bool GleanPings::DefineGleanPings(JSContext* aCx,
                                  JS::Handle<JSObject*> aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(JS::GetClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
             "Passed object is not a global object!");

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return false;
  }

  JS::Rooted<JS::Value> gleanPings(aCx);
  js::AssertSameCompartment(aCx, aGlobal);

  auto impl = MakeRefPtr<GleanPings>();
  if (!dom::GetOrCreateDOMReflector(aCx, impl.get(), &gleanPings)) {
    return false;
  }

  return JS_DefineProperty(aCx, aGlobal, "GleanPings", gleanPings,
                           JSPROP_ENUMERATE);
}

already_AddRefed<GleanPing> GleanPings::NamedGetter(const nsAString& aName,
                                                    bool& aFound) {
  aFound = false;

  NS_ConvertUTF16toUTF8 pingName(aName);

  JOG::EnsureRuntimeMetricsRegistered();

  Maybe<uint32_t> pingId = JOG::GetPing(pingName);
  if (pingId.isNothing() && !JOG::AreRuntimeMetricsComprehensive()) {
    pingId = PingByNameLookup(pingName);
  }

  if (pingId.isNothing()) {
    aFound = false;
    return nullptr;
  }

  aFound = true;
  return MakeAndAddRef<GleanPing>(pingId.value());
}

bool GleanPings::NameIsEnumerable(const nsAString& aName) { return false; }

void GleanPings::GetSupportedNames(nsTArray<nsString>& aNames) {
  JOG::GetPingNames(aNames);
  if (!JOG::AreRuntimeMetricsComprehensive()) {
    for (uint8_t idx : sPingByNameLookupEntries) {
      const char* pingName = GetPingName(idx);
      aNames.AppendElement()->AssignASCII(pingName);
    }
  }
}

}  // namespace mozilla::glean
