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

template <class CharT>
struct ChunkList
  {
    struct Chunk
      {
        Chunk*  mNext;
        Chunk*  mPrev;

        CharT*    mStorageStart;
        PRUint32  mStorageLength;

        CharT*    mDataStart;
        PRUint32  mDataLength;

        Chunk( CharT* aStorageStart, PRUint32 aStorageLength, PRUint32 aDataLength=0, CharT* aDataStart=0 )
            : mStorageStart( aStorageStart ),
              mStorageLength( aStorageLength ),
              mDataLength( aDataLength ),
              mDataStart( aDataStart ? aDataStart, aStorageStart )
          {
            // nothing else to do here
          }
      };

    Chunk*  mFirstChunk;
    Chunk*  mLastChunk;


    static Chunk* NewChunk( const CharT* aData, PRUint32 aDataLength );

    Chunk*  InsertChunk( Chunk* aPrevElem, Chunk* aNewElem, Chunk* aNextElem );
    void    SplitChunk( Chunk* aElem, PRUint32 aSplitOffset );
    Chunk*  RemoveChunk( Chunk* aChunkToRemove );
  };


template <class CharT>
ChunkList<CharT>::Chunk*
ChunkList<CharT>::NewChunk( const CharT* aData, PRUint32 aDataLength, PRUint32 aAdditionalSpace = 0 )
  {
    size_t    object_size    = ((sizeof(Chunk) + sizeof(CharT) - 1) / sizeof(CharT)) * sizeof(CharT);
    PRUint32  buffer_length  = aDataLength + aAdditionalSpace;
    size_t    buffer_size    = size_t(buffer_length) * sizeof(CharT);

    void* object_ptr = operator new(object_size + buffer_size);
    if ( object_ptr )
      {
        typedef CharT* CharT_ptr;
        CharT* buffer_ptr = CharT_ptr(NS_STATIC_CAST(unsigned char*, object_ptr) + object_size);
        copy_string(aData, aData+aDataLength, buffer_ptr);
        return new (object_ptr) Chunk(buffer_ptr, buffer_length, aDataLength);
      }

    return 0;
  }



template <class CharT>
void
ChunkList<CharT>::InsertChunk( Chunk* aPrevElem, Chunk* aNewElem, Chunk* aNextElem )
  {
    // assert( aNewElem )
    // assert( aPrevElem || mFirstChunk == aNextElem )
    // assert( !aPrevElem || aPrevElem->mNext == aNextElem )
    // assert( aNextElem || mLastChunk == aPrevElem )
    // assert( !aNextElem || aNextElem->mPrev == aPrevElem )

    if ( aNewElem->mPrev = aPrevElem )
      aPrevElem->mNext = aNewElem;
    else
      mFirstChunk = aNewElem;

    if ( aNewElem->mNext = aNextElem )
      aNextElem->mPrev = aNewElem;
    else
      mLastChunk = aNewElem;
  }

template <class CharT>
void
ChunkList<CharT>::SplitChunk( Chunk* aChunkToSplit, PRUint32 aSplitPosition )
  {
    // assert( aChunkToSplit )
    // assert( aSplitPosition <= aChunkToSplit->mDataLength )

    if ( (aChunkToSplit->mDataLength >> 1) > aSplitPosition )
      {
        Chunk* new_chunk = NewChunk(aChunkToSplit->mDataStart, aSplitPosition);
        InsertChunk(aChunkToSplit->mPrev, new_chunk, aChunkToSplit);
        aChunkToSplit->mDataStart += aSplitPosition;
        aChunkToSplit->mDataLength -= aSplitPosition;
      }
    else
      {
        Chunk* new_chunk = NewChunk(aChunkToSplit->mDataStart+aSplitPosition, aChunkToSplit->mDataLength-aSplitPosition);
        InsertChunk(aChunkToSplit, new_chunk, aChunkToSplit->mNext);
        aChunkToSplit->mDataLength = aSplitPosition;
      }
  }


template <class CharT>
ChunkList<CharT>::Chunk*
ChunkList<CharT>::RemoveChunk( Chunk* aChunkToRemove )
  {
    Chunk* prev_chunk = aChunkToRemove->mPrev;
    Chunk* next_chunk = aChunkToRemove->mNext;

    if ( prev_chunk )
      prev_chunk->mNext = next_chunk;
    else
      mFirstChunk = next_chunk;

    if ( next_chunk )
      next_chunk->mPrev = prev_chunk;
    else
      mLastChunk = prev_chunk;

    return aChunkToRemove;
  }

...::SetupFragmentFromChunk( nsReadableFragment<CharT>& aFragment, ChunkList<CharT>::Chunk* aChunk, PRUint32 aOffset )
  {
    aFragment.mStart = aChunk->mDataStart;
    aFragment.mEnd = aChunk->mDataStart + aChunk->mDataLength;
    aFragment.mFragmentIdentifier = aChunk;
    return aChunk->mDataStart + aOffset;
  }

template <class CharT>
const CharT*
...::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    Chunk* chunk = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          chunk = NS_STATIC_CAST(Chunk*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          chunk = mChunkList.mFirstChunk;
          break;

        case kLastFragment:
          chunk = mChunkList.mLastChunk;
          break;

        case kNextFragment:
          chunk = NS_STATIC_CAST(Chunk*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }
    return chunk ? SetupFragmentFromChunk(aFragment, chunk, aOffset) : 0;
  }