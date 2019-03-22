/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <prinrval.h>
#include <thread>
#include <mutex>
#include "FuzzingTraits.h"

namespace mozilla {
namespace fuzzing {

/* static */
unsigned int FuzzingTraits::Random(unsigned int aMax) {
  MOZ_ASSERT(aMax > 0, "aMax needs to be bigger than 0");
  std::uniform_int_distribution<unsigned int> d(0, aMax);
  return d(Rng());
}

/* static */
bool FuzzingTraits::Sometimes(unsigned int aProbability) {
  return FuzzingTraits::Random(aProbability) == 0;
}

/* static */
size_t FuzzingTraits::Frequency(const size_t aSize, const uint64_t aFactor) {
  return RandomIntegerRange<size_t>(0, ceil(float(aSize) / aFactor)) + 1;
}

/* static */
std::mt19937_64& FuzzingTraits::Rng() {
  static std::mt19937_64 rng;
  static std::once_flag flag;
  std::call_once(flag, [&] { rng.seed(PR_IntervalNow()); });
  return rng;
}

}  // namespace fuzzing
}  // namespace mozilla
