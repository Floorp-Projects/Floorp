/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RandomNum.h"
#include <vector>

/*

 * We're going to check that random number generation is sane on a basic
 * level - That is, we want to check that the function returns success
 * and doesn't just keep returning the same number.
 *
 * Note that there are many more tests that could be done, but to really test
 * a PRNG we'd probably need to generate a large set of randoms and
 * perform statistical analysis on them. Maybe that's worth doing eventually?
 *
 * For now we should be fine just performing a dumb test of generating 5
 * numbers and making sure they're all unique. In theory, it is possible for
 * this test to report a false negative, but with 5 numbers the probability
 * is less than one-in-a-trillion.
 *
 */

#define NUM_RANDOMS_TO_GENERATE 5

using mozilla::Maybe;
using mozilla::RandomUint64;

static uint64_t getRandomUint64OrDie() {
  Maybe<uint64_t> maybeRandomNum = RandomUint64();

  MOZ_RELEASE_ASSERT(maybeRandomNum.isSome());

  return maybeRandomNum.value();
}

static void TestRandomUint64() {
  // The allocator uses RandomNum.h too, but its initialization path allocates
  // memory. While the allocator itself handles the situation, we can't, so
  // we make sure to use an allocation before getting a Random number ourselves.
  std::vector<uint64_t> randomsList;
  randomsList.reserve(NUM_RANDOMS_TO_GENERATE);

  for (uint8_t i = 0; i < NUM_RANDOMS_TO_GENERATE; ++i) {
    uint64_t randomNum = getRandomUint64OrDie();

    for (uint64_t num : randomsList) {
      MOZ_RELEASE_ASSERT(randomNum != num);
    }

    randomsList.push_back(randomNum);
  }
}

int main() {
  TestRandomUint64();
  return 0;
}
