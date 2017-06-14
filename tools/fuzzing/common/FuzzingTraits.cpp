/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingTraits.h"

namespace mozilla {
namespace fuzzing {

/* static */
unsigned int
FuzzingTraits::Random(unsigned int aMax)
{
  MOZ_ASSERT(aMax > 0, "aMax needs to be bigger than 0");
  return static_cast<unsigned int>(random() % aMax);
}

/* static */
bool
FuzzingTraits::Sometimes(unsigned int aProbability)
{
  return FuzzingTraits::Random(aProbability) == 0;
}

/*static */
size_t
FuzzingTraits::Frequency(const size_t aSize, const uint64_t aFactor)
{
  return RandomIntegerRange<size_t>(0, ceil(float(aSize) / aFactor)) + 1;
}

} // namespace fuzzing
} // namespace mozilla
