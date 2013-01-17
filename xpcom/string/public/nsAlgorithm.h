/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlgorithm_h___
#define nsAlgorithm_h___

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
  // for |nsCharSourceTraits|, |nsCharSinkTraits|
#endif

#ifndef nsDebug_h___
#include "nsDebug.h"
  // for NS_ASSERTION
#endif


template <class T>
inline
T
NS_ROUNDUP( const T& a, const T& b )
  {
    return ((a + (b - 1)) / b) * b;
  }

// We use these instead of std::min/max because we can't include the algorithm
// header in all of XPCOM because the stl wrappers will error out when included
// in parts of XPCOM. These functions should never be used outside of XPCOM.
template <class T>
inline
const T&
XPCOM_MIN( const T& a, const T& b )
  {
    return b < a ? b : a;
  }

// Must return b when a == b in case a is -0
template <class T>
inline
const T&
XPCOM_MAX( const T& a, const T& b )
  {
    return a > b ? a : b;
  }

#if defined(_MSC_VER) && (_MSC_VER < 1600)
namespace std {
inline
long long
abs( const long long& a )
{
  return a < 0 ? -a : a;
}
}
#endif

namespace mozilla {

template <class T>
inline
const T&
clamped( const T& a, const T& min, const T& max )
  {
    NS_ABORT_IF_FALSE(max >= min, "clamped(): max must be greater than or equal to min");
    return XPCOM_MIN(XPCOM_MAX(a, min), max);
  }

}

template <class InputIterator, class T>
inline
uint32_t
NS_COUNT( InputIterator& first, const InputIterator& last, const T& value )
  {
    uint32_t result = 0;
    for ( ; first != last; ++first )
      if ( *first == value )
        ++result;
    return result;
  }

template <class InputIterator, class OutputIterator>
inline
OutputIterator&
copy_string( const InputIterator& first, const InputIterator& last, OutputIterator& result )
  {
    typedef nsCharSourceTraits<InputIterator> source_traits;
    typedef nsCharSinkTraits<OutputIterator>  sink_traits;

    sink_traits::write(result, source_traits::read(first), source_traits::readable_distance(first, last));
    return result;
  }

#endif // !defined(nsAlgorithm_h___)
