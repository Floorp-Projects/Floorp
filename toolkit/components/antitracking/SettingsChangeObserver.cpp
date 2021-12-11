/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SettingsChangeObserver.h"
#include "ContentBlockingUserInteraction.h"

#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"
#include "nsIPermission.h"
#include "nsTArray.h"

using namespace mozilla;

namespace {

UniquePtr<nsTArray<SettingsChangeObserver::AntiTrackingSettingsChangedCallback>>
    gSettingsChangedCallbacks;

}

NS_IMPL_ISUPPORTS(SettingsChangeObserver, nsIObserver)

NS_IMETHODIMP SettingsChangeObserver::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "perm-added");
      obs->RemoveObserver(this, "perm-changed");
      obs->RemoveObserver(this, "perm-cleared");
      obs->RemoveObserver(this, "perm-deleted");
      obs->RemoveObserver(this, "xpcom-shutdown");

      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged,
          "browser.contentblocking.");
      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged, "network.cookie.");
      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged, "privacy.");

      gSettingsChangedCallbacks = nullptr;
    }
  } else {
    nsCOMPtr<nsIPermission> perm = do_QueryInterface(aSubject);
    if (perm) {
      nsAutoCString type;
      nsresult rv = perm->GetType(type);
      if (NS_WARN_IF(NS_FAILED(rv)) || type.Equals(USER_INTERACTION_PERM)) {
        // Ignore failures or notifications that have been sent because of
        // user interactions.
        return NS_OK;
      }
    }

    RunAntiTrackingSettingsChangedCallbacks();
  }

  return NS_OK;
}

// static
void SettingsChangeObserver::PrivacyPrefChanged(const char* aPref,
                                                void* aClosure) {
  RunAntiTrackingSettingsChangedCallbacks();
}

// static
void SettingsChangeObserver::RunAntiTrackingSettingsChangedCallbacks() {
  if (gSettingsChangedCallbacks) {
    for (auto& callback : *gSettingsChangedCallbacks) {
      callback();
    }
  }
}

// static
void SettingsChangeObserver::OnAntiTrackingSettingsChanged(
    const SettingsChangeObserver::AntiTrackingSettingsChangedCallback&
        aCallback) {
  static bool initialized = false;
  if (!initialized) {
    // It is possible that while we have some data in our cache, something
    // changes in our environment that causes the anti-tracking checks below to
    // change their response.  Therefore, we need to clear our cache when we
    // detect a related change.
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "browser.contentblocking.");
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "network.cookie.");
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "privacy.");

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      RefPtr<SettingsChangeObserver> observer = new SettingsChangeObserver();
      obs->AddObserver(observer, "perm-added", false);
      obs->AddObserver(observer, "perm-changed", false);
      obs->AddObserver(observer, "perm-cleared", false);
      obs->AddObserver(observer, "perm-deleted", false);
      obs->AddObserver(observer, "xpcom-shutdown", false);
    }

    gSettingsChangedCallbacks =
        MakeUnique<nsTArray<AntiTrackingSettingsChangedCallback>>();

    initialized = true;
  }

  gSettingsChangedCallbacks->AppendElement(aCallback);
}
