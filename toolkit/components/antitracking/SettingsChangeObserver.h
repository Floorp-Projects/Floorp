/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_settingschangeobserver_h
#define mozilla_settingschangeobserver_h

#include "nsIObserver.h"

namespace mozilla {

class SettingsChangeObserver final : public nsIObserver {
  ~SettingsChangeObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // This API allows consumers to get notified when the anti-tracking component
  // settings change.  After this callback is called, an anti-tracking check
  // that has been previously performed with the same parameters may now return
  // a different result.
  using AntiTrackingSettingsChangedCallback = std::function<void()>;
  static void OnAntiTrackingSettingsChanged(
      const AntiTrackingSettingsChangedCallback& aCallback);

  static void PrivacyPrefChanged(const char* aPref = nullptr, void* = nullptr);

 private:
  static void RunAntiTrackingSettingsChangedCallbacks();
};

}  // namespace mozilla

#endif  // mozilla_settingschangeobserver_h
