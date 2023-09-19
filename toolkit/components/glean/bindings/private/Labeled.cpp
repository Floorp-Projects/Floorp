/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Labeled.h"

#include "mozilla/dom/GleanBinding.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/bindings/GleanJSMetricsLookup.h"
#include "mozilla/glean/bindings/MetricTypes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "nsString.h"

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(GleanLabeled)

NS_IMPL_CYCLE_COLLECTING_ADDREF(GleanLabeled)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GleanLabeled)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GleanLabeled)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* GleanLabeled::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanLabeled_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsISupports> GleanLabeled::NamedGetter(const nsAString& aName,
                                                        bool& aFound) {
  auto label = NS_ConvertUTF16toUTF8(aName);
  aFound = true;
  uint32_t submetricId = 0;
  already_AddRefed<nsISupports> submetric =
      NewSubMetricFromIds(mTypeId, mId, label, &submetricId);

  auto mirrorId = ScalarIdForMetric(mId);
  if (mirrorId) {
    GetLabeledMirrorLock().apply([&](auto& lock) {
      auto tuple = std::make_tuple<Telemetry::ScalarID, nsString>(
          mirrorId.extract(), nsString(aName));
      lock.ref()->InsertOrUpdate(submetricId, std::move(tuple));
    });
  }
  return submetric;
}

bool GleanLabeled::NameIsEnumerable(const nsAString& aName) { return false; }

void GleanLabeled::GetSupportedNames(nsTArray<nsString>& aNames) {
  // We really don't know, so don't do anything.
}

}  // namespace mozilla::glean
