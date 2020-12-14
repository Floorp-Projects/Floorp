/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/glean/GleanPings.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/Maybe.h"
#include "mozilla/Tuple.h"
#include "nsTArray.h"

#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsString.h"
#include "prtime.h"

using mozilla::Preferences;
using namespace mozilla::glean;
using namespace mozilla::glean::impl;

#define DATA_PREF "datareporting.healthreport.uploadEnabled"

extern "C" {
// This function is called by the rust code in test.rs if a non-fatal test
// failure occurs.
void GTest_FOG_ExpectFailure(const char* aMessage) {
  EXPECT_STREQ(aMessage, "");
}
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
      mozilla::glean::test_only::bad_code.TestGetValue("test-ping"_ns).value());
}

TEST(FOG, TestCppStringWorks)
{
  auto kValue = "cheez!"_ns;
  mozilla::glean::test_only::cheesy_string.Set(kValue);

  ASSERT_STREQ(kValue.get(), mozilla::glean::test_only::cheesy_string
                                 .TestGetValue("test-ping"_ns)
                                 .value()
                                 .get());
}

TEST(FOG, TestCppTimespanWorks)
{
  mozilla::glean::test_only::can_we_time_it.Start();
  PR_Sleep(PR_MillisecondsToInterval(10));
  mozilla::glean::test_only::can_we_time_it.Stop();

  ASSERT_TRUE(
      mozilla::glean::test_only::can_we_time_it.TestGetValue("test-ping"_ns)
          .value() > 0);
}

TEST(FOG, TestCppUuidWorks)
{
  nsCString kTestUuid("decafdec-afde-cafd-ecaf-decafdecafde");
  test_only::what_id_it.Set(kTestUuid);
  ASSERT_STREQ(
      kTestUuid.get(),
      test_only::what_id_it.TestGetValue("test-ping"_ns).value().get());

  test_only::what_id_it.GenerateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  ASSERT_STRNE(
      kTestUuid.get(),
      test_only::what_id_it.TestGetValue("test-ping"_ns).value().get());
}

TEST(FOG, TestCppBooleanWorks)
{
  mozilla::glean::test_only::can_we_flag_it.Set(false);

  ASSERT_EQ(false, mozilla::glean::test_only::can_we_flag_it
                       .TestGetValue("test-ping"_ns)
                       .value());
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

using mozilla::MakeTuple;
using mozilla::Tuple;
using mozilla::glean::test_only_ipc::AnEventKeys;

TEST(FOG, TestCppEventWorks)
{
  test_only_ipc::no_extra_event.Record();
  ASSERT_TRUE(test_only_ipc::no_extra_event.TestGetValue("store1"_ns).isSome());

  // Ugh, this API...
  nsTArray<Tuple<test_only_ipc::AnEventKeys, nsCString>> extra;
  nsCString val = "can set extras"_ns;
  extra.AppendElement(MakeTuple(AnEventKeys::Extra1, val));

  test_only_ipc::an_event.Record(std::move(extra));
  ASSERT_TRUE(test_only_ipc::an_event.TestGetValue("store1"_ns).isSome());
}

TEST(FOG, TestCppMemoryDistWorks)
{
  test_only::do_you_remember.Accumulate(7);
  test_only::do_you_remember.Accumulate(17);

  DistributionData data =
      test_only::do_you_remember.TestGetValue("test-ping"_ns).ref();
  // Sum is in bytes, test_only::do_you_remember is in megabytes. So
  // multiplication ahoy!
  ASSERT_EQ(data.sum, 24UL * 1024 * 1024);
  for (auto iter = data.values.Iter(); !iter.Done(); iter.Next()) {
    const uint64_t bucket = iter.Key();
    const uint64_t count = iter.UserData();
    ASSERT_TRUE(count == 0 ||
                (count == 1 && (bucket == 17520006 || bucket == 7053950)))
    << "Only two occupied buckets";
  }
}

TEST(FOG, TestCppPings)
{
  auto ping = mozilla::glean_pings::OnePingOnly;
  mozilla::Unused << ping;
  // That's it. That's the test. It will fail to compile if it's missing.
  // For a test that actually submits the ping, we have integration tests.
  // See also bug 1681742.
}
