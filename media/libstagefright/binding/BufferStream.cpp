/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/BufferStream.h"
#include "MediaData.h"
#include "MediaResource.h"
#include <algorithm>

using namespace mozilla;

namespace mp4_demuxer {

BufferStream::BufferStream()
  : mStartOffset(0)
  , mData(new mozilla::MediaByteBuffer)
{
}

BufferStream::BufferStream(mozilla::MediaByteBuffer* aBuffer)
  : mStartOffset(0)
  , mData(aBuffer)
{
}

BufferStream::~BufferStream()
{
}

/*virtual*/ bool
BufferStream::ReadAt(int64_t aOffset, void* aData, size_t aLength,
                     size_t* aBytesRead)
{
  if (aOffset < mStartOffset || aOffset > mStartOffset + mData->Length()) {
    return false;
  }
  *aBytesRead =
    std::min(aLength, size_t(mStartOffset + mData->Length() - aOffset));
  memcpy(aData, mData->Elements() + aOffset - mStartOffset, *aBytesRead);
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
  *aLength = mStartOffset + mData->Length();
  return true;
}

/* virtual */ void
BufferStream::DiscardBefore(int64_t aOffset)
{
  if (aOffset > mStartOffset) {
    mData->RemoveElementsAt(0, aOffset - mStartOffset);
    mStartOffset = aOffset;
  }
}

bool
BufferStream::AppendBytes(const uint8_t* aData, size_t aLength)
{
  return mData->AppendElements(aData, aLength, fallible);
}

MediaByteRange
BufferStream::GetByteRange()
{
  return MediaByteRange(mStartOffset, mStartOffset + mData->Length());
}
}
