/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsASCIIMask_h_
#define nsASCIIMask_h_

#include <array>
#include <utility>

#include "mozilla/Attributes.h"

typedef std::array<bool, 128> ASCIIMaskArray;

namespace mozilla {

// Boolean arrays, fixed size and filled in at compile time, meant to
// record something about each of the (standard) ASCII characters.
// No extended ASCII for now, there has been no use case.
// If you have loops that go through a string character by character
// and test for equality to a certain set of characters before deciding
// on a course of action, chances are building up one of these arrays
// and using it is going to be faster, especially if the set of
// characters is more than one long, and known at compile time.
class ASCIIMask
{
public:
  // Preset masks for some common character groups
  // When testing, you must check if the index is < 128 or use IsMasked()
  //
  // if (someChar < 128 && MaskCRLF()[someChar]) this is \r or \n

  static const ASCIIMaskArray& MaskCRLF();
  static const ASCIIMaskArray& Mask0to9();
  static const ASCIIMaskArray& MaskCRLFTab();
  static const ASCIIMaskArray& MaskWhitespace();

  static MOZ_ALWAYS_INLINE bool IsMasked(const ASCIIMaskArray& aMask, uint32_t aChar)
  {
    return aChar < 128 && aMask[aChar];
  }
};

// Outside of the preset ones, use these templates to create more masks.
//
// The example creation will look like this:
//
// constexpr bool TestABC(char c) { return c == 'A' || c == 'B' || c == 'C'; }
// constexpr std::array<bool, 128> sABCMask = CreateASCIIMask(TestABC);
// ...
// if (someChar < 128 && sABCMask[someChar]) this is A or B or C


namespace details
{
template<typename F, size_t... Indices>
constexpr std::array<bool, 128> CreateASCIIMask(F fun, std::index_sequence<Indices...>)
{
  return {{ fun(Indices)... }};
}
} // namespace details

template<typename F>
constexpr std::array<bool, 128> CreateASCIIMask(F fun)
{
  return details::CreateASCIIMask(fun, std::make_index_sequence<128>{});
}

} // namespace mozilla

#endif // nsASCIIMask_h_
