/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/BufferStream.h"
#include "MediaResource.h"
#include <algorithm>

using namespace mozilla;

namespace mp4_demuxer {

BufferStream::BufferStream()
  : mStartOffset(0)
  , mLogicalLength(0)
  , mStartIndex(0)
{
}

/*virtual*/ bool
BufferStream::ReadAt(int64_t aOffset, void* aData, size_t aLength,
                     size_t* aBytesRead)
{
  if (aOffset < (mStartOffset + mStartIndex) ||
      mData.IsEmpty() ||
      aOffset > mStartOffset + mData.LastElement()->Length()) {
    return false;
  }
  *aBytesRead =
    std::min(aLength, size_t(mStartOffset + mData.Length() - aOffset));
  memcpy(aData, &mData[aOffset - mStartOffset], *aBytesRead);

  aOffset -= mStartOffset;
  size_t index = mStartIndex;
  *aBytesRead = 0;

  uint8_t* dest = (uint8_t*)aData;

  for (uint32_t i = 0; i < mData.Length() && aLength; i++) {
    ResourceItem* item = mData[i];
    if (aOffset >= index && aOffset < item->Length()) {
      size_t count = std::min(item->Length() - aOffset, (uint64_t)aLength);
      *aBytesRead += count;
      memcpy(dest, &item->mData[aOffset], count);
      dest += count;
      aLength -= count;
    }
    aOffset -= item->Length();
    index = 0;
  }
  return true;
}

/*virtual*/ bool
BufferStream::CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                           size_t* aBytesRead)
{
  return ReadAt(aOffset, aData, aLength, aBytesRead);
}

/*virtual*/ bool
BufferStream::Length(int64_t* aLength)
{
  *aLength = mLogicalLength;
  return true;
}

/* virtual */ void
BufferStream::DiscardBefore(int64_t aOffset)
{
  while (!mData.IsEmpty() &&
         mStartOffset + mData[0]->mData.Length() <= aOffset) {
    mStartOffset += mData[0]->mData.Length();
    mData.RemoveElementAt(0);
  }
  mStartIndex = aOffset - mStartOffset;
}

void
BufferStream::AppendBytes(const uint8_t* aData, size_t aLength)
{
  mData.AppendElement(new ResourceItem(aData, aLength));
}

void
BufferStream::AppendData(ResourceItem* aItem)
{
  mData.AppendElement(aItem);
}

MediaByteRange
BufferStream::GetByteRange()
{
  return MediaByteRange(mStartOffset + mStartIndex, mLogicalLength);
}
}
