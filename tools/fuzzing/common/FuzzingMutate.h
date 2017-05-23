/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fuzzing_FuzzingMutate_h
#define mozilla_fuzzing_FuzzingMutate_h

#include "mozilla/TypeTraits.h"
#include <random>

namespace mozilla {
namespace fuzzing {

class FuzzingMutate
{
public:
  static void ChangeBit(uint8_t* aData, size_t aLength);
  static void ChangeByte(uint8_t* aData, size_t aLength);
};

} // namespace fuzzing
} // namespace mozilla

#endif
