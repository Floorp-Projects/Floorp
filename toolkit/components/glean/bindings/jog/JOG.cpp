/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/jog/JOG.h"

#include <locale>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Tuple.h"
#include "nsThreadUtils.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"

namespace mozilla::glean {

// Storage
// Thread Safety: Only used on the main thread.
StaticAutoPtr<nsTHashSet<nsCString>> gCategories;
StaticAutoPtr<nsTHashMap<nsCString, uint32_t>> gMetrics;

// static
bool JOG::HasCategory(const nsACString& aCategoryName) {
  MOZ_ASSERT(NS_IsMainThread());

  return gCategories && gCategories->Contains(aCategoryName);
}

// static
bool JOG::EnsureRuntimeMetricsRegistered(bool aForce) {
  return false;  // TODO: implement.
}

// static
bool JOG::AreRuntimeMetricsComprehensive() {
  return false;  // TODO: implement.
}

// static
void JOG::GetCategoryNames(nsTArray<nsString>& aNames) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gCategories) {
    return;
  }
  for (const auto& category : *gCategories) {
    aNames.EmplaceBack(NS_ConvertUTF8toUTF16(category));
  }
}

// static
Maybe<uint32_t> JOG::GetMetric(const nsACString& aMetricName) {
  MOZ_ASSERT(NS_IsMainThread());
  return !gMetrics ? Nothing() : gMetrics->MaybeGet(aMetricName);
}

}  // namespace mozilla::glean

// static
nsCString dottedSnakeToCamel(const nsACString& aSnake) {
  nsCString camel;
  bool first = true;
  for (const nsACString& segment : aSnake.Split('_')) {
    for (const nsACString& part : segment.Split('.')) {
      if (first) {
        first = false;
        camel.Append(part);
      } else if (part.Length()) {
        char lower = part.CharAt(0);
        if ('a' <= lower && lower <= 'z') {
          camel.Append(
              std::toupper(lower, std::locale()));  // append the Capital.
          camel.Append(part.BeginReading() + 1,
                       part.Length() - 1);  // append the rest.
        } else {
          // Not gonna try to capitalize anything outside a->z.
          camel.Append(part);
        }
      }
    }
  }
  return camel;
}

using mozilla::AppShutdown;
using mozilla::ShutdownPhase;
using mozilla::glean::gCategories;
using mozilla::glean::gMetrics;

extern "C" NS_EXPORT void JOG_RegisterMetric(const nsACString& aCategory,
                                             const nsACString& aName,
                                             uint32_t aMetric) {
  MOZ_ASSERT(NS_IsMainThread());

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
    return;
  }

  // aCategory is dotted.snake_case. aName is snake_case.
  auto categoryCamel = dottedSnakeToCamel(aCategory);
  auto nameCamel = dottedSnakeToCamel(aName);

  // Register the category
  if (!gCategories) {
    gCategories = new nsTHashSet<nsCString>();
    RunOnShutdown([&] { gCategories = nullptr; },
                  ShutdownPhase::XPCOMWillShutdown);
  }
  gCategories->Insert(categoryCamel);

  // Register the metric
  if (!gMetrics) {
    gMetrics = new nsTHashMap<nsCString, uint32_t>();
    RunOnShutdown([&] { gMetrics = nullptr; },
                  ShutdownPhase::XPCOMWillShutdown);
  }
  gMetrics->InsertOrUpdate(categoryCamel + "."_ns + nameCamel, aMetric);
}
