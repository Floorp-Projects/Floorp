/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#ifndef nsAlgorithm_h___
#define nsAlgorithm_h___

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
  // for |nsCharSourceTraits|, |nsCharSinkTraits|
#endif

#ifndef prtypes_h___
#include "prtypes.h"
  // for |PRUint32|...
#endif

template <class T>
inline
const T&
NS_MIN( const T& a, const T& b )
  {
    return b < a ? b : a;
  }

template <class T>
inline
const T&
NS_MAX( const T& a, const T& b )
  {
    return a > b ? a : b;
  }

template <class InputIterator, class T>
inline
PRUint32
NS_COUNT( InputIterator& first, const InputIterator& last, const T& value )
  {
    PRUint32 result = 0;
    for ( ; first != last; ++first )
      if ( *first == value )
        ++result;
    return result;
  }

template <class InputIterator, class OutputIterator>
inline
OutputIterator&
copy_string( InputIterator& first, const InputIterator& last, OutputIterator& result )
  {
    typedef nsCharSourceTraits<InputIterator> source_traits;
    typedef nsCharSinkTraits<OutputIterator>  sink_traits;

    while ( first != last )
      {
        PRInt32 count_copied = PRInt32(sink_traits::write(result, source_traits::read(first), source_traits::readable_distance(first, last)));
        NS_ASSERTION(count_copied > 0, "|copy_string| will never terminate");
        source_traits::advance(first, count_copied);
      }

    return result;
  }

template <class InputIterator, class OutputIterator>
OutputIterator&
copy_string_backward( const InputIterator& first, InputIterator& last, OutputIterator& result )
  {
    while ( first != last )
      {
        last.normalize_backward();
        result.normalize_backward();
        PRUint32 lengthToCopy = PRUint32( NS_MIN(last.size_backward(), result.size_backward()) );
        if ( first.fragment().mStart == last.fragment().mStart )
          lengthToCopy = NS_MIN(lengthToCopy, PRUint32(last.get() - first.get()));

        NS_ASSERTION(lengthToCopy, "|copy_string_backward| will never terminate");

#ifdef _MSC_VER
        // XXX Visual C++ can't stomach 'typename' where it rightfully should
        nsCharTraits<OutputIterator::value_type>::move(result.get()-lengthToCopy, last.get()-lengthToCopy, lengthToCopy);
#else
        nsCharTraits<typename OutputIterator::value_type>::move(result.get()-lengthToCopy, last.get()-lengthToCopy, lengthToCopy);
#endif

        last.advance( -PRInt32(lengthToCopy) );
        result.advance( -PRInt32(lengthToCopy) );
      }

    return result;
  }


#endif // !defined(nsAlgorithm_h___)
