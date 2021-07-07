/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/browser/NimbusFeatures.h"

namespace mozilla {

void NimbusFeatures::GetPrefName(const nsACString& aFeatureId,
                                 const nsACString& aVariable,
                                 nsACString& aPref) {
  // This branch is used to store experiment data
  constexpr auto kSyncDataPrefBranch = "nimbus.syncdatastore."_ns;
  aPref.Truncate();
  aPref.Append(kSyncDataPrefBranch);
  aPref.Append(aFeatureId);
  aPref.Append(".");
  aPref.Append(aVariable);
}

void NimbusFeatures::PreferencesCallback(const char* aPref, void* aData) {
  PrefChangedFunc cb = reinterpret_cast<PrefChangedFunc>(aData);
  (*cb)(aPref, nullptr);
}

bool NimbusFeatures::GetBool(const nsACString& aFeatureId,
                             const nsACString& aVariable, bool aDefault) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::GetBool(pref.get(), aDefault);
}

int NimbusFeatures::GetInt(const nsACString& aFeatureId,
                           const nsACString& aVariable, int aDefault) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::GetInt(pref.get(), aDefault);
}

nsresult NimbusFeatures::OnUpdate(const nsACString& aFeatureId,
                                  const nsACString& aVariable,
                                  PrefChangedFunc aUserCallback) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::RegisterCallback(PreferencesCallback, pref,
                                       (void*)aUserCallback);
}

nsresult NimbusFeatures::OffUpdate(const nsACString& aFeatureId,
                                   const nsACString& aVariable,
                                   PrefChangedFunc aUserCallback) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::UnregisterCallback(
      PreferencesCallback, pref, reinterpret_cast<void*>(aUserCallback));
}

}  // namespace mozilla
