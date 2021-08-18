/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/browser/NimbusFeatures.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsapi.h"
#include "js/JSON.h"
#include "nsJSUtils.h"

namespace mozilla {

static nsTHashSet<nsCString> sExposureFeatureSet;

void NimbusFeatures::GetPrefName(const nsACString& aFeatureId,
                                 const nsACString& aVariable,
                                 nsACString& aPref) {
  // This branch is used to store experiment data
  constexpr auto kSyncDataPrefBranch = "nimbus.syncdatastore."_ns;
  aPref.Truncate();
  aPref.Append(kSyncDataPrefBranch);
  aPref.Append(aFeatureId);
  if (!aVariable.IsEmpty()) {
    aPref.Append(".");
    aPref.Append(aVariable);
  }
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
                                  PrefChangedFunc aUserCallback,
                                  void* aUserData) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::RegisterCallback(aUserCallback, pref, aUserData);
}

nsresult NimbusFeatures::OffUpdate(const nsACString& aFeatureId,
                                   const nsACString& aVariable,
                                   PrefChangedFunc aUserCallback,
                                   void* aUserData) {
  nsAutoCString pref;
  GetPrefName(aFeatureId, aVariable, pref);
  return Preferences::UnregisterCallback(aUserCallback, pref, aUserData);
}

/**
 * Attempt to read Nimbus preference to determine experiment and branch slug.
 * Nimbus will store a pref with experiment metadata in the following format:
 * {
 *   slug: "experiment slug",
 *   branch: { slug: "branch slug" },
 *   ...
 * }
 * The naming convention for preference names is:
 * `nimbus.syncdatastore.<feature_id>`
 * These values are used to send `exposure` telemetry pings.
 */
nsresult NimbusFeatures::GetExperimentSlug(const nsACString& aFeatureId,
                                           nsACString& aExperimentSlug,
                                           nsACString& aBranchSlug) {
  nsAutoCString prefName;
  nsAutoString prefValue;

  aExperimentSlug.Truncate();
  aBranchSlug.Truncate();

  GetPrefName(aFeatureId, ""_ns, prefName);
  MOZ_TRY(Preferences::GetString(prefName.get(), prefValue));
  if (prefValue.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }
  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::PrivilegedJunkScope())) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> json(cx, JS::NullValue());
  if (JS_ParseJSON(cx, prefValue.BeginReading(), prefValue.Length(), &json) &&
      json.isObject()) {
    JS::Rooted<JSObject*> experimentJSON(cx, json.toObjectOrNull());
    JS::RootedValue expSlugValue(cx);
    if (!JS_GetProperty(cx, experimentJSON, "slug", &expSlugValue)) {
      return NS_ERROR_UNEXPECTED;
    }
    AssignJSString(cx, aExperimentSlug, expSlugValue.toString());

    JS::RootedValue branchJSON(cx);
    if (!JS_GetProperty(cx, experimentJSON, "branch", &branchJSON) &&
        !branchJSON.isObject()) {
      return NS_ERROR_UNEXPECTED;
    }
    JS::Rooted<JSObject*> branchObj(cx, branchJSON.toObjectOrNull());
    JS::RootedValue branchSlugValue(cx);
    if (!JS_GetProperty(cx, branchObj, "slug", &branchSlugValue)) {
      return NS_ERROR_UNEXPECTED;
    }
    AssignJSString(cx, aBranchSlug, branchSlugValue.toString());
  }

  return NS_OK;
}

/**
 * Sends an exposure event for aFeatureId when enrolled in an experiment.
 * By default we only attempt to send once. For some usecases it might be useful
 * to send multiple times or retry to send (when for example we are enrolled
 * after the first call to this function) in which case set the optional
 * aForce to `true`.
 */
nsresult NimbusFeatures::RecordExposureEvent(const nsACString& aFeatureId,
                                             const bool aForce) {
  nsAutoCString featureName(aFeatureId);
  if (!sExposureFeatureSet.EnsureInserted(featureName) && !aForce) {
    // We already sent (or tried to send) an exposure ping for this featureId
    return NS_ERROR_ABORT;
  }
  nsAutoCString slugName;
  nsAutoCString branchName;
  MOZ_TRY(GetExperimentSlug(aFeatureId, slugName, branchName));
  if (slugName.IsEmpty() || branchName.IsEmpty()) {
    // Failed getting experiment metadata or not enrolled in an experiment for
    // this featureId
    return NS_ERROR_UNEXPECTED;
  }
  Telemetry::SetEventRecordingEnabled("normandy"_ns, true);
  nsTArray<Telemetry::EventExtraEntry> extra(2);
  extra.AppendElement(Telemetry::EventExtraEntry{"branchSlug"_ns, branchName});
  extra.AppendElement(Telemetry::EventExtraEntry{"featureId"_ns, featureName});
  Telemetry::RecordEvent(Telemetry::EventID::Normandy_Expose_NimbusExperiment,
                         Some(slugName), Some(std::move(extra)));

  return NS_OK;
}

}  // namespace mozilla
