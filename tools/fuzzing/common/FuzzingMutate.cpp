/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingMutate.h"
#include "FuzzingTraits.h"

namespace mozilla {
namespace fuzzing {

/**
 * Randomly mutates a byte inside |aData| by using bit manipulation.
 */
/* static */
void
FuzzingMutate::ChangeBit(uint8_t* aData, size_t aLength)
{
  size_t offset = RandomIntegerRange<size_t>(0, aLength);
  aData[offset] ^= (1 << RandomIntegerRange<unsigned char>(0, 8));
}

/**
 * Randomly replaces a byte inside |aData| with one in the range of [0, 255].
 */
/* static */
void
FuzzingMutate::ChangeByte(uint8_t* aData, size_t aLength)
{
  size_t offset = RandomIntegerRange<size_t>(0, aLength);
  aData[offset] = RandomInteger<unsigned char>();
}

} // namespace fuzzing
} // namespace mozilla
