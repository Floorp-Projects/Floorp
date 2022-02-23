/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Preferences.h"
#include "mozilla/browser/NimbusFeatures.h"

using namespace mozilla;

static bool gPrefUpdate = false;

TEST(NimbusFeaturesGet, Errors)
{
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdatastore.foo.value", 42,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(NimbusFeatures::GetInt("foo"_ns, "value"_ns, 0), 42);
  ASSERT_EQ(Preferences::SetBool("nimbus.syncdatastore.foo.enabled", true,
                                 PrefValueKind::User),
            NS_OK);
  ASSERT_TRUE(NimbusFeatures::GetBool("foo"_ns, "enabled"_ns, false));
}

TEST(NimbusFeaturesGetRollout, Errors)
{
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdefaultsstore.rollout.value", 7,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(NimbusFeatures::GetInt("rollout"_ns, "value"_ns, 0), 7);
  ASSERT_EQ(Preferences::SetBool("nimbus.syncdefaultsstore.rollout.enabled",
                                 true, PrefValueKind::User),
            NS_OK);
  ASSERT_TRUE(NimbusFeatures::GetBool("rollout"_ns, "enabled"_ns, false));
}

TEST(NimbusFeaturesExperimentPriorityOverRollouts, Errors)
{
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdatastore.feature.value", 12,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdefaultsstore.feature.value", 22,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(NimbusFeatures::GetInt("feature"_ns, "value"_ns, 0), 12);
  ASSERT_EQ(Preferences::SetBool("nimbus.syncdatastore.feature.enabled", true,
                                 PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(Preferences::SetBool("nimbus.syncdefaultsstore.feature.enabled",
                                 false, PrefValueKind::User),
            NS_OK);
  ASSERT_TRUE(NimbusFeatures::GetBool("feature"_ns, "enabled"_ns, false));
}

// Make sure user prefs take predence over experiments and rollouts
TEST(NimbusFeaturesDataSourcePrecedence, Errors)
{
  ASSERT_EQ(
      Preferences::SetInt("nimbus.testing.testInt", 29, PrefValueKind::User),
      NS_OK);
  ASSERT_EQ(NimbusFeatures::GetInt("testFeature"_ns, "testInt"_ns, 0), 29);

  ASSERT_EQ(Preferences::SetInt("nimbus.syncdatastore.testFeature.testInt", 12,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdefaultsstore.testFeature.testInt",
                                13, PrefValueKind::User),
            NS_OK);
  // Still return user pref
  ASSERT_EQ(NimbusFeatures::GetInt("testFeature"_ns, "testInt"_ns, 0), 29);
  // After user prefs it should default to experiment value
  Preferences::ClearUser("nimbus.testing.testInt");
  ASSERT_EQ(NimbusFeatures::GetInt("testFeature"_ns, "testInt"_ns, 0), 12);
  Preferences::ClearUser("nimbus.syncdatastore.testFeature.testInt");
  // After experiments it should default to rollouts
  ASSERT_EQ(NimbusFeatures::GetInt("testFeature"_ns, "testInt"_ns, 0), 13);
  // Cleanup
  Preferences::ClearUser("nimbus.syncdefaultsstore.testFeature.testInt");
}

static void FooValueUpdated(const char* aPref, void* aUserData) {
  ASSERT_STREQ(aPref, "nimbus.syncdatastore.foo.value");
  ASSERT_EQ(aUserData, reinterpret_cast<void*>(13));

  ASSERT_FALSE(gPrefUpdate);
  gPrefUpdate = true;

  ASSERT_EQ(NimbusFeatures::GetInt("foo"_ns, "value"_ns, 0), 24);
}

static void BarRolloutValueUpdated(const char* aPref, void* aUserData) {
  ASSERT_STREQ(aPref, "nimbus.syncdefaultsstore.bar.value");

  ASSERT_FALSE(gPrefUpdate);
  gPrefUpdate = true;
}

TEST(NimbusFeaturesGetFallback, Errors)
{
  // No experiment is set and we expect to return fallback pref values

  // As defined by fallbackPref browser.aboutwelcome.skipFocus
  // in FeatureManifest.yaml
  Preferences::SetBool("browser.aboutwelcome.skipFocus", true,
                       PrefValueKind::Default);
  ASSERT_EQ(NimbusFeatures::GetBool("aboutwelcome"_ns, "skipFocus"_ns, false),
            true);
  Preferences::SetBool("browser.aboutwelcome.skipFocus", false,
                       PrefValueKind::User);
  ASSERT_EQ(NimbusFeatures::GetBool("aboutwelcome"_ns, "skipFocus"_ns, true),
            false);
  Preferences::ClearUser("browser.aboutwelcome.skipFocus");

  Preferences::SetInt("nimbus.testing.testInt", 5, PrefValueKind::Default);
  ASSERT_EQ(NimbusFeatures::GetInt("testFeature"_ns, "testInt"_ns, 42), 5);
}

TEST(NimbusFeaturesUpdate, Errors)
{
  // Verify updating foo.value calls FooValueUpdated.
  ASSERT_EQ(NimbusFeatures::OnUpdate("foo"_ns, "value"_ns, FooValueUpdated,
                                     reinterpret_cast<void*>(13)),
            NS_OK);
  ASSERT_EQ(
      NimbusFeatures::OnUpdate("bar"_ns, "value"_ns, BarRolloutValueUpdated,
                               reinterpret_cast<void*>(13)),
      NS_OK);
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdatastore.foo.value", 24,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_TRUE(gPrefUpdate);
  ASSERT_EQ(NimbusFeatures::GetInt("foo"_ns, "value"_ns, 0), 24);

  // Verify updating foo.enabled doesn't call FooValueUpdated.
  ASSERT_TRUE(NimbusFeatures::GetBool("foo"_ns, "enabled"_ns, false));
  ASSERT_EQ(Preferences::SetBool("nimbus.syncdatastore.foo.enabled", false,
                                 PrefValueKind::User),
            NS_OK);
  ASSERT_FALSE(NimbusFeatures::GetBool("foo"_ns, "enabled"_ns, true));
  gPrefUpdate = false;

  ASSERT_EQ(Preferences::SetInt("nimbus.syncdefaultsstore.bar.value", 25,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_TRUE(gPrefUpdate);
  gPrefUpdate = false;

  // Verify OffUpdate requires a matching user data pointer to unregister.
  ASSERT_EQ(NimbusFeatures::OffUpdate("foo"_ns, "value"_ns, FooValueUpdated,
                                      reinterpret_cast<void*>(14)),
            NS_ERROR_FAILURE);

  // Verify updating foo.value no longer calls FooValueUpdated after it has
  // been unregistered.
  ASSERT_EQ(NimbusFeatures::OffUpdate("foo"_ns, "value"_ns, FooValueUpdated,
                                      reinterpret_cast<void*>(13)),
            NS_OK);
  ASSERT_EQ(Preferences::SetInt("nimbus.syncdatastore.foo.value", 25,
                                PrefValueKind::User),
            NS_OK);
  ASSERT_EQ(NimbusFeatures::GetInt("foo"_ns, "value"_ns, 0), 25);
}
