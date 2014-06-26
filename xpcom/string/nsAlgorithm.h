/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlgorithm_h___
#define nsAlgorithm_h___

#include "nsCharTraits.h"  // for |nsCharSourceTraits|, |nsCharSinkTraits|

template <class T>
inline T
NS_ROUNDUP(const T& aA, const T& aB)
{
  return ((aA + (aB - 1)) / aB) * aB;
}

// We use these instead of std::min/max because we can't include the algorithm
// header in all of XPCOM because the stl wrappers will error out when included
// in parts of XPCOM. These functions should never be used outside of XPCOM.
template <class T>
inline const T&
XPCOM_MIN(const T& aA, const T& aB)
{
  return aB < aA ? aB : aA;
}

// Must return b when a == b in case a is -0
template <class T>
inline const T&
XPCOM_MAX(const T& aA, const T& aB)
{
  return aA > aB ? aA : aB;
}

namespace mozilla {

template <class T>
inline const T&
clamped(const T& aA, const T& aMin, const T& aMax)
{
  NS_ABORT_IF_FALSE(aMax >= aMin,
                    "clamped(): aMax must be greater than or equal to aMin");
  return XPCOM_MIN(XPCOM_MAX(aA, aMin), aMax);
}

}

template <class InputIterator, class T>
inline uint32_t
NS_COUNT(InputIterator& aFirst, const InputIterator& aLast, const T& aValue)
{
  uint32_t result = 0;
  for (; aFirst != aLast; ++aFirst)
    if (*aFirst == aValue) {
      ++result;
    }
  return result;
}

template <class InputIterator, class OutputIterator>
inline OutputIterator&
copy_string(const InputIterator& aFirst, const InputIterator& aLast,
            OutputIterator& aResult)
{
  typedef nsCharSourceTraits<InputIterator> source_traits;
  typedef nsCharSinkTraits<OutputIterator>  sink_traits;

  sink_traits::write(aResult, source_traits::read(aFirst),
                     source_traits::readable_distance(aFirst, aLast));
  return aResult;
}

#endif // !defined(nsAlgorithm_h___)
