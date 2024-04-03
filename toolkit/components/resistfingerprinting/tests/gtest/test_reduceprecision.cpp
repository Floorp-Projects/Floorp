/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "gtest/gtest.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsRFPService.h"

using namespace mozilla;

// clang-format off
/*
   Hello! Are you looking at this file because you got an error you don't understand?
   Perhaps something that looks like the following?

    toolkit/components/resistfingerprinting/tests/test_reduceprecision.cpp:15: Failure
      Expected: reduced1
        Which is: 2064.83
      To be equal to: reduced2
        Which is: 2064.83

   "Gosh," you might say, "They sure look equal to me. What the heck is going on here?"

   The answer lies beyond what you can see, in that which you cannot see. One must
   journey into the depths, the hidden, that which the world fights its hardest to
   conceal from us.

   Specifically: you need to look at more decimal places. Run the test with:
       MOZ_LOG="nsResistFingerprinting:5"

   And look for two successive lines similar to the below (the format will probably
   be different by the time you read this comment):
      V/nsResistFingerprinting Given: 2064.83384599999999, Reciprocal Rounding with 50000.00000000000000, Intermediate: 103241692.00000000000000, Got: 2064.83383999999978
      V/nsResistFingerprinting Given: 2064.83383999999978, Reciprocal Rounding with 50000.00000000000000, Intermediate: 103241691.00000000000000, Got: 2064.83381999999983

   Look at the last two values:
      Got: 2064.83383999999978
      Got: 2064.83381999999983

   They're supposed to be equal. They're not. But they both round to 2064.83.
   Good luck and godspeed.
*/
// clang-format on

bool setupJitter(bool enabled) {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  bool jitterEnabled = false;
  if (prefs) {
    prefs->GetBoolPref(
        "privacy.resistFingerprinting.reduceTimerPrecision.jitter",
        &jitterEnabled);
    prefs->SetBoolPref(
        "privacy.resistFingerprinting.reduceTimerPrecision.jitter", enabled);
  }

  return jitterEnabled;
}

void cleanupJitter(bool jitterWasEnabled) {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetBoolPref(
        "privacy.resistFingerprinting.reduceTimerPrecision.jitter",
        jitterWasEnabled);
  }
}

void process(double clock, nsRFPService::TimeScale clockUnits,
             double precision) {
  double reduced1 = nsRFPService::ReduceTimePrecisionImpl(
      clock, clockUnits, precision, -1, TimerPrecisionType::Normal);
  double reduced2 = nsRFPService::ReduceTimePrecisionImpl(
      reduced1, clockUnits, precision, -1, TimerPrecisionType::Normal);
  ASSERT_EQ(reduced1, reduced2);
}

TEST(ResistFingerprinting, ReducePrecision_Assumptions)
{
  ASSERT_EQ(FLT_RADIX, 2);
  ASSERT_EQ(DBL_MANT_DIG, 53);
}

TEST(ResistFingerprinting, ReducePrecision_Reciprocal)
{
  bool jitterEnabled = setupJitter(false);
  // This one has a rounding error in the Reciprocal case:
  process(2064.8338460, nsRFPService::TimeScale::MicroSeconds, 20);
  // These are just big values
  process(1516305819, nsRFPService::TimeScale::MicroSeconds, 20);
  process(69053.12, nsRFPService::TimeScale::MicroSeconds, 20);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_KnownGood)
{
  bool jitterEnabled = setupJitter(false);
  process(2064.8338460, nsRFPService::TimeScale::MilliSeconds, 20);
  process(69027.62, nsRFPService::TimeScale::MilliSeconds, 20);
  process(69053.12, nsRFPService::TimeScale::MilliSeconds, 20);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_KnownBad)
{
  bool jitterEnabled = setupJitter(false);
  process(1054.842405, nsRFPService::TimeScale::MilliSeconds, 20);
  process(273.53038600000002, nsRFPService::TimeScale::MilliSeconds, 20);
  process(628.66686500000003, nsRFPService::TimeScale::MilliSeconds, 20);
  process(521.28919100000007, nsRFPService::TimeScale::MilliSeconds, 20);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_Edge)
{
  bool jitterEnabled = setupJitter(false);
  process(2611.14, nsRFPService::TimeScale::MilliSeconds, 20);
  process(2611.16, nsRFPService::TimeScale::MilliSeconds, 20);
  process(2612.16, nsRFPService::TimeScale::MilliSeconds, 20);
  process(2601.64, nsRFPService::TimeScale::MilliSeconds, 20);
  process(2595.16, nsRFPService::TimeScale::MilliSeconds, 20);
  process(2578.66, nsRFPService::TimeScale::MilliSeconds, 20);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_Expectations)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.14, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.145, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.141, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15999, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.13, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 2611.12);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_Expectations_HighRes)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.14, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.145, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.141, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15999, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.13, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 2611.12);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_Expectations_RFP)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.14, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.145, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.141, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15999, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.13, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 2611.12);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_Expectations_DangerouslyNone)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.14, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.14);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.145, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.145);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.141, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.141);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15999, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.15999);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.15, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.15);
  result = nsRFPService::ReduceTimePrecisionImpl(
      2611.13, nsRFPService::TimeScale::MilliSeconds, 20, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 2611.13);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_ExpectedLossOfPrecision)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  // We lose integer precision at 9007199254740992 - let's confirm that.
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740992.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 9007199254740990.0);
  // 9007199254740995 is approximated to 9007199254740996
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740995.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 9007199254740996);
  // 9007199254740999 is approximated as 9007199254741000
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740999.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 9007199254741000.0);
  // 9007199254743568 can be represented exactly, but will be clamped to
  // 9007199254743564
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254743568.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 9007199254743564.0);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_ExpectedLossOfPrecision_HighRes)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  // We lose integer precision at 9007199254740992 - let's confirm that.
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740992.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 9007199254740980.0);
  // 9007199254740995 is approximated to 9007199254740980
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740995.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 9007199254740980);
  // 9007199254740999 is approximated as 9007199254741000
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740999.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 9007199254741000.0);
  // 9007199254743568 can be represented exactly, but will be clamped to
  // 9007199254743560
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254743568.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::UnconditionalAKAHighRes);
  ASSERT_EQ(result, 9007199254743560.0);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_ExpectedLossOfPrecision_RFP)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  // We lose integer precision at 9007199254740992 - let's confirm that.
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740992.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 9007199254740990.0);
  // 9007199254740995 is approximated to 9007199254740980
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740995.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 9007199254740995);
  // 9007199254740999 is approximated as 9007199254741000
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740999.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 9007199254741000.0);
  // 9007199254743568 can be represented exactly, but will be clamped to
  // 9007199254743560
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254743568.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::RFP);
  ASSERT_EQ(result, 9007199254743565.0);
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting,
     ReducePrecision_ExpectedLossOfPrecision_DangerouslyNone)
{
  bool jitterEnabled = setupJitter(false);
  double result;
  // We lose integer precision at 9007199254740992 - let's confirm that.
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740992.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 9007199254740992.0);
  // 9007199254740995 is approximated to 9007199254740980
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740995.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 9007199254740995);
  // 9007199254740999 is approximated as 9007199254741000
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254740999.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 9007199254740999.0);
  // 9007199254743568 can be represented exactly, but will be clamped to
  // 9007199254743560
  result = nsRFPService::ReduceTimePrecisionImpl(
      9007199254743568.0, nsRFPService::TimeScale::MicroSeconds, 5, -1,
      TimerPrecisionType::DangerouslyNone);
  ASSERT_EQ(result, 9007199254743568.0);
  cleanupJitter(jitterEnabled);
}

// Use an ugly but simple hack to turn an integer-based rand()
// function to a double-based one.
#define RAND_DOUBLE (rand() * (rand() / (double)rand()))

// If you're doing logging, you really don't want to run this test.
#define RUN_AGGRESSIVE false

TEST(ResistFingerprinting, ReducePrecision_Aggressive)
{
  if (!RUN_AGGRESSIVE) {
    return;
  }

  bool jitterEnabled = setupJitter(false);

  for (int i = 0; i < 10000; i++) {
    // Test three different time magnitudes, with decimals.
    // Note that we need separate variables for the different units, as scaling
    // them after calculating them will erase effects of approximation.
    // A magnitude in the seconds since epoch range.
    double time1_s = fmod(RAND_DOUBLE, 1516305819.0);
    double time1_ms = fmod(RAND_DOUBLE, 1516305819000.0);
    double time1_us = fmod(RAND_DOUBLE, 1516305819000000.0);
    // A magnitude in the 'couple of minutes worth of milliseconds' range.
    double time2_s = fmod(RAND_DOUBLE, (60.0 * 60 * 5));
    double time2_ms = fmod(RAND_DOUBLE, (1000.0 * 60 * 60 * 5));
    double time2_us = fmod(RAND_DOUBLE, (1000000.0 * 60 * 60 * 5));
    // A magnitude in the small range
    double time3_s = fmod(RAND_DOUBLE, 10);
    double time3_ms = fmod(RAND_DOUBLE, 10000);
    double time3_us = fmod(RAND_DOUBLE, 10000000);

    // Test two precision magnitudes, no decimals.
    // A magnitude in the high milliseconds.
    double precision1 = rand() % 250000;
    // a magnitude in the low microseconds.
    double precision2 = rand() % 200;

    process(time1_s, nsRFPService::TimeScale::Seconds, precision1);
    process(time1_s, nsRFPService::TimeScale::Seconds, precision2);
    process(time2_s, nsRFPService::TimeScale::Seconds, precision1);
    process(time2_s, nsRFPService::TimeScale::Seconds, precision2);
    process(time3_s, nsRFPService::TimeScale::Seconds, precision1);
    process(time3_s, nsRFPService::TimeScale::Seconds, precision2);

    process(time1_ms, nsRFPService::TimeScale::MilliSeconds, precision1);
    process(time1_ms, nsRFPService::TimeScale::MilliSeconds, precision2);
    process(time2_ms, nsRFPService::TimeScale::MilliSeconds, precision1);
    process(time2_ms, nsRFPService::TimeScale::MilliSeconds, precision2);
    process(time3_ms, nsRFPService::TimeScale::MilliSeconds, precision1);
    process(time3_ms, nsRFPService::TimeScale::MilliSeconds, precision2);

    process(time1_us, nsRFPService::TimeScale::MicroSeconds, precision1);
    process(time1_us, nsRFPService::TimeScale::MicroSeconds, precision2);
    process(time2_us, nsRFPService::TimeScale::MicroSeconds, precision1);
    process(time2_us, nsRFPService::TimeScale::MicroSeconds, precision2);
    process(time3_us, nsRFPService::TimeScale::MicroSeconds, precision1);
    process(time3_us, nsRFPService::TimeScale::MicroSeconds, precision2);
  }
  cleanupJitter(jitterEnabled);
}

TEST(ResistFingerprinting, ReducePrecision_JitterTestVectors)
{
  bool jitterEnabled = setupJitter(true);

  // clang-format off
  /*
   * Here's our test vector. We set the secret to the 16 byte value
   * 0x0001020304050607 0x1011121314151617
   *
   * We then use a stand alone XOrShift128+ generator:
   *
   * The midpoints are:
   *   0    = 328
   *   500  = 25
   *   1000 = 73
   *   1500 = 89
   *   2000 = 73
   *   2500 = 153
   *   3000 = 9
   *   3500 = 89
   *   4000 = 73
   *   4500 = 205
   *   5000 = 253
   *   5500 = 141
   */
  // clang-format on

  // Set the secret
  long long throwAway;
  uint8_t hardcodedSecret[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x06, 0x07, 0x10, 0x11, 0x12, 0x13,
                                 0x14, 0x15, 0x16, 0x17};

  nsRFPService::RandomMidpoint(0, 500, -1, &throwAway, hardcodedSecret);

  // Run the test vectors
  double result;

  result = nsRFPService::ReduceTimePrecisionImpl(
      1, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 0);
  result = nsRFPService::ReduceTimePrecisionImpl(
      184, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 0);
  result = nsRFPService::ReduceTimePrecisionImpl(
      185, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      329, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      499, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);

  result = nsRFPService::ReduceTimePrecisionImpl(
      500, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      520, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      524, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      525, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 1000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      930, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 1000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      1072, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 1000);

  result = nsRFPService::ReduceTimePrecisionImpl(
      4000, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4050, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4072, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4073, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4340, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4499, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);

  result = nsRFPService::ReduceTimePrecisionImpl(
      4500, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4536, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4704, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 4500);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4705, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 5000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      4726, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 5000);
  result = nsRFPService::ReduceTimePrecisionImpl(
      5106, nsRFPService::TimeScale::MicroSeconds, 500, 4000,
      TimerPrecisionType::Normal);
  ASSERT_EQ(result, 5000);

  cleanupJitter(jitterEnabled);
}
