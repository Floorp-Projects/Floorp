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

nsSlidingSubstring::nsSlidingSubstring( const nsSlidingSubstring& aString )
    : mStart(aString.mStart),
      mEnd(aString.mEnd),
      mBufferList(aString.mBufferList),
      mLength(aString.mLength)
  {
    mBufferList.AcquireReference();
    mStart.mBuffer->AcquireReference();
  }

nsSlidingSubstring::nsSlidingSubstring( const nsSlidingSubstring& aString, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
    : mStart(aStart),
      mEnd(aEnd),
      mBufferList(aString.mBufferList),
      mLength(PRUint32(Distance(mStart, mEnd)))
  {
    mBufferList.AcquireReference();
    mStart.mBuffer->AcquireReference();
  }

nsSlidingSubstring::nsSlidingSubstring( nsSlidingSharedBufferList& aBufferList )
    : mBufferList(aBufferList)
  {
    mBufferList.AcquireReference();

    mStart.PointBefore(mBufferList.GetFirstBuffer());
    mStart.mBuffer->AcquireReference();

    mEnd.PointAfter(mBufferList.GetLastBuffer());
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
    const Buffer* result_buffer = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          {
            const Buffer* current_buffer = NS_STATIC_CAST(const Buffer*, aFragment.mFragmentIdentifier);
            if ( current_buffer != mStart.mBuffer )
              result_buffer = current_buffer->mPrev;
          }
          break;

        case kFirstFragment:
          result_buffer = mStart.mBuffer;
          break;

        case kLastFragment:
          result_buffer = mEnd.mBuffer;
          break;

        case kNextFragment:
          {
            const Buffer* current_buffer = NS_STATIC_CAST(const Buffer*, aFragment.mFragmentIdentifier);
            if ( current_buffer != mEnd.mBuffer )
              result_buffer = current_buffer->mNext;
          }
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( result_buffer )
      {
        if ( result_buffer == mStart.mBuffer )
          aFragment.mStart = mStart.mPosInBuffer;
        else
          aFragment.mStart = result_buffer->DataStart();

        if ( result_buffer == mEnd.mBuffer )
          aFragment.mEnd = mEnd.mPosInBuffer;
        else
          aFragment.mEnd = result_buffer->DataEnd();

        aFragment.mFragmentIdentifier = result_buffer;
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
    Buffer* new_buffer = new Buffer(aStorageStart, aDataEnd, aStorageStart, aStorageEnd);
    Buffer* old_last_buffer = mBufferList.GetLastBuffer();
    mBufferList.LinkBuffer(old_last_buffer, new_buffer, 0);
    mLength += new_buffer->DataLength();

    mEnd.PointAfter(new_buffer);
  }

void
nsSlidingString::DiscardPrefix( const nsReadingIterator<PRUnichar>& aIter )
  {
    Position old_start(mStart);
    mStart = aIter;
    mLength -= Distance(old_start, mStart);

    mStart.mBuffer->AcquireReference();
    old_start.mBuffer->ReleaseReference();

    mBufferList.DiscardUnreferencedPrefix(old_start.mBuffer);
  }
