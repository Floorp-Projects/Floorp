/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NimbusFeatures_h
#define mozilla_NimbusFeatures_h

#include "mozilla/Preferences.h"

namespace mozilla {

class NimbusFeatures {
  using PrefChangedFunc = void (*)(const char* aPref, void* aData);

 private:
  static void GetPrefName(const nsACString& aFeatureId,
                          const nsACString& aVariable, nsACString& aPref);

  static void PreferencesCallback(const char* aPref, void* aData);

 public:
  static bool GetBool(const nsACString& aFeatureId, const nsACString& aVariable,
                      bool aDefault);

  static int GetInt(const nsACString& aFeatureId, const nsACString& aVariable,
                    int aDefault);

  static nsresult OnUpdate(const nsACString& aFeatureId,
                           const nsACString& aVariable,
                           PrefChangedFunc aUserCallback);

  static nsresult OffUpdate(const nsACString& aFeatureId,
                            const nsACString& aVariable,
                            PrefChangedFunc aUserCallback);
};

}  // namespace mozilla

#endif
