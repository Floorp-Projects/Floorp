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

//-------1---------2---------3---------4---------5---------6---------7---------8

#include "nsSharableString.h"
// #include "nsBufferHandleUtils.h"
#include "nsDependentSubstring.h"

void
nsSharableString::SetCapacity( size_type aNewCapacity )
  {
    // Note: Capacity numbers do not include room for a terminating
    // NULL.  However, StorageLength numbers do, since this string type
    // requires a terminating null so we include it in the storage of
    // our buffer handle.

    if ( aNewCapacity )
      {
        // SetCapacity wouldn't be called if the caller didn't intend to
        // mutate the string.
        //
        // If the buffer is shared, we want to allocate a new buffer
        // unconditionally.  If we do not, and the caller plans to do an
        // assign and a series of appends, the assign will lead to a
        // small buffer that will then be grown in steps, defeating the
        // point of |SetCapacity|.
        //
        // Since the caller is planning to mutate the string, we don't
        // want to make the new buffer any larger than the requested
        // capacity since that would be a waste of space.  This means
        // that, when the string is shared, we may want to give a string
        // a buffer shorter than its current length.
        //
        // Since sharing should be transparent to the caller, we should
        // therefore *unconditionally* truncate the current length of
        // the string to the requested capacity.

        if ( mBuffer->IsShared() )
          {
            if ( aNewCapacity > Length() )
              mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                            *this, PRUint32(aNewCapacity - Length() + 1));
            else
              mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                            Substring(*this, 0, aNewCapacity), PRUint32(1));
          }
        else
          {
            if ( aNewCapacity >= mBuffer->StorageLength() )
              {
                // This is where we implement the "exact size on assign,
                // double on fault" allocation strategy.  We don't do it
                // exactly since we don't double on a fault when the
                // buffer is shared, but we'll double soon enough.
                size_type doubledCapacity = (mBuffer->StorageLength() - 1) * 2;
                if ( doubledCapacity > aNewCapacity )
                  aNewCapacity = doubledCapacity;

                // XXX We should be able to use |realloc| under certain
                // conditions (contiguous buffer handle, kIsShared
                // (,etc.)?)
                mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                                 *this, PRUint32(aNewCapacity - Length() + 1));
              }
            else if ( aNewCapacity < mBuffer->DataLength() )
              {
                // Ensure we always have the same effect on the length
                // whether or not the buffer is shared, as mentioned
                // above.
                mBuffer->DataEnd(mBuffer->DataStart() + aNewCapacity);
                *mBuffer->DataEnd() = char_type('\0');
              }
          }
      }
    else
      mBuffer = GetSharedEmptyBufferHandle();
  }

void
nsSharableString::SetLength( size_type aNewLength )
  {
    if ( aNewLength > mBuffer->DataLength() )
      {
        SetCapacity(aNewLength);
        mBuffer->DataEnd( mBuffer->DataStart() + aNewLength );
      }
    else
      {
        mBuffer->DataEnd( mBuffer->DataStart() + aNewLength );
        *mBuffer->DataEnd() = char_type(0); // This is needed for |Truncate|
                                            // callers but perhaps not
                                            // for callers that are
                                            // manipulating the
                                            // internals of the string.
      }
  }

void
nsSharableString::do_AssignFromReadable( const abstract_string_type& aReadable )
  {
    const shared_buffer_handle_type* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      {
        // null-check |mBuffer.get()| here only for the constructor
        // taking |const abstract_string_type&|
        size_type inLength;
        if ( mBuffer.get() && !mBuffer->IsShared() &&
             mBuffer->StorageLength() > (inLength = aReadable.Length()) &&
             !aReadable.IsDependentOn(*this) )
          {
            abstract_string_type::const_iterator fromBegin, fromEnd;
            char_type *storage_start = mBuffer->DataStart();
            *copy_string( aReadable.BeginReading(fromBegin),
                          aReadable.EndReading(fromEnd),
                          storage_start ) = char_type(0);
            mBuffer->DataEnd(storage_start + inLength);
            return; // don't want to assign to |mBuffer| below
          }
        else
          handle = NS_AllocateContiguousHandleWithData(handle,
                                                       aReadable, PRUint32(1));
      }
    mBuffer = handle;
  }

const nsSharableString::shared_buffer_handle_type*
nsSharableString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

void
nsSharableString::Adopt( char_type* aNewValue )
  {
    size_type length = nsCharTraits<char_type>::length(aNewValue);
    mBuffer = new nsSharedBufferHandle<char_type>(aNewValue, aNewValue+length,
                                                  length, PR_FALSE);
  }

/* static */
nsSharableString::shared_buffer_handle_type*
nsSharableString::GetSharedEmptyBufferHandle()
  {
    static shared_buffer_handle_type* sBufferHandle = nsnull;

    if (!sBufferHandle) {
      sBufferHandle = NS_AllocateContiguousHandleWithData(sBufferHandle,
                                              PRUint32(1), (self_type*)nsnull);
      sBufferHandle->AcquireReference();
    }
    return sBufferHandle;
  }


void
nsSharableCString::SetCapacity( size_type aNewCapacity )
  {
    // Note: Capacity numbers do not include room for a terminating
    // NULL.  However, StorageLength numbers do, since this string type
    // requires a terminating null so we include it in the storage of
    // our buffer handle.

    if ( aNewCapacity )
      {
        // SetCapacity wouldn't be called if the caller didn't intend to
        // mutate the string.
        //
        // If the buffer is shared, we want to allocate a new buffer
        // unconditionally.  If we do not, and the caller plans to do an
        // assign and a series of appends, the assign will lead to a
        // small buffer that will then be grown in steps, defeating the
        // point of |SetCapacity|.
        //
        // Since the caller is planning to mutate the string, we don't
        // want to make the new buffer any larger than the requested
        // capacity since that would be a waste of space.  This means
        // that, when the string is shared, we may want to give a string
        // a buffer shorter than its current length.
        //
        // Since sharing should be transparent to the caller, we should
        // therefore *unconditionally* truncate the current length of
        // the string to the requested capacity.

        if ( mBuffer->IsShared() )
          {
            if ( aNewCapacity > Length() )
              mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                            *this, PRUint32(aNewCapacity - Length() + 1));
            else
              mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                            Substring(*this, 0, aNewCapacity), PRUint32(1));
          }
        else
          {
            if ( aNewCapacity >= mBuffer->StorageLength() )
              {
                // This is where we implement the "exact size on assign,
                // double on fault" allocation strategy.  We don't do it
                // exactly since we don't double on a fault when the
                // buffer is shared, but we'll double soon enough.
                size_type doubledCapacity = (mBuffer->StorageLength() - 1) * 2;
                if ( doubledCapacity > aNewCapacity )
                  aNewCapacity = doubledCapacity;

                // XXX We should be able to use |realloc| under certain
                // conditions (contiguous buffer handle, kIsShared
                // (,etc.)?)
                mBuffer = NS_AllocateContiguousHandleWithData(mBuffer.get(),
                                 *this, PRUint32(aNewCapacity - Length() + 1));
              }
            else if ( aNewCapacity < mBuffer->DataLength() )
              {
                // Ensure we always have the same effect on the length
                // whether or not the buffer is shared, as mentioned
                // above.
                mBuffer->DataEnd(mBuffer->DataStart() + aNewCapacity);
                *mBuffer->DataEnd() = char_type('\0');
              }
          }
      }
    else
      mBuffer = GetSharedEmptyBufferHandle();
  }

void
nsSharableCString::SetLength( size_type aNewLength )
  {
    if ( aNewLength > mBuffer->DataLength() )
      {
        SetCapacity(aNewLength);
        mBuffer->DataEnd( mBuffer->DataStart() + aNewLength );
      }
    else
      {
        mBuffer->DataEnd( mBuffer->DataStart() + aNewLength );
        *mBuffer->DataEnd() = char_type(0); // This is needed for |Truncate|
                                            // callers but perhaps not
                                            // for callers that are
                                            // manipulating the
                                            // internals of the string.
      }
  }

void
nsSharableCString::do_AssignFromReadable( const abstract_string_type& aReadable )
  {
    const shared_buffer_handle_type* handle = aReadable.GetSharedBufferHandle();
    if ( !handle )
      {
        // null-check |mBuffer.get()| here only for the constructor
        // taking |const abstract_string_type&|
        size_type inLength;
        if ( mBuffer.get() && !mBuffer->IsShared() &&
             mBuffer->StorageLength() > (inLength = aReadable.Length()) &&
             !aReadable.IsDependentOn(*this) )
          {
            abstract_string_type::const_iterator fromBegin, fromEnd;
            char_type *storage_start = mBuffer->DataStart();
            *copy_string( aReadable.BeginReading(fromBegin),
                          aReadable.EndReading(fromEnd),
                          storage_start ) = char_type(0);
            mBuffer->DataEnd(storage_start + inLength);
            return; // don't want to assign to |mBuffer| below
          }
        else
          handle = NS_AllocateContiguousHandleWithData(handle,
                                                       aReadable, PRUint32(1));
      }
    mBuffer = handle;
  }

const nsSharableCString::shared_buffer_handle_type*
nsSharableCString::GetSharedBufferHandle() const
  {
    return mBuffer.get();
  }

void
nsSharableCString::Adopt( char_type* aNewValue )
  {
    size_type length = nsCharTraits<char_type>::length(aNewValue);
    mBuffer = new nsSharedBufferHandle<char_type>(aNewValue, aNewValue+length,
                                                  length, PR_FALSE);
  }

/* static */
nsSharableCString::shared_buffer_handle_type*
nsSharableCString::GetSharedEmptyBufferHandle()
  {
    static shared_buffer_handle_type* sBufferHandle = nsnull;

    if (!sBufferHandle) {
      sBufferHandle = NS_AllocateContiguousHandleWithData(sBufferHandle,
                                              PRUint32(1), (self_type*)nsnull);
      sBufferHandle->AcquireReference();
    }
    return sBufferHandle;
  }

