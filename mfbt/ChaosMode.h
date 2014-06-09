/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ChaosMode_h
#define mozilla_ChaosMode_h

#include <stdint.h>
#include <stdlib.h>

namespace mozilla {

/**
 * When "chaos mode" is activated, code that makes implicitly nondeterministic
 * choices is encouraged to make random and extreme choices, to test more
 * code paths and uncover bugs.
 */
class ChaosMode
{
public:
  static bool isActive()
  {
    // Flip this to true to activate chaos mode
    return false;
  }

  /**
   * Returns a somewhat (but not uniformly) random uint32_t < aBound.
   * Not to be used for anything except ChaosMode, since it's not very random.
   */
  static uint32_t randomUint32LessThan(uint32_t aBound)
  {
    return uint32_t(rand()) % aBound;
  }
};

} /* namespace mozilla */

#endif /* mozilla_ChaosMode_h */
