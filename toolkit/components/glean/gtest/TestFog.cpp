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
  const nsCString empty;
  ASSERT_EQ(NS_OK, fog_init(&empty, &empty));
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
  ASSERT_EQ(NS_OK, fog_submit_ping(&baselinePingName));
  ASSERT_EQ(NS_OK, fog_submit_ping(&eventsPingName));
}

TEST(FOG, TestCppCounterWorks)
{
  mozilla::glean::test_only::bad_code.Add(42);

  ASSERT_EQ(
      42,
      mozilla::glean::test_only::bad_code.TestGetValue("test-ping"_ns).value());
  // And test that the ping name's optional, while you're at it:
  ASSERT_EQ(42, test_only::bad_code.TestGetValue().value());
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

TEST(FOG, TestCppDatetimeWorks)
{
  PRExplodedTime date = {0, 35, 10, 12, 6, 10, 2020, 0, 0, {5 * 60 * 60, 0}};
  test_only::what_a_date.Set(&date);

  auto received = test_only::what_a_date.TestGetValue("test-ping"_ns);
  ASSERT_STREQ(received.value().get(), "2020-11-06T12:10:35+05:00");
}

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
  auto optEvents = test_only_ipc::an_event.TestGetValue("store1"_ns);
  ASSERT_TRUE(optEvents.isSome());

  auto events = optEvents.extract();
  ASSERT_EQ(1UL, events.Length());
  ASSERT_STREQ("test_only.ipc", events[0].mCategory.get());
  ASSERT_STREQ("an_event", events[0].mName.get());
  ASSERT_EQ(1UL, events[0].mExtra.Length());
  ASSERT_STREQ("extra1", mozilla::Get<0>(events[0].mExtra[0]).get());
  ASSERT_STREQ("can set extras", mozilla::Get<1>(events[0].mExtra[0]).get());
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
  for (const auto& entry : data.values) {
    const uint64_t bucket = entry.GetKey();
    const uint64_t count = entry.GetData();
    ASSERT_TRUE(count == 0 ||
                (count == 1 && (bucket == 17520006 || bucket == 7053950)))
    << "Only two occupied buckets";
  }
}

TEST(FOG, TestCppPings)
{
  test_only::one_ping_one_bool.Set(false);
  const auto& ping = mozilla::glean_pings::OnePingOnly;
  bool submitted = false;
  ping.TestBeforeNextSubmit([&submitted](const nsACString& aReason) {
    submitted = true;
    ASSERT_EQ(false, test_only::one_ping_one_bool.TestGetValue().ref());
  });
  ping.Submit();
  ASSERT_TRUE(submitted)
  << "Must have actually called the lambda.";
}

TEST(FOG, TestCppStringLists)
{
  auto kValue = "cheez!"_ns;
  auto kValue2 = "cheezier!"_ns;
  auto kValue3 = "cheeziest."_ns;

  nsTArray<nsCString> cheezList;
  cheezList.EmplaceBack(kValue);
  cheezList.EmplaceBack(kValue2);

  test_only::cheesy_string_list.Set(cheezList);

  auto val = test_only::cheesy_string_list.TestGetValue().value();
  // Note: This is fragile if the order is ever not preserved.
  ASSERT_STREQ(kValue.get(), val[0].get());
  ASSERT_STREQ(kValue2.get(), val[1].get());

  test_only::cheesy_string_list.Add(kValue3);

  val = test_only::cheesy_string_list.TestGetValue().value();
  ASSERT_STREQ(kValue3.get(), val[2].get());
}

TEST(FOG, TestCppTimingDistWorks)
{
  auto id1 = test_only::what_time_is_it.Start();
  auto id2 = test_only::what_time_is_it.Start();
  PR_Sleep(PR_MillisecondsToInterval(5));
  auto id3 = test_only::what_time_is_it.Start();
  test_only::what_time_is_it.Cancel(std::move(id1));
  PR_Sleep(PR_MillisecondsToInterval(5));
  test_only::what_time_is_it.StopAndAccumulate(std::move(id2));
  test_only::what_time_is_it.StopAndAccumulate(std::move(id3));

  DistributionData data = test_only::what_time_is_it.TestGetValue().ref();
  const uint64_t NANOS_IN_MILLIS = 1e6;

  // bug 1701847 - Sleeps don't necessarily round up as you'd expect.
  // Give ourselves a 40000ns (0.04ms) window to be off on fast machines.
  const uint64_t EPSILON = 40000;

  // We don't know exactly how long those sleeps took, only that it was at
  // least 15ms total.
  ASSERT_GT(data.sum, (uint64_t)(15 * NANOS_IN_MILLIS) - EPSILON);

  // We also can't guarantee the buckets, but we can guarantee two samples.
  uint64_t sampleCount = 0;
  for (const auto& value : data.values.Values()) {
    sampleCount += value;
  }
  ASSERT_EQ(sampleCount, (uint64_t)2);
}

TEST(FOG, TestLabeledBooleanWorks)
{
  ASSERT_EQ(mozilla::Nothing(),
            test_only::mabels_like_balloons.Get("hot_air"_ns).TestGetValue());
  test_only::mabels_like_balloons.Get("hot_air"_ns).Set(true);
  test_only::mabels_like_balloons.Get("helium"_ns).Set(false);
  ASSERT_EQ(
      true,
      test_only::mabels_like_balloons.Get("hot_air"_ns).TestGetValue().ref());
  ASSERT_EQ(
      false,
      test_only::mabels_like_balloons.Get("helium"_ns).TestGetValue().ref());
}

TEST(FOG, TestLabeledCounterWorks)
{
  ASSERT_EQ(mozilla::Nothing(),
            test_only::mabels_kitchen_counters.Get("marble"_ns).TestGetValue());
  test_only::mabels_kitchen_counters.Get("marble"_ns).Add(1);
  test_only::mabels_kitchen_counters.Get("laminate"_ns).Add(2);
  ASSERT_EQ(
      1,
      test_only::mabels_kitchen_counters.Get("marble"_ns).TestGetValue().ref());
  ASSERT_EQ(2, test_only::mabels_kitchen_counters.Get("laminate"_ns)
                   .TestGetValue()
                   .ref());
}

TEST(FOG, TestLabeledStringWorks)
{
  ASSERT_EQ(mozilla::Nothing(),
            test_only::mabels_balloon_strings.Get("twine"_ns).TestGetValue());
  test_only::mabels_balloon_strings.Get("twine"_ns).Set("seems acceptable"_ns);
  test_only::mabels_balloon_strings.Get("parachute_cord"_ns)
      .Set("preferred"_ns);
  ASSERT_STREQ("seems acceptable",
               test_only::mabels_balloon_strings.Get("twine"_ns)
                   .TestGetValue()
                   .ref()
                   .get());
  ASSERT_STREQ("preferred",
               test_only::mabels_balloon_strings.Get("parachute_cord"_ns)
                   .TestGetValue()
                   .ref()
                   .get());
}

TEST(FOG, TestCppQuantityWorks)
{
  // This joke only works in base 13.
  const uint32_t kValue = 6 * 9;
  mozilla::glean::test_only::meaning_of_life.Set(kValue);

  ASSERT_EQ(kValue,
            mozilla::glean::test_only::meaning_of_life.TestGetValue().value());
}
