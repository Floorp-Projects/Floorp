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
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#ifndef nsFragmentedString_h___
#define nsFragmentedString_h___

  // WORK IN PROGRESS

#ifndef nsAWritableString_h___
#include "nsAWritableString.h"
#endif

template <class CharT>
struct nsChunkList
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
              mDataStart( aDataStart ? aDataStart : aStorageStart )
          {
            // nothing else to do here
          }
      };

    Chunk*    mFirstChunk;
    Chunk*    mLastChunk;
    PRUint32  mTotalDataLength;

    nsChunkList() : mFirstChunk(0), mLastChunk(0), mTotalDataLength(0) { }
   ~nsChunkList();

    static Chunk* NewChunk( const CharT* aData, PRUint32 aDataLength, PRUint32 aAdditionalSpace = 0 );

    void    InsertChunk( Chunk* aPrevElem, Chunk* aNewElem, Chunk* aNextElem );
    void    SplitChunk( Chunk* aElem, PRUint32 aSplitOffset );
    Chunk*  RemoveChunk( Chunk* aChunkToRemove );
    void    DestroyAllChunks();

    void    CutLeadingData( PRUint32 aLengthToCut );
    void    CutTrailingData( PRUint32 aLengthToCut );
  };


template <class CharT>
typename nsChunkList<CharT>::Chunk*
nsChunkList<CharT>::NewChunk( const CharT* aData, PRUint32 aDataLength, PRUint32 aAdditionalSpace )
  {
    size_t    object_size    = ((sizeof(Chunk) + sizeof(CharT) - 1) / sizeof(CharT)) * sizeof(CharT);
    PRUint32  buffer_length  = aDataLength + aAdditionalSpace;
    size_t    buffer_size    = size_t(buffer_length) * sizeof(CharT);

    void* object_ptr = operator new(object_size + buffer_size);
    if ( object_ptr )
      {
        typedef CharT* CharT_ptr;
        CharT* buffer_ptr = CharT_ptr(NS_STATIC_CAST(unsigned char*, object_ptr) + object_size);
        if ( aDataLength )
          {
            CharT* toBegin = buffer_ptr;
            copy_string(aData, aData+aDataLength, toBegin);
          }
        return new (object_ptr) Chunk(buffer_ptr, buffer_length, aDataLength);
      }

    return 0;
  }



template <class CharT>
void
nsChunkList<CharT>::InsertChunk( Chunk* aPrevElem, Chunk* aNewElem, Chunk* aNextElem )
  {
    NS_ASSERTION(aNewElem, "aNewElem");
    NS_ASSERTION(aPrevElem || mFirstChunk == aNextElem, "aPrevElem || mFirstChunk == aNextElem");
    NS_ASSERTION(!aPrevElem || aPrevElem->mNext == aNextElem, "!aPrevElem || aPrevElem->mNext == aNextElem");
    NS_ASSERTION(aNextElem || mLastChunk == aPrevElem, "aNextElem || mLastChunk == aPrevElem");
    NS_ASSERTION(!aNextElem || aNextElem->mPrev == aPrevElem, "!aNextElem || aNextElem->mPrev == aPrevElem");

    if ( aNewElem->mPrev = aPrevElem )
      aPrevElem->mNext = aNewElem;
    else
      mFirstChunk = aNewElem;

    if ( aNewElem->mNext = aNextElem )
      aNextElem->mPrev = aNewElem;
    else
      mLastChunk = aNewElem;

    mTotalDataLength += aNewElem->mDataLength;
  }

template <class CharT>
void
nsChunkList<CharT>::SplitChunk( Chunk* aChunkToSplit, PRUint32 aSplitPosition )
  {
    NS_ASSERTION(aChunkToSplit, "aChunkToSplit");
    NS_ASSERTION(aSplitPosition <= aChunkToSplit->mDataLength, "aSplitPosition <= aChunkToSplit->mDataLength");

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
typename nsChunkList<CharT>::Chunk*
nsChunkList<CharT>::RemoveChunk( Chunk* aChunkToRemove )
  {
    NS_ASSERTION(aChunkToRemove, "aChunkToRemove");

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

    mTotalDataLength -= aChunkToRemove->mDataLength;

    return aChunkToRemove;
  }

template <class CharT>
void
nsChunkList<CharT>::CutLeadingData( PRUint32 /* aLengthToCut */ )
  {
    // BULLSHIT ALERT
  }

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

template <class CharT>
void
nsChunkList<CharT>::DestroyAllChunks()
  {
      // destroy the entire list of hunks, without bothering to manage their links
    Chunk* next_chunk;
    for ( Chunk* cur_chunk=mFirstChunk; cur_chunk; cur_chunk=next_chunk )
      {
        next_chunk = cur_chunk->mNext;
        operator delete(cur_chunk);
      }
    mFirstChunk = mLastChunk = 0;
    mTotalDataLength = 0;
  }

template <class CharT>
nsChunkList<CharT>::~nsChunkList()
  {
    DestroyAllChunks();
  }


template <class CharT>
class basic_nsFragmentedString
      : public basic_nsAWritableString<CharT>
    /*
      ...
    */
  {
    protected:
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;
      virtual CharT* GetWritableFragment( nsWritableFragment<CharT>&, nsFragmentRequest, PRUint32 );

    public:
      basic_nsFragmentedString() { }

      virtual PRUint32 Length() const;

      virtual void SetLength( PRUint32 aNewLength );
      // virtual void SetCapacity( PRUint32 aNewCapacity );

      // virtual void Cut( PRUint32 cutStart, PRUint32 cutLength );

    protected:
      // virtual void do_AssignFromReadable( const basic_nsAReadableString<CharT>& );
      // virtual void do_AppendFromReadable( const basic_nsAReadableString<CharT>& );
      // virtual void do_InsertFromReadable( const basic_nsAReadableString<CharT>&, PRUint32 );
      // virtual void do_ReplaceFromReadable( PRUint32, PRUint32, const basic_nsAReadableString<CharT>& );

    private:
      nsChunkList<CharT>  mChunkList;
  };

template <class CharT>
const CharT*
basic_nsFragmentedString<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    typename nsChunkList<CharT>::Chunk* chunk = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          chunk = NS_STATIC_CAST(typename nsChunkList<CharT>::Chunk*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          chunk = mChunkList.mFirstChunk;
          break;

        case kLastFragment:
          chunk = mChunkList.mLastChunk;
          break;

        case kNextFragment:
          chunk = NS_STATIC_CAST(typename nsChunkList<CharT>::Chunk*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( chunk )
      {
        aFragment.mStart  = chunk->mDataStart;
        aFragment.mEnd    = chunk->mDataStart + chunk->mDataLength;
        aFragment.mFragmentIdentifier = chunk;
        return aFragment.mStart + aOffset;
      }

    return 0;
  }

template <class CharT>
CharT*
basic_nsFragmentedString<CharT>::GetWritableFragment( nsWritableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    typename nsChunkList<CharT>::Chunk* chunk = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          chunk = NS_STATIC_CAST(typename nsChunkList<CharT>::Chunk*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          chunk = mChunkList.mFirstChunk;
          break;

        case kLastFragment:
          chunk = mChunkList.mLastChunk;
          break;

        case kNextFragment:
          chunk = NS_STATIC_CAST(typename nsChunkList<CharT>::Chunk*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( chunk )
      {
        aFragment.mStart  = chunk->mDataStart;
        aFragment.mEnd    = chunk->mDataStart + chunk->mDataLength;
        aFragment.mFragmentIdentifier = chunk;
        return aFragment.mStart + aOffset;
      }

    return 0;
  }

  /**
   * ...
   */
template <class CharT>
PRUint32
basic_nsFragmentedString<CharT>::Length() const
  {
    return mChunkList.mTotalDataLength;
  }

  /**
   * |SetLength|
   */
template <class CharT>
void
basic_nsFragmentedString<CharT>::SetLength( PRUint32 aNewLength )
  {
    // according to the current interpretation of |SetLength|,
    //  cut off characters from the end, or else add unitialized space to fill

    if ( aNewLength < mChunkList.mTotalDataLength )
      {
//      if ( aNewLength )
          mChunkList.CutTrailingData(mChunkList.mTotalDataLength-aNewLength);
//      else
//        mChunkList.DestroyAllChunks();
      }

// temporarily... eliminate as soon as our munging routines don't need this form of |SetLength|
    else if ( aNewLength > mChunkList.mTotalDataLength )
      {
        size_t empty_space_to_add = aNewLength - mChunkList.mTotalDataLength;
        typename nsChunkList<CharT>::Chunk* new_chunk = nsChunkList<CharT>::NewChunk(0, 0, empty_space_to_add);
        new_chunk->mDataLength = empty_space_to_add;
        mChunkList.InsertChunk(mChunkList.mLastChunk, new_chunk, 0);
      }
  }


#if 0
  /**
   * |SetCapacity|.
   *
   * If a client tries to increase the capacity of multi-fragment string, perhaps a single
   * empty fragment of the appropriate size should be appended.
   */
template <class CharT>
void
basic_nsFragmentedString<CharT>::SetCapacity( PRUint32 aNewCapacity )
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

// void Cut( PRUint32 cutStart, PRUint32 cutLength );

// void do_AssignFromReadable( const basic_nsAReadableString<CharT>& );
// void do_AppendFromReadable( const basic_nsAReadableString<CharT>& );
// void do_InsertFromReadable( const basic_nsAReadableString<CharT>&, PRUint32 );
// void do_ReplaceFromReadable( PRUint32, PRUint32, const basic_nsAReadableString<CharT>& );

typedef basic_nsFragmentedString<PRUnichar> nsFragmentedString;
typedef basic_nsFragmentedString<char>      nsFragmentedCString;

#endif // !defined(nsFragmentedString_h___)
