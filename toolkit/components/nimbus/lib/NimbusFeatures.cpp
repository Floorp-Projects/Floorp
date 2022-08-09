/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/browser/NimbusFeatures.h"
#include "mozilla/browser/NimbusFeatureManifest.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsapi.h"
#include "js/JSON.h"
#include "nsJSUtils.h"

namespace mozilla {

static nsTHashSet<nsCString> sExposureFeatureSet;

void NimbusFeatures::GetPrefName(const nsACString& branchPrefix,
                                 const nsACString& aFeatureId,
                                 const nsACString& aVariable,
                                 nsACString& aPref) {
  nsAutoCString featureAndVariable;
  featureAndVariable.Append(aFeatureId);
  if (!aVariable.IsEmpty()) {
    featureAndVariable.Append(".");
    featureAndVariable.Append(aVariable);
  }
  aPref.Truncate();
  aPref.Append(branchPrefix);
  aPref.Append(featureAndVariable);
}

/**
 * Returns the variable value configured via experiment or rollout.
 * If a fallback pref is defined in the FeatureManifest and it
 * has a user value set this takes precedence over remote configurations.
 */
bool NimbusFeatures::GetBool(const nsACString& aFeatureId,
                             const nsACString& aVariable, bool aDefault) {
  nsAutoCString experimentPref;
  GetPrefName(kSyncDataPrefBranch, aFeatureId, aVariable, experimentPref);
  if (Preferences::HasUserValue(experimentPref.get())) {
    return Preferences::GetBool(experimentPref.get(), aDefault);
  }

  nsAutoCString rolloutPref;
  GetPrefName(kSyncRolloutsPrefBranch, aFeatureId, aVariable, rolloutPref);
  if (Preferences::HasUserValue(rolloutPref.get())) {
    return Preferences::GetBool(rolloutPref.get(), aDefault);
  }

  auto prefName = GetNimbusFallbackPrefName(aFeatureId, aVariable);
  if (prefName.isSome()) {
    return Preferences::GetBool(prefName->get(), aDefault);
  }
  return aDefault;
}

/**
 * Returns the variable value configured via experiment or rollout.
 * If a fallback pref is defined in the FeatureManifest and it
 * has a user value set this takes precedence over remote configurations.
 */
int NimbusFeatures::GetInt(const nsACString& aFeatureId,
                           const nsACString& aVariable, int aDefault) {
  nsAutoCString experimentPref;
  GetPrefName(kSyncDataPrefBranch, aFeatureId, aVariable, experimentPref);
  if (Preferences::HasUserValue(experimentPref.get())) {
    return Preferences::GetInt(experimentPref.get(), aDefault);
  }

  nsAutoCString rolloutPref;
  GetPrefName(kSyncRolloutsPrefBranch, aFeatureId, aVariable, rolloutPref);
  if (Preferences::HasUserValue(rolloutPref.get())) {
    return Preferences::GetInt(rolloutPref.get(), aDefault);
  }

  auto prefName = GetNimbusFallbackPrefName(aFeatureId, aVariable);
  if (prefName.isSome()) {
    return Preferences::GetInt(prefName->get(), aDefault);
  }
  return aDefault;
}

nsresult NimbusFeatures::OnUpdate(const nsACString& aFeatureId,
                                  const nsACString& aVariable,
                                  PrefChangedFunc aUserCallback,
                                  void* aUserData) {
  nsAutoCString experimentPref;
  nsAutoCString rolloutPref;
  GetPrefName(kSyncDataPrefBranch, aFeatureId, aVariable, experimentPref);
  GetPrefName(kSyncRolloutsPrefBranch, aFeatureId, aVariable, rolloutPref);
  nsresult rv =
      Preferences::RegisterCallback(aUserCallback, experimentPref, aUserData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = Preferences::RegisterCallback(aUserCallback, rolloutPref, aUserData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult NimbusFeatures::OffUpdate(const nsACString& aFeatureId,
                                   const nsACString& aVariable,
                                   PrefChangedFunc aUserCallback,
                                   void* aUserData) {
  nsAutoCString experimentPref;
  nsAutoCString rolloutPref;
  GetPrefName(kSyncDataPrefBranch, aFeatureId, aVariable, experimentPref);
  GetPrefName(kSyncRolloutsPrefBranch, aFeatureId, aVariable, rolloutPref);
  nsresult rv =
      Preferences::UnregisterCallback(aUserCallback, experimentPref, aUserData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = Preferences::UnregisterCallback(aUserCallback, rolloutPref, aUserData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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

  GetPrefName(kSyncDataPrefBranch, aFeatureId, EmptyCString(), prefName);
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
    JS::Rooted<JS::Value> expSlugValue(cx);
    if (!JS_GetProperty(cx, experimentJSON, "slug", &expSlugValue)) {
      return NS_ERROR_UNEXPECTED;
    }
    AssignJSString(cx, aExperimentSlug, expSlugValue.toString());

    JS::Rooted<JS::Value> branchJSON(cx);
    if (!JS_GetProperty(cx, experimentJSON, "branch", &branchJSON) &&
        !branchJSON.isObject()) {
      return NS_ERROR_UNEXPECTED;
    }
    JS::Rooted<JSObject*> branchObj(cx, branchJSON.toObjectOrNull());
    JS::Rooted<JS::Value> branchSlugValue(cx);
    if (!JS_GetProperty(cx, branchObj, "slug", &branchSlugValue)) {
      return NS_ERROR_UNEXPECTED;
    }
    AssignJSString(cx, aBranchSlug, branchSlugValue.toString());
  }

  return NS_OK;
}

/**
 * Sends an exposure event for aFeatureId when enrolled in an experiment.
 * By default attempt to send once per function call. For some usecases it might
 * be useful to send only once, in which case set the optional aOnce to `true`.
 */
nsresult NimbusFeatures::RecordExposureEvent(const nsACString& aFeatureId,
                                             const bool aOnce) {
  nsAutoCString featureName(aFeatureId);
  if (!sExposureFeatureSet.EnsureInserted(featureName) && aOnce) {
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
