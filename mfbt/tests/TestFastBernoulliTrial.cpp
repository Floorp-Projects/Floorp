/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/FastBernoulliTrial.h"

#include <math.h>

// Note that because we always provide FastBernoulliTrial with a fixed
// pseudorandom seed in these tests, the results here are completely
// deterministic.
//
// A non-optimized version of this test runs in .009s on my laptop. Using larger
// sample sizes lets us meet tighter bounds on the counts.

static void
TestProportions()
{
  mozilla::FastBernoulliTrial bernoulli(1.0,
                                        698079309544035222ULL,
                                        6012389156611637584ULL);

  for (size_t i = 0; i < 100; i++)
    MOZ_RELEASE_ASSERT(bernoulli.trial());

  {
    bernoulli.setProbability(0.5);
    size_t count = 0;
    for (size_t i = 0; i < 1000; i++)
      count += bernoulli.trial();
    MOZ_RELEASE_ASSERT(count == 496);
  }

  {
    bernoulli.setProbability(0.001);
    size_t count = 0;
    for (size_t i = 0; i < 1000; i++)
      count += bernoulli.trial();
    MOZ_RELEASE_ASSERT(count == 2);
  }

  {
    bernoulli.setProbability(0.85);
    size_t count = 0;
    for (size_t i = 0; i < 1000; i++)
      count += bernoulli.trial();
    MOZ_RELEASE_ASSERT(count == 852);
  }

  bernoulli.setProbability(0.0);
  for (size_t i = 0; i < 100; i++)
    MOZ_RELEASE_ASSERT(!bernoulli.trial());
}

static void
TestHarmonics()
{
  mozilla::FastBernoulliTrial bernoulli(0.1,
                                        698079309544035222ULL,
                                        6012389156611637584ULL);

  const size_t n = 100000;
  bool trials[n];
  for (size_t i = 0; i < n; i++)
    trials[i] = bernoulli.trial();

  // For each harmonic and phase, check that the proportion sampled is
  // within acceptable bounds.
  for (size_t harmonic = 1; harmonic < 20; harmonic++) {
    size_t expected = n / harmonic / 10;
    size_t low_expected = expected * 85 / 100;
    size_t high_expected = expected * 115 / 100;

    for (size_t phase = 0; phase < harmonic; phase++) {
      size_t count = 0;
      for (size_t i = phase; i < n; i += harmonic)
        count += trials[i];

      MOZ_RELEASE_ASSERT(low_expected <= count && count <= high_expected);
    }
  }
}

static void
TestTrialN()
{
  mozilla::FastBernoulliTrial bernoulli(0.01,
                                        0x67ff17e25d855942ULL,
                                        0x74f298193fe1c5b1ULL);

  {
    size_t count = 0;
    for (size_t i = 0; i < 10000; i++)
      count += bernoulli.trial(1);

    // Expected value: 0.01 * 10000 == 100
    MOZ_RELEASE_ASSERT(count == 97);
  }

  {
    size_t count = 0;
    for (size_t i = 0; i < 10000; i++)
      count += bernoulli.trial(3);

    // Expected value: (1 - (1 - 0.01) ** 3) == 0.0297,
    // 0.0297 * 10000 == 297
    MOZ_RELEASE_ASSERT(count == 304);
  }

  {
    size_t count = 0;
    for (size_t i = 0; i < 10000; i++)
      count += bernoulli.trial(10);

    // Expected value: (1 - (1 - 0.01) ** 10) == 0.0956,
    // 0.0956 * 10000 == 956
    MOZ_RELEASE_ASSERT(count == 936);
  }

  {
    size_t count = 0;
    for (size_t i = 0; i < 10000; i++)
      count += bernoulli.trial(100);

    // Expected value: (1 - (1 - 0.01) ** 100) == 0.6339
    // 0.6339 * 10000 == 6339
    MOZ_RELEASE_ASSERT(count == 6372);
  }

  {
    size_t count = 0;
    for (size_t i = 0; i < 10000; i++)
      count += bernoulli.trial(1000);

    // Expected value: (1 - (1 - 0.01) ** 1000) == 0.9999
    // 0.9999 * 10000 == 9999
    MOZ_RELEASE_ASSERT(count == 9998);
  }
}

static void
TestChangeProbability()
{
  mozilla::FastBernoulliTrial bernoulli(1.0,
                                        0x67ff17e25d855942ULL,
                                        0x74f298193fe1c5b1ULL);

  // Establish a very high skip count.
  bernoulli.setProbability(0.0);

  // This should re-establish a zero skip count.
  bernoulli.setProbability(1.0);

  // So this should return true.
  MOZ_RELEASE_ASSERT(bernoulli.trial());
}

static void
TestCuspProbabilities()
{
  /*
   * FastBernoulliTrial takes care to avoid screwing up on edge cases. The
   * checks here all look pretty dumb, but they exercise paths in the code that
   * could exhibit undefined behavior if coded naïvely.
   */

  /*
   * This should not be perceptibly different from 1; for 64-bit doubles, this
   * is a one in ten trillion chance of the trial not succeeding. Overflows
   * converting doubles to size_t skip counts may change this, though.
   */
  mozilla::FastBernoulliTrial bernoulli(nextafter(1, 0),
                                        0x67ff17e25d855942ULL,
                                        0x74f298193fe1c5b1ULL);

  for (size_t i = 0; i < 1000; i++)
    MOZ_RELEASE_ASSERT(bernoulli.trial());

  /*
   * This should not be perceptibly different from 0; for 64-bit doubles,
   * the FastBernoulliTrial will actually treat this as exactly zero.
   */
  bernoulli.setProbability(nextafter(0, 1));
  for (size_t i = 0; i < 1000; i++)
    MOZ_RELEASE_ASSERT(!bernoulli.trial());

  /*
   * This should be a vanishingly low probability which FastBernoulliTrial does
   * *not* treat as exactly zero.
   */
  bernoulli.setProbability(1 - nextafter(1, 0));
  for (size_t i = 0; i < 1000; i++)
    MOZ_RELEASE_ASSERT(!bernoulli.trial());
}

int
main()
{
  TestProportions();
  TestHarmonics();
  TestTrialN();
  TestChangeProbability();
  TestCuspProbabilities();

  return 0;
}
