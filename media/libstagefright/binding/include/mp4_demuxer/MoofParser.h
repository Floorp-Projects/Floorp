/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOOF_PARSER_H_
#define MOOF_PARSER_H_

#include "media/stagefright/MediaSource.h"
#include "mp4_demuxer/mp4_demuxer.h"

namespace mozilla { class MediaByteRange; }

namespace mp4_demuxer {

class Stream;
class Box;

class Tkhd
{
public:
  Tkhd()
    : mCreationTime(0)
    , mModificationTime(0)
    , mTrackId(0)
    , mDuration(0)
  {
  }
  Tkhd(Box& aBox);

  uint64_t mCreationTime;
  uint64_t mModificationTime;
  uint32_t mTrackId;
  uint64_t mDuration;
};

class Mdhd
{
public:
  Mdhd()
    : mCreationTime(0)
    , mModificationTime(0)
    , mTimescale(0)
    , mDuration(0)
  {
  }
  Mdhd(Box& aBox);

  uint64_t mCreationTime;
  uint64_t mModificationTime;
  uint32_t mTimescale;
  uint64_t mDuration;
};

class Trex
{
public:
  Trex()
    : mFlags(0)
    , mTrackId(0)
    , mDefaultSampleDescriptionIndex(0)
    , mDefaultSampleDuration(0)
    , mDefaultSampleSize(0)
    , mDefaultSampleFlags(0)
  {
  }

  Trex(Box& aBox);

  uint32_t mFlags;
  uint32_t mTrackId;
  uint32_t mDefaultSampleDescriptionIndex;
  uint32_t mDefaultSampleDuration;
  uint32_t mDefaultSampleSize;
  uint32_t mDefaultSampleFlags;
};

class Tfhd : public Trex
{
public:
  Tfhd(Trex& aTrex) : Trex(aTrex), mBaseDataOffset(0) {}
  Tfhd(Box& aBox, Trex& aTrex);

  uint64_t mBaseDataOffset;
};

class Tfdt
{
public:
  Tfdt() : mBaseMediaDecodeTime(0) {}
  Tfdt(Box& aBox);

  uint64_t mBaseMediaDecodeTime;
};

class MoofParser
{
public:
  MoofParser(Stream* aSource, uint32_t aTrackId)
    : mSource(aSource), mTrackId(aTrackId)
  {
  }
  void RebuildFragmentedIndex(
    const nsTArray<mozilla::MediaByteRange>& aByteRanges);
  void ParseMoov(Box& aBox);
  void ParseTrak(Box& aBox);
  void ParseMdia(Box& aBox, Tkhd& aTkhd);
  void ParseMvex(Box& aBox);

  nsRefPtr<Stream> mSource;
  uint32_t mTrackId;
  nsTArray<uint64_t> mMoofOffsets;
  Mdhd mMdhd;
  Trex mTrex;
  Tfdt mTfdt;
  nsTArray<stagefright::MediaSource::Indice> mIndex;
};
}

#endif
