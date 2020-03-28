/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"

#include <type_traits>

using mozilla::Range;

static_assert(std::is_convertible_v<Range<int>, Range<const int>>,
              "Range should convert into const");
static_assert(!std::is_convertible_v<Range<const int>, Range<int>>,
              "Range should not drop const in conversion");

void test_RangeToBoolConversionShouldCompile() {
  auto dummy = bool{Range<int>{}};
  (void)dummy;
}

// We need a proper program so we have someplace to hang the static_asserts.
int main() { return 0; }
