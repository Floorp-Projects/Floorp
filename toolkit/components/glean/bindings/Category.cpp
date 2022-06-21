/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GleanBinding.h"
#include "mozilla/glean/bindings/Glean.h"
#include "mozilla/glean/bindings/Category.h"
#include "mozilla/glean/bindings/GleanJSMetricsLookup.h"

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

  nsCString metricName;
  metricName.AppendASCII(mName);
  metricName.AppendLiteral(".");
  AppendUTF16toUTF8(aName, metricName);

  Maybe<uint32_t> metricIdx = MetricByNameLookup(metricName);

  if (metricIdx.isNothing()) {
    aFound = false;
    return nullptr;
  }

  aFound = true;
  return NewMetricFromId(metricIdx.value());
}

bool Category::NameIsEnumerable(const nsAString& aName) { return false; }

void Category::GetSupportedNames(nsTArray<nsString>& aNames) {
  for (metric_entry_t entry : sMetricByNameLookupEntries) {
    const char* identifierBuf = GetMetricIdentifier(entry);
    nsDependentCString identifier(identifierBuf);

    // We're iterating all metrics,
    // so we need to check for the ones in the right category.
    //
    // We need to ensure that we found _only_ the exact category by checking it
    // is followed by a dot.
    if (identifier.Find(mName, false, 0, 1) == 0 &&
        identifier.CharAt(mName.Length()) == '.') {
      const char* metricName = &identifierBuf[mName.Length() + 1];
      aNames.AppendElement()->AssignASCII(metricName);
    }
  }
}

}  // namespace mozilla::glean
