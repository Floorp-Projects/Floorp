
#include "nsSharedBufferList.h"


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


template <class CharT>
nsSharedBufferList::Buffer*
nsSharedBufferList::NewBuffer( const CharT* aData, PRUint32 aDataLength, PRUint32 aAdditionalSpace )
  {
    size_t    object_size    = ((sizeof(Buffer) + sizeof(CharT) - 1) / sizeof(CharT)) * sizeof(CharT);
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
        return new (object_ptr) Buffer(buffer_ptr, buffer_length, aDataLength);
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

    if ( aNewBuffer->mPrev = aPrevBuffer )
      aPrevBuffer->mNext = aNewBuffer;
    else
      mFirstBuffer = aNewBuffer;

    if ( aNewBuffer->mNext = aNextBuffer )
      aNextBuffer->mPrev = aNewBuffer;
    else
      mLastBuffer = aNewBuffer;

    mTotalDataLength += aNewBuffer->DataLength();
  }

void
nsSharedBufferList::SplitBuffer( Buffer* aBufferToSplit, PRUint32 aSplitPosition )
  {
    NS_ASSERTION(aBufferToSplit, "aBufferToSplit");
    NS_ASSERTION(aSplitPosition <= aBufferToSplit->DataLength(), "aSplitPosition <= aBufferToSplit->DataLength()");

    if ( (aBufferToSplit->DataLength() >> 1) > aSplitPosition )
      {
        Buffer* new_buffer = NewBuffer(aBufferToSplit->DataStart(), aSplitPosition);
        LinkBuffer(aBufferToSplit->mPrev, new_buffer, aBufferToSplit);
        aBufferToSplit->mDataStart += aSplitPosition;
        aBufferToSplit->DataLength() -= aSplitPosition;
      }
    else
      {
        Buffer* new_buffer = NewBuffer(aBufferToSplit->mDataStart+aSplitPosition, aBufferToSplit->DataLength()-aSplitPosition);
        LinkBuffer(aBufferToSplit, new_buffer, aBufferToSplit->mNext);
        aBufferToSplit->DataLength() = aSplitPosition;
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

