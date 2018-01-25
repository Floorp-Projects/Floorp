/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "gtest/gtest.h"
#include "nsRFPService.h"

using namespace mozilla;

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

   Specially: you need to look at more decimal places. Run the test with:
   	  MOZ_LOG="nsResistFingerprinting:5"

   And look for two successive lines similar to the below (the format will certainly
   be different by the time you read this comment):
      V/nsResistFingerprinting Given: 2064.83384599999999, Reciprocal Rounding with 50000.00000000000000, Intermediate: 103241692.00000000000000, Got: 2064.83383999999978
      V/nsResistFingerprinting Given: 2064.83383999999978, Reciprocal Rounding with 50000.00000000000000, Intermediate: 103241691.00000000000000, Got: 2064.83381999999983

   Look at the last two values:
      Got: 2064.83383999999978
      Got: 2064.83381999999983

   They're supposed to be equal. They're not. But they both round to 2064.83.
*/

void process(double clock, nsRFPService::TimeScale clockUnits, double precision) {
    double reduced1 = nsRFPService::ReduceTimePrecisionImpl(clock, clockUnits, precision);
	double reduced2 = nsRFPService::ReduceTimePrecisionImpl(reduced1, clockUnits, precision);
	ASSERT_EQ(reduced1, reduced2);
}

TEST(ResistFingerprinting, ReducePrecision_Assumptions) {
	ASSERT_EQ(FLT_RADIX, 2);
	ASSERT_EQ(DBL_MANT_DIG, 53);
}

TEST(ResistFingerprinting, ReducePrecision_Reciprocal) {
	// This one has a rounding error in the Reciprocal case:
	process(2064.8338460, nsRFPService::TimeScale::MicroSeconds, 20);
	// These are just big values
	process(1516305819, nsRFPService::TimeScale::MicroSeconds, 20);
	process(69053.12, nsRFPService::TimeScale::MicroSeconds, 20);
}

TEST(ResistFingerprinting, ReducePrecision_KnownGood) {
	process(2064.8338460, nsRFPService::TimeScale::MilliSeconds, 20);
	process(69027.62, nsRFPService::TimeScale::MilliSeconds, 20);
	process(69053.12, nsRFPService::TimeScale::MilliSeconds, 20);
}

TEST(ResistFingerprinting, ReducePrecision_KnownBad) {
	process(1054.842405, nsRFPService::TimeScale::MilliSeconds, 20);
	process(273.53038600000002, nsRFPService::TimeScale::MilliSeconds, 20);
	process(628.66686500000003, nsRFPService::TimeScale::MilliSeconds, 20);
	process(521.28919100000007, nsRFPService::TimeScale::MilliSeconds, 20);
}

TEST(ResistFingerprinting, ReducePrecision_Edge) {
	process(2611.14, nsRFPService::TimeScale::MilliSeconds, 20);
	process(2611.16, nsRFPService::TimeScale::MilliSeconds, 20);
	process(2612.16, nsRFPService::TimeScale::MilliSeconds, 20);
	process(2601.64, nsRFPService::TimeScale::MilliSeconds, 20);
	process(2595.16, nsRFPService::TimeScale::MilliSeconds, 20);
	process(2578.66, nsRFPService::TimeScale::MilliSeconds, 20);
}

TEST(ResistFingerprinting, ReducePrecision_ExpectedLossOfPrecision) {
	double result;
	// We lose integer precision at 9007199254740992 - let's confirm that.
	result = nsRFPService::ReduceTimePrecisionImpl(9007199254740992, nsRFPService::TimeScale::MicroSeconds, 5);
	ASSERT_EQ(result, 9007199254740990);
	// 9007199254740995 is approximated to 9007199254740996
	result = nsRFPService::ReduceTimePrecisionImpl(9007199254740995, nsRFPService::TimeScale::MicroSeconds, 5);
	ASSERT_EQ(result, 9007199254740996);
	// 9007199254740999 is approximated as 9007199254741000
	result = nsRFPService::ReduceTimePrecisionImpl(9007199254740999, nsRFPService::TimeScale::MicroSeconds, 5);
	ASSERT_EQ(result, 9007199254741000);
	// 9007199254743568 can be represented exactly, but will be clamped to 9007199254743564
	result = nsRFPService::ReduceTimePrecisionImpl(9007199254743568, nsRFPService::TimeScale::MicroSeconds, 5);
	ASSERT_EQ(result, 9007199254743564);
}


TEST(ResistFingerprinting, ReducePrecision_Expectations) {
	double result;
	result = nsRFPService::ReduceTimePrecisionImpl(2611.14, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.145, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.141, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.159, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.15, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.13, nsRFPService::TimeScale::MilliSeconds, 20);
	ASSERT_EQ(result, 2611.12);
}

// Use an ugly but simple hack to turn an integer-based rand()
// function to a double-based one.
#define RAND_DOUBLE (rand() * (rand() / (double)rand()))

// If you're doing logging, you really don't want to run this test.
#define RUN_AGGRESSIVE true

TEST(ResistFingerprinting, ReducePrecision_Aggressive) {
	if(!RUN_AGGRESSIVE) {
		return;
	}

	for (int i=0; i<10000; i++) {
		// Test three different time magnitudes, with decimals.
		// Note that we need separate variables for the different units, as scaling
		// them after calculating them will erase effects of approximation.
		// A magnitude in the seconds since epoch range.
		double time1_s = fmod(RAND_DOUBLE, 1516305819);
		double time1_ms = fmod(RAND_DOUBLE, 1516305819000);
		double time1_us = fmod(RAND_DOUBLE, 1516305819000000);
		// A magnitude in the 'couple of minutes worth of milliseconds' range.
		double time2_s = fmod(RAND_DOUBLE, (60 * 60 * 5));
		double time2_ms = fmod(RAND_DOUBLE, (1000 * 60 * 60 * 5));
		double time2_us = fmod(RAND_DOUBLE, (1000000 * 60 * 60 * 5));
		// A magnitude in the small range.
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
}
