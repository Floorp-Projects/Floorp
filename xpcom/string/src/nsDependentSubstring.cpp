/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#include "nsDependentSubstring.h"

PRUint32
nsDependentSubstring::Length() const
  {
    return mLength;
  }

const nsDependentSubstring::char_type*
nsDependentSubstring::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
      // Offset any request for a specific position (First, Last, At) by our
      //  substrings startpos within the owning string

    if ( aRequest == kFirstFragment )
      {
        aPosition = mStartPos;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kLastFragment )
      {
        aPosition = mStartPos + mLength;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kFragmentAt )
      aPosition += mStartPos;

    // requests for |kNextFragment| or |kPrevFragment| are just relayed down into the string we're slicing

    const char_type* position_ptr = mString.GetReadableFragment(aFragment, aRequest, aPosition);

    // If |GetReadableFragment| returns |0|, then we are off the string, the contents of the
    //  fragment are garbage.

      // Therefore, only need to fix up the fragment boundaries when |position_ptr| is not null
    if ( position_ptr )
      {
          // if there's more physical data in the returned fragment than I logically have left...
        size_t logical_size_backward = aPosition - mStartPos;
        if ( size_t(position_ptr - aFragment.mStart) > logical_size_backward )
          aFragment.mStart = position_ptr - logical_size_backward;

        size_t logical_size_forward = mLength - logical_size_backward;
        if ( size_t(aFragment.mEnd - position_ptr) > logical_size_forward )
          aFragment.mEnd = position_ptr + logical_size_forward;
      }

    return position_ptr;
  }




PRUint32
nsDependentCSubstring::Length() const
  {
    return mLength;
  }

const nsDependentCSubstring::char_type*
nsDependentCSubstring::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
      // Offset any request for a specific position (First, Last, At) by our
      //  substrings startpos within the owning string

    if ( aRequest == kFirstFragment )
      {
        aPosition = mStartPos;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kLastFragment )
      {
        aPosition = mStartPos + mLength;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kFragmentAt )
      aPosition += mStartPos;

    // requests for |kNextFragment| or |kPrevFragment| are just relayed down into the string we're slicing

    const char_type* position_ptr = mString.GetReadableFragment(aFragment, aRequest, aPosition);

    // If |GetReadableFragment| returns |0|, then we are off the string, the contents of the
    //  fragment are garbage.

      // Therefore, only need to fix up the fragment boundaries when |position_ptr| is not null
    if ( position_ptr )
      {
          // if there's more physical data in the returned fragment than I logically have left...
        size_t logical_size_backward = aPosition - mStartPos;
        if ( size_t(position_ptr - aFragment.mStart) > logical_size_backward )
          aFragment.mStart = position_ptr - logical_size_backward;

        size_t logical_size_forward = mLength - logical_size_backward;
        if ( size_t(aFragment.mEnd - position_ptr) > logical_size_forward )
          aFragment.mEnd = position_ptr + logical_size_forward;
      }

    return position_ptr;
  }
