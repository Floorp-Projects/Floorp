/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Box.h"
#include "mp4_demuxer/Stream.h"
#include "mozilla/EndianUtils.h"
#include <algorithm>

using namespace mozilla;

namespace mp4_demuxer {

// Limit reads to 32MiB max.
// static
const uint64_t Box::kMAX_BOX_READ = 32 * 1024 * 1024;

// Returns the offset from the start of the body of a box of type |aType|
// to the start of its first child.
static uint32_t
BoxOffset(AtomType aType)
{
  const uint32_t FULLBOX_OFFSET = 4;

  if (aType == AtomType("mp4a") || aType == AtomType("enca")) {
    // AudioSampleEntry; ISO 14496-12, section 8.16
    return 28;
  } else if (aType == AtomType("mp4v") || aType == AtomType("encv")) {
    // VideoSampleEntry; ISO 14496-12, section 8.16
    return 78;
  } else if (aType == AtomType("stsd")) {
    // SampleDescriptionBox; ISO 14496-12, section 8.16
    // This is a FullBox, and contains a |count| member before its child
    // boxes.
    return FULLBOX_OFFSET + 4;
  }

  return 0;
}

Box::Box(BoxContext* aContext, uint64_t aOffset, const Box* aParent)
  : mContext(aContext), mParent(aParent)
{
  uint8_t header[8];

  if (aOffset > INT64_MAX - sizeof(header)) {
    return;
  }

  MediaByteRange headerRange(aOffset, aOffset + sizeof(header));
  if (mParent && !mParent->mRange.Contains(headerRange)) {
    return;
  }

  const MediaByteRange* byteRange;
  for (int i = 0; ; i++) {
    if (i == mContext->mByteRanges.Length()) {
      return;
    }

    byteRange = static_cast<const MediaByteRange*>(&mContext->mByteRanges[i]);
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
    if (aOffset > INT64_MAX - sizeof(header) - sizeof(bigLength)) {
      return;
    }
    MediaByteRange bigLengthRange(headerRange.mEnd,
                                  headerRange.mEnd + sizeof(bigLength));
    if ((mParent && !mParent->mRange.Contains(bigLengthRange)) ||
        !byteRange->Contains(bigLengthRange) ||
        !mContext->mSource->CachedReadAt(aOffset + sizeof(header), bigLength,
                                         sizeof(bigLength), &bytes) ||
        bytes != sizeof(bigLength)) {
      return;
    }
    size = BigEndian::readUint64(bigLength);
    mBodyOffset = bigLengthRange.mEnd;
  } else if (size == 0) {
    // box extends to end of file.
    size = mContext->mByteRanges.LastInterval().mEnd - aOffset;
    mBodyOffset = headerRange.mEnd;
  } else {
    mBodyOffset = headerRange.mEnd;
  }

  if (size > INT64_MAX) {
    return;
  }
  int64_t end = static_cast<int64_t>(aOffset) + static_cast<int64_t>(size);
  if (end < static_cast<int64_t>(aOffset)) {
    // Overflowed.
    return;
  }

  mType = BigEndian::readUint32(&header[4]);
  mChildOffset = mBodyOffset + BoxOffset(mType);

  MediaByteRange boxRange(aOffset, end);
  if (mChildOffset > boxRange.mEnd ||
      (mParent && !mParent->mRange.Contains(boxRange)) ||
      !byteRange->Contains(boxRange)) {
    return;
  }

  mRange = boxRange;
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

bool
Box::Read(nsTArray<uint8_t>* aDest)
{
  return Read(aDest, mRange);
}

bool
Box::Read(nsTArray<uint8_t>* aDest, const MediaByteRange& aRange)
{
  int64_t length;
  if (!mContext->mSource->Length(&length)) {
    // The HTTP server didn't give us a length to work with.
    // Limit the read to kMAX_BOX_READ max.
    length = std::min(aRange.mEnd - mChildOffset, kMAX_BOX_READ);
  } else {
    length = aRange.mEnd - mChildOffset;
  }
  aDest->SetLength(length);
  size_t bytes;
  if (!mContext->mSource->CachedReadAt(mChildOffset, aDest->Elements(),
                                       aDest->Length(), &bytes) ||
      bytes != aDest->Length()) {
    // Byte ranges are being reported incorrectly
    NS_WARNING("Read failed in mp4_demuxer::Box::Read()");
    aDest->Clear();
    return false;
  }
  return true;
}
}
