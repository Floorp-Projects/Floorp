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

#ifndef nsBufferHandleUtils_h___
#include "nsBufferHandleUtils.h"
#endif

#include <new.h>



  /**
   * |nsSlidingSharedBufferList|
   */

void
nsSlidingSharedBufferList::DiscardUnreferencedPrefix( Buffer* aRecentlyReleasedBuffer )
  {
    if ( aRecentlyReleasedBuffer == mFirstBuffer )
      {
        while ( mFirstBuffer && !mFirstBuffer->IsReferenced() )
          delete UnlinkBuffer(mFirstBuffer);
      }
  }



  /**
   * |nsSlidingSubstring|
   */

  // copy constructor
nsSlidingSubstring::nsSlidingSubstring( const nsSlidingSubstring& aString )
    : mStart(aString.mStart),
      mEnd(aString.mEnd),
      mBufferList(aString.mBufferList),
      mLength(aString.mLength)
  {
    acquire_ownership_of_buffer_list();
  }

nsSlidingSubstring::nsSlidingSubstring( const nsSlidingSubstring& aString, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
    : mStart(aStart),
      mEnd(aEnd),
      mBufferList(aString.mBufferList),
      mLength(PRUint32(Position::Distance(mStart, mEnd)))
  {
    acquire_ownership_of_buffer_list();
  }

nsSlidingSubstring::nsSlidingSubstring( const nsSlidingString& aString )
    : mStart(aString.mStart),
      mEnd(aString.mEnd),
      mBufferList(aString.mBufferList),
      mLength(aString.mLength)
  {
    acquire_ownership_of_buffer_list();
  }

nsSlidingSubstring::nsSlidingSubstring( const nsSlidingString& aString, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
    : mStart(aStart),
      mEnd(aEnd),
      mBufferList(aString.mBufferList),
      mLength(PRUint32(Position::Distance(mStart, mEnd)))
  {
    acquire_ownership_of_buffer_list();
  }

nsSlidingSubstring::nsSlidingSubstring( nsSlidingSharedBufferList* aBufferList )
    : mBufferList(aBufferList)
  {
    init_range_from_buffer_list();
    acquire_ownership_of_buffer_list();
  }

typedef const nsSharedBufferList::Buffer* Buffer_ptr;

static nsSharedBufferList::Buffer*
AllocateContiguousHandleWithData( Buffer_ptr aDummyHandlePtr, const nsAReadableString& aDataSource )
  {
    typedef const PRUnichar* PRUnichar_ptr;

      // figure out the number of bytes needed the |HandleT| part, including padding to correctly align the data part
    size_t handle_size    = NS_AlignedHandleSize(aDummyHandlePtr, PRUnichar_ptr(0));

      // figure out how many |CharT|s wee need to fit in the data part
    size_t string_length  = aDataSource.Length();

      // how many bytes is that (including a zero-terminator so we can claim to be flat)?
    size_t string_size    = (string_length+1) * sizeof(PRUnichar);


    nsSharedBufferList::Buffer* result = 0;
    void* handle_ptr = ::operator new(handle_size + string_size);

    if ( handle_ptr )
      {
        typedef PRUnichar* PRUnichar_ptr;
        PRUnichar* string_start_ptr = PRUnichar_ptr(NS_STATIC_CAST(unsigned char*, handle_ptr) + handle_size);
        PRUnichar* string_end_ptr   = string_start_ptr + string_length;

        nsReadingIterator<PRUnichar> fromBegin, fromEnd;
        PRUnichar* toBegin = string_start_ptr;
        copy_string(aDataSource.BeginReading(fromBegin), aDataSource.EndReading(fromEnd), toBegin);
        result = new (handle_ptr) nsSharedBufferList::Buffer(string_start_ptr, string_end_ptr, string_start_ptr, string_end_ptr+1, PR_TRUE);
      }

    return result;
  }

nsSlidingSubstring::nsSlidingSubstring( const nsAReadableString& aSourceString )
    : mBufferList(new nsSlidingSharedBufferList(AllocateContiguousHandleWithData(Buffer_ptr(0), aSourceString)))
  {
    init_range_from_buffer_list();
    acquire_ownership_of_buffer_list();
  }

void
nsSlidingSubstring::Rebind( const nsSlidingSubstring& aString )
  {
    aString.acquire_ownership_of_buffer_list();
    release_ownership_of_buffer_list();

    mStart      = aString.mStart;
    mEnd        = aString.mEnd;
    mBufferList = aString.mBufferList;
    mLength     = aString.mLength;
  }

void
nsSlidingSubstring::Rebind( const nsSlidingSubstring& aString, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
  {
    release_ownership_of_buffer_list();

    mStart      = aStart;
    mEnd        = aEnd;
    mBufferList = aString.mBufferList;
    mLength     = PRUint32(Position::Distance(mStart, mEnd));

    acquire_ownership_of_buffer_list();
  }

void
nsSlidingSubstring::Rebind( const nsSlidingString& aString )
  {
    aString.acquire_ownership_of_buffer_list();
    release_ownership_of_buffer_list();

    mStart      = aString.mStart;
    mEnd        = aString.mEnd;
    mBufferList = aString.mBufferList;
    mLength     = aString.mLength;
  }

void
nsSlidingSubstring::Rebind( const nsSlidingString& aString, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
  {
    release_ownership_of_buffer_list();

    mStart      = aStart;
    mEnd        = aEnd;
    mBufferList = aString.mBufferList;
    mLength     = PRUint32(Position::Distance(mStart, mEnd));

    acquire_ownership_of_buffer_list();
  }

void
nsSlidingSubstring::Rebind( const nsAReadableString& aSourceString )
  {
    release_ownership_of_buffer_list();
    mBufferList = new nsSlidingSharedBufferList(AllocateContiguousHandleWithData(Buffer_ptr(0), aSourceString));
    init_range_from_buffer_list();
    acquire_ownership_of_buffer_list();
  }

nsSlidingSubstring::~nsSlidingSubstring()
  {
    release_ownership_of_buffer_list();
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



  /**
   * |nsSlidingString|
   */

nsSlidingString::nsSlidingString( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
    : nsSlidingSubstring(new nsSlidingSharedBufferList(nsSlidingSharedBufferList::NewWrappingBuffer(aStorageStart, aDataEnd, aStorageEnd)))
  {
    // nothing else to do here
  }

void
nsSlidingString::AppendBuffer( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
  {
    Buffer* new_buffer = new Buffer(aStorageStart, aDataEnd, aStorageStart, aStorageEnd);
    Buffer* old_last_buffer = mBufferList->GetLastBuffer();
    mBufferList->LinkBuffer(old_last_buffer, new_buffer, 0);
    mLength += new_buffer->DataLength();

    mEnd.PointAfter(new_buffer);
  }

void
nsSlidingString::DiscardPrefix( const nsReadingIterator<PRUnichar>& aIter )
  {
    Position old_start(mStart);
    mStart = aIter;
    mLength -= Position::Distance(old_start, mStart);

    mStart.mBuffer->AcquireNonOwningReference();
    old_start.mBuffer->ReleaseNonOwningReference();

    mBufferList->DiscardUnreferencedPrefix(old_start.mBuffer);
  }

const PRUnichar*
nsSlidingString::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    return nsSlidingSubstring::GetReadableFragment(aFragment, aRequest, aOffset);
  }
