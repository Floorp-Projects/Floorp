/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/ResourceStream.h"

namespace mp4_demuxer {

ResourceStream::ResourceStream(mozilla::MediaResource* aResource)
  : mResource(aResource)
  , mPinCount(0)
{
  MOZ_ASSERT(aResource);
}

ResourceStream::~ResourceStream()
{
  MOZ_ASSERT(mPinCount == 0);
}

bool
ResourceStream::ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                       size_t* aBytesRead)
{
  uint32_t sum = 0;
  uint32_t bytesRead = 0;
  do {
    uint64_t offset = aOffset + sum;
    char* buffer = reinterpret_cast<char*>(aBuffer) + sum;
    uint32_t toRead = aCount - sum;
    nsresult rv = mResource->ReadAt(offset, buffer, toRead, &bytesRead);
    if (NS_FAILED(rv)) {
      return false;
    }
    sum += bytesRead;
  } while (sum < aCount && bytesRead > 0);

  *aBytesRead = sum;
  return true;
}

bool
ResourceStream::CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                        size_t* aBytesRead)
{
  nsresult rv = mResource->ReadFromCache(reinterpret_cast<char*>(aBuffer),
                                         aOffset, aCount);
  if (NS_FAILED(rv)) {
    *aBytesRead = 0;
    return false;
  }
  *aBytesRead = aCount;
  return true;
}

bool
ResourceStream::Length(int64_t* aSize)
{
  if (mResource->GetLength() < 0)
    return false;
  *aSize = mResource->GetLength();
  return true;
}

} // namespace mp4_demuxer
