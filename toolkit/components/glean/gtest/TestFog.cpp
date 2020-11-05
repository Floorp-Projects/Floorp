/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/glean/GleanMetrics.h"

#include "mozilla/Preferences.h"
#include "nsString.h"

using mozilla::Preferences;

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

extern "C" void Rust_MeasureInitializeTime();
TEST(FOG, TestMeasureInitializeTime)
{ Rust_MeasureInitializeTime(); }

TEST(FOG, BuiltinPingsRegistered)
{
  Preferences::SetInt("telemetry.fog.test.localhost_port", -1);
  nsAutoCString metricsPingName("metrics");
  nsAutoCString baselinePingName("baseline");
  nsAutoCString eventsPingName("events");
  ASSERT_EQ(NS_OK, fog_submit_ping(&metricsPingName));
  // This will probably change to NS_OK once "duration" is implemented.
  ASSERT_EQ(NS_ERROR_NO_CONTENT, fog_submit_ping(&baselinePingName));
  ASSERT_EQ(NS_ERROR_NO_CONTENT, fog_submit_ping(&eventsPingName));
}

TEST(FOG, TestCppCounterWorks)
{
  mozilla::glean::test_only::bad_code.Add(42);

  ASSERT_TRUE(mozilla::glean::test_only::bad_code.TestHasValue("test-ping"));
  ASSERT_EQ(42, mozilla::glean::test_only::bad_code.TestGetValue("test-ping"));
}

TEST(FOG, TestCppTimespanWorks)
{
  mozilla::glean::test_only::can_we_time_it.Start();
  PR_Sleep(PR_MillisecondsToInterval(10));
  mozilla::glean::test_only::can_we_time_it.Stop();

  ASSERT_TRUE(
      mozilla::glean::test_only::can_we_time_it.TestHasValue("test-ping"));
  ASSERT_TRUE(
      mozilla::glean::test_only::can_we_time_it.TestGetValue("test-ping") > 0);
}

TEST(FOG, TestCppBooleanWorks)
{
  mozilla::glean::test_only::can_we_flag_it.Set(false);

  ASSERT_TRUE(
      mozilla::glean::test_only::can_we_flag_it.TestHasValue("test-ping"));
  ASSERT_EQ(false, mozilla::glean::test_only::can_we_flag_it.TestGetValue(
                       "test-ping"));
}
