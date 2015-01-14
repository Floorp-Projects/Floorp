/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Box.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "mozilla/Endian.h"

using namespace mozilla;

namespace mp4_demuxer {

Box::Box(BoxContext* aContext, uint64_t aOffset, const Box* aParent)
  : mContext(aContext), mParent(aParent)
{
  uint8_t header[8];
  MediaByteRange headerRange(aOffset, aOffset + sizeof(header));
  if (mParent && !mParent->mRange.Contains(headerRange)) {
    return;
  }

  const MediaByteRange* byteRange;
  for (int i = 0; ; i++) {
    if (i == mContext->mByteRanges.Length()) {
      return;
    }

    byteRange = &mContext->mByteRanges[i];
    if (byteRange->Contains(headerRange)) {
      break;
    }
  }

  size_t bytes;
  if (!mContext->mSource->CachedReadAt(aOffset, header, sizeof(header),
                                       &bytes) ||
      bytes != sizeof(header)) {
    return;
  }

  uint64_t size = BigEndian::readUint32(header);
  if (size == 1) {
    uint8_t bigLength[8];
    MediaByteRange bigLengthRange(headerRange.mEnd,
                                  headerRange.mEnd + sizeof(bigLength));
    if ((mParent && !mParent->mRange.Contains(bigLengthRange)) ||
        !byteRange->Contains(bigLengthRange) ||
        !mContext->mSource->CachedReadAt(aOffset, bigLength,
                                         sizeof(bigLength), &bytes) ||
        bytes != sizeof(bigLength)) {
      return;
    }
    size = BigEndian::readUint64(bigLength);
    mChildOffset = bigLengthRange.mEnd;
  } else {
    mChildOffset = headerRange.mEnd;
  }

  MediaByteRange boxRange(aOffset, aOffset + size);
  if (mChildOffset > boxRange.mEnd ||
      (mParent && !mParent->mRange.Contains(boxRange)) ||
      !byteRange->Contains(boxRange)) {
    return;
  }
  mRange = boxRange;
  mType = BigEndian::readUint32(&header[4]);
}

Box::Box()
  : mContext(nullptr)
{}

Box
Box::Next() const
{
  MOZ_ASSERT(IsAvailable());
  return Box(mContext, mRange.mEnd, mParent);
}

Box
Box::FirstChild() const
{
  MOZ_ASSERT(IsAvailable());
  if (mChildOffset == mRange.mEnd) {
    return Box();
  }
  return Box(mContext, mChildOffset, this);
}

void
Box::Read(nsTArray<uint8_t>* aDest)
{
  aDest->SetLength(mRange.mEnd - mChildOffset);
  size_t bytes;
  if (!mContext->mSource->CachedReadAt(mChildOffset, &(*aDest)[0],
                                       aDest->Length(), &bytes) ||
      bytes != aDest->Length()) {
    // Byte ranges are being reported incorrectly
    NS_WARNING("Read failed in mp4_demuxer::Box::Read()");
    aDest->Clear();
  }
}
}
