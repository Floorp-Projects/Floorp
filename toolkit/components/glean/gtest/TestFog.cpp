/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/glean/GleanMetrics.h"

#include "mozilla/Preferences.h"
#include "nsString.h"
#include "prtime.h"

using mozilla::Preferences;
using namespace mozilla::glean;

#define DATA_PREF "datareporting.healthreport.uploadEnabled"

extern "C" {
// This function is called by the rust code in test.rs if a non-fatal test
// failure occurs.
void GTest_FOG_ExpectFailure(const char* aMessage) {
  EXPECT_STREQ(aMessage, "");
}

nsresult fog_init();
nsresult fog_submit_ping(const nsACString* aPingName);
}

// Initialize FOG exactly once.
// This needs to be the first test to run!
TEST(FOG, FogInitDoesntCrash)
{
  Preferences::SetInt("telemetry.fog.test.localhost_port", -1);
  ASSERT_EQ(NS_OK, fog_init());
  // Fog init isn't actually done (it passes work to a background thread)
  Preferences::SetBool(DATA_PREF, false);
  Preferences::SetBool(DATA_PREF, true);
}

// TODO: to be enabled after changes from bug 1677455 are vendored.
// extern "C" void Rust_MeasureInitializeTime();
// TEST(FOG, TestMeasureInitializeTime)
// { Rust_MeasureInitializeTime(); }

TEST(FOG, BuiltinPingsRegistered)
{
  Preferences::SetInt("telemetry.fog.test.localhost_port", -1);
  nsAutoCString metricsPingName("metrics");
  nsAutoCString baselinePingName("baseline");
  nsAutoCString eventsPingName("events");
  ASSERT_EQ(NS_OK, fog_submit_ping(&metricsPingName));
  ASSERT_EQ(NS_OK, fog_submit_ping(&baselinePingName));
  ASSERT_EQ(NS_OK, fog_submit_ping(&eventsPingName));
}

TEST(FOG, TestCppCounterWorks)
{
  mozilla::glean::test_only::bad_code.Add(42);

  ASSERT_EQ(
      42,
      mozilla::glean::test_only::bad_code.TestGetValue("test-ping").value());
}

TEST(FOG, TestCppStringWorks)
{
  auto kValue = "cheez!"_ns;
  mozilla::glean::test_only::cheesy_string.Set(kValue);

  ASSERT_STREQ(kValue.get(), mozilla::glean::test_only::cheesy_string
                                 .TestGetValue("test-ping")
                                 .value()
                                 .get());
}

// TODO: to be enabled after changes from bug 1677455 are vendored.
// TEST(FOG, TestCppTimespanWorks)
// {
//   mozilla::glean::test_only::can_we_time_it.Start();
//   PR_Sleep(PR_MillisecondsToInterval(10));
//   mozilla::glean::test_only::can_we_time_it.Stop();
//
//   ASSERT_TRUE(
//       mozilla::glean::test_only::can_we_time_it.TestGetValue("test-ping")
//           .value() > 0);
// }

TEST(FOG, TestCppUuidWorks)
{
  nsCString kTestUuid("decafdec-afde-cafd-ecaf-decafdecafde");
  test_only::what_id_it.Set(kTestUuid);
  ASSERT_STREQ(kTestUuid.get(),
               test_only::what_id_it.TestGetValue("test-ping").value().get());

  test_only::what_id_it.GenerateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  ASSERT_STRNE(kTestUuid.get(),
               test_only::what_id_it.TestGetValue("test-ping").value().get());
}

// TODO: to be enabled after changes from bug 1677448 are vendored.
// TEST(FOG, TestCppDatetimeWorks)
// {
//   PRExplodedTime date = {0, 35, 10, 12, 6, 10, 2020, 0, 0, {5 * 60 * 60, 0}};
//   test_only::what_a_date.Set(&date);
//
//   auto received = test_only::what_a_date.TestGetValue("test-ping");
//   ASSERT_STREQ(received.value().get(), "2020-11-06T12:10:35+05:00");
// }
