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
 * The Original Code is Mozilla XPCOM.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 */

#include "nsFragmentedString.h"


const nsFragmentedString::char_type*
nsFragmentedString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    const nsSharedBufferList::Buffer* buffer = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          buffer = NS_STATIC_CAST(const nsSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          buffer = mBufferList.GetFirstBuffer();
          break;

        case kLastFragment:
          buffer = mBufferList.GetLastBuffer();
          break;

        case kNextFragment:
          buffer = NS_STATIC_CAST(const nsSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( buffer )
      {
        aFragment.mStart  = buffer->DataStart();
        aFragment.mEnd    = buffer->DataEnd();
        aFragment.mFragmentIdentifier = buffer;
        return aFragment.mStart + aOffset;
      }

    return 0;
  }


nsFragmentedString::char_type*
nsFragmentedString::GetWritableFragment( fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    nsSharedBufferList::Buffer* buffer = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          buffer = NS_STATIC_CAST(nsSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          buffer = mBufferList.GetFirstBuffer();
          break;

        case kLastFragment:
          buffer = mBufferList.GetLastBuffer();
          break;

        case kNextFragment:
          buffer = NS_STATIC_CAST(nsSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( buffer )
      {
        aFragment.mStart  = buffer->DataStart();
        aFragment.mEnd    = buffer->DataEnd();
        aFragment.mFragmentIdentifier = buffer;
        return aFragment.mStart + aOffset;
      }

    return 0;
  }

  /**
   * ...
   */
PRUint32
nsFragmentedString::Length() const
  {
    return PRUint32(mBufferList.GetDataLength());
  }

  /**
   * |SetLength|
   */
void
nsFragmentedString::SetLength( PRUint32 aNewLength )
  {
    // according to the current interpretation of |SetLength|,
    //  cut off characters from the end, or else add unitialized space to fill

    if ( aNewLength < PRUint32(mBufferList.GetDataLength()) )
      {
//      if ( aNewLength )
          mBufferList.DiscardSuffix(mBufferList.GetDataLength()-aNewLength);
//      else
//        mBufferList.DestroyBuffers();
      }

// temporarily... eliminate as soon as our munging routines don't need this form of |SetLength|
    else if ( aNewLength > PRUint32(mBufferList.GetDataLength()) )
      {
        size_t empty_space_to_add = aNewLength - mBufferList.GetDataLength();
        nsSharedBufferList::Buffer* new_buffer = nsSharedBufferList::NewSingleAllocationBuffer(0, 0, empty_space_to_add);
        new_buffer->DataEnd(new_buffer->DataStart()+empty_space_to_add);
        mBufferList.LinkBuffer(mBufferList.GetLastBuffer(), new_buffer, 0);
      }
  }


#if 0
  /**
   * |SetCapacity|.
   *
   * If a client tries to increase the capacity of multi-fragment string, perhaps a single
   * empty fragment of the appropriate size should be appended.
   */
void
nsFragmentedString::SetCapacity( PRUint32 aNewCapacity )
  {
    if ( !aNewCapacity )
      {
        // |SetCapacity(0)| is special and means ``release all storage''.
      }
    else if ( aNewCapacity > ... )
      {
        
      }
  }
#endif
