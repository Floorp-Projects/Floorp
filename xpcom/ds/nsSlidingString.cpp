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
 * The Original Code is Mozilla strings.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#include "nsSlidingString.h"

void
nsSlidingSharedBufferList::DiscardUnreferencedPrefix( Buffer* aRecentlyReleasedBuffer )
  {
    if ( aRecentlyReleasedBuffer == mFirstBuffer )
      {
        while ( mFirstBuffer && !mFirstBuffer->IsReferenced() )
          delete UnlinkBuffer(mFirstBuffer);
      }
  }


ptrdiff_t Distance( const nsSharedBufferList::Position&, const nsSharedBufferList::Position& );

ptrdiff_t
Distance( const nsSharedBufferList::Position& aStart, const nsSharedBufferList::Position& aEnd )
  {
    ptrdiff_t result = 0;
    if ( aStart.mBuffer == aEnd.mBuffer )
      result = aEnd.mPosInBuffer - aStart.mPosInBuffer;
    else
      {
        result = aStart.mBuffer->DataEnd() - aStart.mPosInBuffer;
        for ( nsSharedBufferList::Buffer* b = aStart.mBuffer->mNext; b != aEnd.mBuffer; b = b->mNext )
          result += b->DataLength();
        result += aEnd.mPosInBuffer - aEnd.mBuffer->DataStart();
      }

    return result;
  }

nsSlidingSubstring::nsSlidingSubstring( nsSlidingSharedBufferList& aBufferList, const nsSharedBufferList::Position& aStart, const nsSharedBufferList::Position& aEnd )
    : mStart(aStart), mEnd(aEnd), mBufferList(aBufferList), mLength(PRUint32(Distance(aStart, aEnd)))
  {
    mBufferList.AcquireReference();
  }

nsSlidingSubstring::nsSlidingSubstring( nsSlidingSharedBufferList& aBufferList )
    : mBufferList(aBufferList)
  {
    mBufferList.AcquireReference();

    mStart  = nsSlidingSharedBufferList::Position(mBufferList.GetFirstBuffer(), mBufferList.GetFirstBuffer()->DataStart());
    mEnd    = nsSlidingSharedBufferList::Position(mBufferList.GetLastBuffer(), mBufferList.GetLastBuffer()->DataEnd());
    mLength = PRUint32(Distance(mStart, mEnd));
  }

nsSlidingSubstring::~nsSlidingSubstring()
  {
    mStart.mBuffer->ReleaseReference();
    mBufferList.DiscardUnreferencedPrefix(mStart.mBuffer);
    mBufferList.ReleaseReference();
  }

const PRUnichar*
nsSlidingSubstring::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    const nsSlidingSharedBufferList::Buffer* buffer = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          buffer = NS_STATIC_CAST(const nsSlidingSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          buffer = mBufferList.GetFirstBuffer();
          break;

        case kLastFragment:
          buffer = mBufferList.GetLastBuffer();
          break;

        case kNextFragment:
          buffer = NS_STATIC_CAST(const nsSlidingSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mNext;
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



nsSlidingString::nsSlidingString( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
    : nsSlidingSubstring(*(new nsSlidingSharedBufferList(nsSlidingSharedBufferList::NewWrappingBuffer(aStorageStart, aDataEnd, aStorageEnd))))
  {
    // nothing else to do here
  }

void
nsSlidingString::AppendBuffer( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
  {
    nsSlidingSharedBufferList::Buffer* new_buffer = new nsSlidingSharedBufferList::Buffer(aStorageStart, aDataEnd, aStorageStart, aStorageEnd);
    nsSlidingSharedBufferList::Buffer* old_last_buffer = mBufferList.GetLastBuffer();
    mBufferList.LinkBuffer(old_last_buffer, new_buffer, 0);
    mLength += new_buffer->DataLength();

    if ( !old_last_buffer )
      {
        // set up mStart and mEnd from the now non-empty list
      }
  }

void
nsSlidingString::DiscardPrefix( const nsReadingIterator<PRUnichar>& aIter )
  {
    const nsSlidingSharedBufferList::Buffer* new_start_buffer = NS_REINTERPRET_CAST(const nsSlidingSharedBufferList::Buffer*, aIter.fragment().mFragmentIdentifier);
    new_start_buffer->AcquireReference();

    nsSlidingSharedBufferList::Buffer* old_start_buffer = mStart.mBuffer;
    mStart.mBuffer = NS_CONST_CAST(nsSlidingSharedBufferList::Buffer*, new_start_buffer);
    mStart.mPosInBuffer = NS_CONST_CAST(PRUnichar*, aIter.get());

    old_start_buffer->ReleaseReference();

    mBufferList.DiscardUnreferencedPrefix(old_start_buffer);
  }
