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

void process(double clock, double precision, double precisionUnits) {
    double reduced1 = nsRFPService::ReduceTimePrecisionImpl(clock, precision, precisionUnits);
	double reduced2 = nsRFPService::ReduceTimePrecisionImpl(reduced1, precision, precisionUnits);
	ASSERT_EQ(reduced1, reduced2);
}

TEST(ResistFingerprinting, ReducePrecision_Assumptions) {
	ASSERT_EQ(FLT_RADIX, 2);
	ASSERT_EQ(DBL_MANT_DIG, 53);
}

TEST(ResistFingerprinting, ReducePrecision_Reciprocal) {
	// This one has a rounding error in the Reciprocal case:
	process(2064.8338460, 20, 1000000);
	// These are just big values
	process(1516305819, 20, 1000000);
	process(69053.12, 20, 1000000);
}

TEST(ResistFingerprinting, ReducePrecision_KnownGood) {
	process(2064.8338460, 20, 1000);
	process(69027.62, 20, 1000);
	process(69053.12, 20, 1000);
}

TEST(ResistFingerprinting, ReducePrecision_KnownBad) {
	process(1054.842405, 20, 1000);
	process(273.53038600000002, 20, 1000);
	process(628.66686500000003, 20, 1000);
	process(521.28919100000007, 20, 1000);
}

TEST(ResistFingerprinting, ReducePrecision_Edge) {
	process(2611.14, 20, 1000);
	process(2611.16, 20, 1000);
	process(2612.16, 20, 1000);
	process(2601.64, 20, 1000);
	process(2595.16, 20, 1000);
	process(2578.66, 20, 1000);
}

TEST(ResistFingerprinting, ReducePrecision_Expectations) {
	double result;
	result = nsRFPService::ReduceTimePrecisionImpl(2611.14, 20, 1000);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.145, 20, 1000);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.141, 20, 1000);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.15999, 20, 1000);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.15, 20, 1000);
	ASSERT_EQ(result, 2611.14);
	result = nsRFPService::ReduceTimePrecisionImpl(2611.13, 20, 1000);
	ASSERT_EQ(result, 2611.12);
}

// Use an ugly but simple hack to turn an integer-based rand()
// function to a double-based one
#define RAND_DOUBLE (rand() * (rand() / (double)rand()))

TEST(ResistFingerprinting, ReducePrecision_Aggressive) {
	for (int i=0; i<10000; i++) {
		// Test three different time magnitudes, with decimals.
		// Note that we need separate variables for the different units, as scaling
		// them after calculating them will erase effects of approximation
		// A magnitude in the seconds since epoch range
		double time1_s = fmod(RAND_DOUBLE, 1516305819);
		double time1_ms = fmod(RAND_DOUBLE, 1516305819000);
		double time1_us = fmod(RAND_DOUBLE, 1516305819000000);
		// A magnitude in the 'couple of minutes worth of milliseconds' range
		double time2_s = fmod(RAND_DOUBLE, (60 * 60 * 5));
		double time2_ms = fmod(RAND_DOUBLE, (1000 * 60 * 60 * 5));
		double time2_us = fmod(RAND_DOUBLE, (1000000 * 60 * 60 * 5));
		// A magnitude in the small range
		double time3_s = fmod(RAND_DOUBLE, 10);
		double time3_ms = fmod(RAND_DOUBLE, 10000);
		double time3_us = fmod(RAND_DOUBLE, 10000000);

		// Test two precision magnitudes, no decimals
		// A magnitude in the high milliseconds
		double precision1 = rand() % 250000;
		// a magnitude in the low microseconds
		double precision2 = rand() % 200;

		//printf("%.*f, %.*f, %.*f\n", DBL_DIG-1, time1, DBL_DIG-1, time2, DBL_DIG-1, time3);
		process(time1_s, precision1, 1000000);
		process(time1_s, precision2, 1000000);
		process(time2_s, precision1, 1000000);
		process(time2_s, precision2, 1000000);
		process(time3_s, precision1, 1000000);
		process(time3_s, precision2, 1000000);

		process(time1_ms, precision1, 1000);
		process(time1_ms, precision2, 1000);
		process(time2_ms, precision1, 1000);
		process(time2_ms, precision2, 1000);
		process(time3_ms, precision1, 1000);
		process(time3_ms, precision2, 1000);

		process(time1_us, precision1, 1);
		process(time1_us, precision2, 1);
		process(time2_us, precision1, 1);
		process(time2_us, precision2, 1);
		process(time3_us, precision1, 1);
		process(time3_us, precision2, 1);
	}
}
