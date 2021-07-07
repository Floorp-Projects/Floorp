/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Preferences.h"
#include "mozilla/browser/NimbusFeatures.h"
#include "mozilla/SpinEventLoopUntil.h"

using namespace mozilla;

static bool gPrefUpdate = false;

TEST(NimbusFeaturesGet, Errors)
{
  Preferences::SetInt("nimbus.syncdatastore.foo.value", 42,
                      PrefValueKind::User);
  ASSERT_EQ(NimbusFeatures::GetInt("foo"_ns, "value"_ns, 0), 42);
  Preferences::SetBool("nimbus.syncdatastore.foo.enabled", true,
                       PrefValueKind::User);
  ASSERT_TRUE(NimbusFeatures::GetBool("foo"_ns, "enabled"_ns, false));
}

TEST(NimbusFeaturesUpdate, Errors)
{
  NimbusFeatures::OnUpdate("foo"_ns, "value"_ns,
                           [](const char*, void*) { gPrefUpdate = true; });
  Preferences::SetInt("nimbus.syncdatastore.foo.value", 24,
                      PrefValueKind::User);
  ASSERT_TRUE(gPrefUpdate);
}
