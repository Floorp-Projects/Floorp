/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

template <class T>
inline
const T&
NS_MIN( const T& a, const T& b )
  {
    return b < a ? b : a;
  }

// Must return b when a == b in case a is -0
template <class T>
inline
const T&
NS_MAX( const T& a, const T& b )
  {
    return a > b ? a : b;
  }

namespace mozilla {

template <class T>
inline
const T&
clamped( const T& a, const T& min, const T& max )
  {
    NS_ABORT_IF_FALSE(max >= min, "clamped(): max must be greater than or equal to min");
    return NS_MIN(NS_MAX(a, min), max);
  }

}

template <class T>
inline
T
NS_ABS( const T& a )
  {
    return a < 0 ? -a : a;
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
copy_string( const InputIterator& first, const InputIterator& last, OutputIterator& result )
  {
    typedef nsCharSourceTraits<InputIterator> source_traits;
    typedef nsCharSinkTraits<OutputIterator>  sink_traits;

    sink_traits::write(result, source_traits::read(first), source_traits::readable_distance(first, last));
    return result;
  }

#endif // !defined(nsAlgorithm_h___)
