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

#include "nsSharedBufferList.h"
#include "nsAlgorithm.h"
  // for |copy_string|
#include <new.h>


void
nsSharedBufferList::DestroyBuffers()
  {
      // destroy the entire list of buffers, without bothering to manage their links
    Buffer* next_buffer;
    for ( Buffer* cur_buffer=mFirstBuffer; cur_buffer; cur_buffer=next_buffer )
      {
        next_buffer = cur_buffer->mNext;
        operator delete(cur_buffer);
      }
    mFirstBuffer = mLastBuffer = 0;
    mTotalDataLength = 0;
  }

nsSharedBufferList::~nsSharedBufferList()
  {
    DestroyBuffers();
  }


nsSharedBufferList::Buffer*
nsSharedBufferList::NewBuffer( const PRUnichar* aData, PRUint32 aDataLength, PRUint32 aAdditionalSpace )
  {
    size_t    object_size    = ((sizeof(Buffer) + sizeof(PRUnichar) - 1) / sizeof(PRUnichar)) * sizeof(PRUnichar);
    PRUint32  buffer_length  = aDataLength + aAdditionalSpace;
    size_t    buffer_size    = size_t(buffer_length) * sizeof(PRUnichar);

    void* object_ptr = operator new(object_size + buffer_size);
    if ( object_ptr )
      {
        typedef PRUnichar* PRUnichar_ptr;
        PRUnichar* buffer_ptr = PRUnichar_ptr(NS_STATIC_CAST(unsigned char*, object_ptr) + object_size);
        if ( aDataLength )
          {
            PRUnichar* toBegin = buffer_ptr;
            copy_string(aData, aData+aDataLength, toBegin);
          }
        return new (object_ptr) Buffer(buffer_ptr, buffer_ptr+aDataLength, buffer_ptr, buffer_ptr+buffer_length);
      }

    return 0;
  }



void
nsSharedBufferList::LinkBuffer( Buffer* aPrevBuffer, Buffer* aNewBuffer, Buffer* aNextBuffer )
  {
    NS_ASSERTION(aNewBuffer, "aNewBuffer");
    NS_ASSERTION(aPrevBuffer || mFirstBuffer == aNextBuffer, "aPrevBuffer || mFirstBuffer == aNextBuffer");
    NS_ASSERTION(!aPrevBuffer || aPrevBuffer->mNext == aNextBuffer, "!aPrevBuffer || aPrevBuffer->mNext == aNextBuffer");
    NS_ASSERTION(aNextBuffer || mLastBuffer == aPrevBuffer, "aNextBuffer || mLastBuffer == aPrevBuffer");
    NS_ASSERTION(!aNextBuffer || aNextBuffer->mPrev == aPrevBuffer, "!aNextBuffer || aNextBuffer->mPrev == aPrevBuffer");

    if ( (aNewBuffer->mPrev = aPrevBuffer) )
      aPrevBuffer->mNext = aNewBuffer;
    else
      mFirstBuffer = aNewBuffer;

    if ( (aNewBuffer->mNext = aNextBuffer) )
      aNextBuffer->mPrev = aNewBuffer;
    else
      mLastBuffer = aNewBuffer;

    mTotalDataLength += aNewBuffer->DataLength();
  }

void
nsSharedBufferList::SplitBuffer( const Position& aSplitPosition )
  {
    Buffer* bufferToSplit = aSplitPosition.mBuffer;

    NS_ASSERTION(bufferToSplit, "bufferToSplit");

    ptrdiff_t splitOffset = aSplitPosition.mPosInBuffer - bufferToSplit->DataStart();

    NS_ASSERTION(0 <= splitOffset && splitOffset <= bufferToSplit->DataLength(), "|splitOffset| within buffer");

    if ( (bufferToSplit->DataLength() >> 1) > splitOffset )
      {
        Buffer* new_buffer = NewBuffer(bufferToSplit->DataStart(), PRUint32(splitOffset));
        LinkBuffer(bufferToSplit->mPrev, new_buffer, bufferToSplit);
        bufferToSplit->DataStart(aSplitPosition.mPosInBuffer);
      }
    else
      {
        Buffer* new_buffer = NewBuffer(bufferToSplit->DataStart()+splitOffset, PRUint32(bufferToSplit->DataLength()-splitOffset));
        LinkBuffer(bufferToSplit, new_buffer, bufferToSplit->mNext);
        bufferToSplit->DataEnd(aSplitPosition.mPosInBuffer);
      }
  }


nsSharedBufferList::Buffer*
nsSharedBufferList::UnlinkBuffer( Buffer* aBufferToUnlink )
  {
    NS_ASSERTION(aBufferToUnlink, "aBufferToUnlink");

    Buffer* prev_buffer = aBufferToUnlink->mPrev;
    Buffer* next_buffer = aBufferToUnlink->mNext;

    if ( prev_buffer )
      prev_buffer->mNext = next_buffer;
    else
      mFirstBuffer = next_buffer;

    if ( next_buffer )
      next_buffer->mPrev = prev_buffer;
    else
      mLastBuffer = prev_buffer;

    mTotalDataLength -= aBufferToUnlink->DataLength();

    return aBufferToUnlink;
  }



#if 0
template <class CharT>
void
nsChunkList<CharT>::CutTrailingData( PRUint32 aLengthToCut )
  {
    Chunk* chunk = mLastChunk;
    while ( chunk && aLengthToCut )
      {
        Chunk* prev_chunk = chunk->mPrev;
        if ( aLengthToCut < chunk->mDataLength )
          {
            chunk->mDataLength -= aLengthToCut;
            aLengthToCut = 0;
          }
        else
          {
            RemoveChunk(chunk);
            aLengthToCut -= chunk->mDataLength;
            operator delete(chunk);
          }

        chunk = prev_chunk;
      }
  }
#endif
