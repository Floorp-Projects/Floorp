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

#ifndef nsAlgorithm_h___
#include "nsAlgorithm.h"
  // for |copy_string|
#endif


ptrdiff_t
nsSharedBufferList::Position::Distance( const Position& aStart, const Position& aEnd )
  {
    ptrdiff_t result = 0;
    if ( aStart.mBuffer == aEnd.mBuffer )
      result = aEnd.mPosInBuffer - aStart.mPosInBuffer;
    else
      {
        result = aStart.mBuffer->DataEnd() - aStart.mPosInBuffer;
        for ( Buffer* b = aStart.mBuffer->mNext; b != aEnd.mBuffer; b = b->mNext )
          result += b->DataLength();
        result += aEnd.mPosInBuffer - aEnd.mBuffer->DataStart();
      }

    return result;
  }


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
nsSharedBufferList::SplitBuffer( const Position& aSplitPosition, SplitDisposition aSplitDirection )
  {
    Buffer* bufferToSplit = aSplitPosition.mBuffer;

    NS_ASSERTION(bufferToSplit, "bufferToSplit");

    Buffer::size_type splitOffset =
        aSplitPosition.mPosInBuffer - bufferToSplit->DataStart();

    NS_ASSERTION(aSplitPosition.mPosInBuffer >= bufferToSplit->DataStart() &&
                 splitOffset <= bufferToSplit->DataLength(),
                 "|splitOffset| within buffer");

      // if the caller specifically asked to split off the right side of
      // the buffer-to-be-split or else if they asked for the minimum
      // amount of work, and that turned out to be the right side...
    ptrdiff_t savedLength = mTotalDataLength;
    if ( aSplitDirection==kSplitCopyRightData ||
         ( aSplitDirection==kSplitCopyLeastData &&
           ((bufferToSplit->DataLength() >> 1) <= splitOffset) ) )
      {
          // ...then allocate a new buffer initializing it by copying all the data _after_ the split in the source buffer (i.e., `split right')
        Buffer* new_buffer = NewSingleAllocationBuffer(bufferToSplit->DataStart()+splitOffset, PRUint32(bufferToSplit->DataLength()-splitOffset));
        LinkBuffer(bufferToSplit, new_buffer, bufferToSplit->mNext);
        bufferToSplit->DataEnd(aSplitPosition.mPosInBuffer);
      }
    else
      {
          // ...else move the data _before_ the split point (i.e., `split left')
        Buffer* new_buffer = NewSingleAllocationBuffer(bufferToSplit->DataStart(), PRUint32(splitOffset));
        LinkBuffer(bufferToSplit->mPrev, new_buffer, bufferToSplit);
        bufferToSplit->DataStart(aSplitPosition.mPosInBuffer);
      }
    mTotalDataLength = savedLength; 
      // duh! splitting a buffer doesn't change the length.
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


void
nsSharedBufferList::DiscardSuffix( PRUint32 /* aLengthToDiscard */ )
  {
    // XXX
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
