
void
nsSlidingSharedBufferList::DiscardUnreferencedPrefix( Buffer* aRecentlyReleasedBuffer )
  {
    if ( aRecentlyReleasedBuffer == mFirstBuffer )
      {
        while ( mFirstBuffer && !mFirstBuffer->IsReferenced() )
          delete UnlinkBuffer(mFirstBuffer);
      }
  }


ptrdiff_t
Distance( const nsSharedBufferList::Position& aStart, const nsSharedBufferList::Position& aEnd )
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

nsSlidingSubstring::nsSlidingSubstring( nsSharedBufferList* aBufferList, const nsSharedBufferList::Position& aStart, const nsSharedBufferList::Position& aEnd )
    : mStart(aStart), mEnd(aEnd), mBufferList(aBufferList), mLength(Distance(aStart, aEnd))
  {
    mBufferList.AcquireReference();
  }

nsSlidingSubstring::nsSlidingSubstring( nsSharedBufferList* aBufferList )
    : /* mStart(aBufferList.First(), ...), mEnd(aEnd),*/ mBufferList(aBufferList), mLength(Distance(aStart, aEnd))
  {
    mBufferList.AcquireReference();
  }

nsSlidingSubstring::~nsSlidingSubstring()
  {
    mStart.mBuffer->ReleaseReference();
    mBufferList.DiscardUnreferencedPrefix(mStart.mBuffer);
    mBufferList.ReleaseReference();
  }

const PRUnichar*
nsSlidingSubstring::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset )
  {
    nsSlidingSharedBufferList::Buffer* buffer = 0;
    switch ( aRequest )
      {
        case kPrevFragment:
          buffer = NS_STATIC_CAST(nsSlidingSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mPrev;
          break;

        case kFirstFragment:
          buffer = mBufferList.mFirstBuffer;
          break;

        case kLastFragment:
          buffer = mBufferList.mLastBuffer;
          break;

        case kNextFragment:
          buffer = NS_STATIC_CAST(nsSlidingSharedBufferList::Buffer*, aFragment.mFragmentIdentifier)->mNext;
          break;

        case kFragmentAt:
          // ...work...
          break;
      }

    if ( buffer )
      {
        aFragment.mStart  = buffer->DataStart();
        aFragment.mEnd    = buffer->DataStart() + buffer->DataLength();
        aFragment.mFragmentIdentifier = buffer;
        return aFragment.mStart + aOffset;
      }

    return 0;
  }



nsSlidingString::nsSlidingString()
    : nsSlidingSubstring(new nsSlidingSharedBufferList)
  {
    // nothing else to do here
  }

void
nsSlidingString::AppendBuffer( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
  {
    Buffer* new_buffer = new Buffer(aStorageStart, aDataEnd, aStorageEnd);
    Buffer* old_last_buffer = mBufferList.Last();
    mBufferList.LinkBuffer(old_last_buffer, new_buffer, 0);
    mLength += new_buffer->DataLength();

    if ( !old_last_buffer )
      {
        // set up mStart and mEnd from the now non-empty list
      }
  }

void
nsSlidingString::DiscardPrefix( const nsReadingIterator<PRUnchar>& aIter )
  {
    Buffer* new_start_buffer = NS_REINTERPRET_CAST(Buffer*, aIter.fragment().mFragmentIdentifier);
    new_start_buffer->AcquireReference();

    Buffer* old_start_buffer = mStart.mBuffer;
    mStart.mBuffer = new_start_buffer;
    mStart.mPosInBuffer = aIter.get();

    old_start_buffer->ReleaseReference();

    mBufferList.DiscardUnreferencedPrefix(old_start_buffer);
  }
