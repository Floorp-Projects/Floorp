/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Preferences.h"
#include "mozilla/browser/NimbusFeatures.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/TypeDecls.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

class NimbusTelemetryFixture : public TelemetryTestFixture {};

TEST_F(NimbusTelemetryFixture, NimbusFeaturesTelemetry) {
  constexpr auto prefName = "nimbus.syncdatastore.foo"_ns;
  constexpr auto prefValue =
      R"({"slug":"experiment-slug","branch":{"slug":"branch-slug"}})";
  AutoJSContextWithGlobal cx(mCleanGlobal);
  Unused << mTelemetry->ClearEvents();

  ASSERT_EQ(NimbusFeatures::RecordExposureEvent("foo"_ns), NS_ERROR_UNEXPECTED)
      << "Should fail because not enrolled in experiment";
  // Set the experiment info for `foo`
  Preferences::SetCString(prefName.get(), prefValue);
  ASSERT_EQ(NimbusFeatures::RecordExposureEvent("foo"_ns), NS_OK)
      << "Should work for the 2nd call";
  ASSERT_EQ(NimbusFeatures::RecordExposureEvent("foo"_ns, true), NS_ERROR_ABORT)
      << "Should abort because we've sent exposure and aOnce is true";
  ASSERT_EQ(NimbusFeatures::RecordExposureEvent("bar"_ns), NS_ERROR_UNEXPECTED)
      << "Should fail because we don't have an experiment for bar";

  JS::Rooted<JS::Value> eventsSnapshot(cx.GetJSContext());
  GetEventSnapshot(cx.GetJSContext(), &eventsSnapshot);
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, "normandy"_ns,
                           "expose"_ns, "nimbus_experiment"_ns));
}
