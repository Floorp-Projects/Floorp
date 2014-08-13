/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/Box.h"

namespace mp4_demuxer
{

using namespace stagefright;
using namespace mozilla;

void
MoofParser::RebuildFragmentedIndex(const nsTArray<MediaByteRange>& aByteRanges)
{
  BoxContext context(mSource, aByteRanges);

  Box box(&context, mOffset);
  for (; box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moov")) {
      ParseMoov(box);
    } else if (box.IsType("moof")) {
      mMoofs.AppendElement(Moof(box, mTrex, mMdhd));
    }
    mOffset = box.NextOffset();
  }
}

void
MoofParser::ParseMoov(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trak")) {
      ParseTrak(box);
    } else if (box.IsType("mvex")) {
      ParseMvex(box);
    }
  }
}

void
MoofParser::ParseTrak(Box& aBox)
{
  Tkhd tkhd;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tkhd")) {
      tkhd = Tkhd(box);
    } else if (box.IsType("mdia")) {
      if (tkhd.mTrackId == mTrex.mTrackId) {
        ParseMdia(box, tkhd);
      }
    }
  }
}

void
MoofParser::ParseMdia(Box& aBox, Tkhd& aTkhd)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mdhd")) {
      mMdhd = Mdhd(box);
    }
  }
}

void
MoofParser::ParseMvex(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trex")) {
      Trex trex = Trex(box);
      if (trex.mTrackId == mTrex.mTrackId) {
        mTrex = trex;
      }
    }
  }
}

Moof::Moof(Box& aBox, Trex& aTrex, Mdhd& aMdhd) : mRange(aBox.Range())
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("traf")) {
      ParseTraf(box, aTrex, aMdhd);
    }
  }
}

void
Moof::ParseTraf(Box& aBox, Trex& aTrex, Mdhd& aMdhd)
{
  Tfhd tfhd(aTrex);
  Tfdt tfdt;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tfhd")) {
      tfhd = Tfhd(box, aTrex);
    } else if (box.IsType("tfdt")) {
      tfdt = Tfdt(box);
    } else if (box.IsType("trun")) {
      if (tfhd.mTrackId == aTrex.mTrackId) {
        ParseTrun(box, tfhd, tfdt, aMdhd);
      }
    }
  }
}

void
Moof::ParseTrun(Box& aBox, Tfhd& aTfhd, Tfdt& aTfdt, Mdhd& aMdhd)
{
  if (!aMdhd.mTimescale) {
    return;
  }

  BoxReader reader(aBox);
  uint32_t flags = reader->ReadU32();
  if ((flags & 0x404) == 0x404) {
    // Can't use these flags together
    reader->DiscardRemaining();
    return;
  }

  uint32_t sampleCount = reader->ReadU32();
  uint64_t offset = aTfhd.mBaseDataOffset + (flags & 1 ? reader->ReadU32() : 0);
  bool hasFirstSampleFlags = flags & 4;
  uint32_t firstSampleFlags = hasFirstSampleFlags ? reader->ReadU32() : 0;
  uint64_t decodeTime = aTfdt.mBaseMediaDecodeTime;
  nsTArray<Interval<Microseconds>> timeRanges;
  for (size_t i = 0; i < sampleCount; i++) {
    uint32_t sampleDuration =
      flags & 0x100 ? reader->ReadU32() : aTfhd.mDefaultSampleDuration;
    uint32_t sampleSize =
      flags & 0x200 ? reader->ReadU32() : aTfhd.mDefaultSampleSize;
    uint32_t sampleFlags =
      flags & 0x400 ? reader->ReadU32() : hasFirstSampleFlags && i == 0
                                            ? firstSampleFlags
                                            : aTfhd.mDefaultSampleFlags;
    uint32_t ctsOffset = flags & 0x800 ? reader->ReadU32() : 0;

    MediaSource::Indice indice;
    indice.start_offset = offset;
    offset += sampleSize;
    indice.end_offset = offset;

    indice.start_composition =
      ((decodeTime + ctsOffset) * 1000000ll) / aMdhd.mTimescale;
    decodeTime += sampleDuration;
    indice.end_composition =
      ((decodeTime + ctsOffset) * 1000000ll) / aMdhd.mTimescale;

    indice.sync = !(sampleFlags & 0x1010000);

    mIndex.AppendElement(indice);

    MediaByteRange compositionRange(indice.start_offset, indice.end_offset);
    if (mMdatRange.IsNull()) {
      mMdatRange = compositionRange;
    } else {
      mMdatRange = mMdatRange.Extents(compositionRange);
    }
    Interval<Microseconds>::SemiNormalAppend(
      timeRanges,
      Interval<Microseconds>(indice.start_composition, indice.end_composition));
  }
  Interval<Microseconds>::Normalize(timeRanges, &mTimeRanges);
}

Tkhd::Tkhd(Box& aBox)
{
  BoxReader reader(aBox);
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  if (version == 0) {
    mCreationTime = reader->ReadU32();
    mModificationTime = reader->ReadU32();
    mTrackId = reader->ReadU32();
    uint32_t reserved = reader->ReadU32();
    NS_ASSERTION(!reserved, "reserved should be 0");
    mDuration = reader->ReadU32();
  } else if (version == 1) {
    mCreationTime = reader->ReadU64();
    mModificationTime = reader->ReadU64();
    mTrackId = reader->ReadU32();
    uint32_t reserved = reader->ReadU32();
    NS_ASSERTION(!reserved, "reserved should be 0");
    mDuration = reader->ReadU64();
  }
  // More stuff that we don't care about
  reader->DiscardRemaining();
}

Mdhd::Mdhd(Box& aBox)
{
  BoxReader reader(aBox);
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  if (version == 0) {
    mCreationTime = reader->ReadU32();
    mModificationTime = reader->ReadU32();
    mTimescale = reader->ReadU32();
    mDuration = reader->ReadU32();
  } else if (version == 1) {
    mCreationTime = reader->ReadU64();
    mModificationTime = reader->ReadU64();
    mTimescale = reader->ReadU32();
    mDuration = reader->ReadU64();
  }
  // language and pre_defined=0
  reader->ReadU32();
}

Trex::Trex(Box& aBox)
{
  BoxReader reader(aBox);
  mFlags = reader->ReadU32();
  mTrackId = reader->ReadU32();
  mDefaultSampleDescriptionIndex = reader->ReadU32();
  mDefaultSampleDuration = reader->ReadU32();
  mDefaultSampleSize = reader->ReadU32();
  mDefaultSampleFlags = reader->ReadU32();
}

Tfhd::Tfhd(Box& aBox, Trex& aTrex) : Trex(aTrex)
{
  MOZ_ASSERT(aBox.IsType("tfhd"));
  MOZ_ASSERT(aBox.Parent()->IsType("traf"));
  MOZ_ASSERT(aBox.Parent()->Parent()->IsType("moof"));

  BoxReader reader(aBox);
  mFlags = reader->ReadU32();
  mBaseDataOffset =
    mFlags & 1 ? reader->ReadU32() : aBox.Parent()->Parent()->Offset();
  mTrackId = reader->ReadU32();
  if (mFlags & 2) {
    mDefaultSampleDescriptionIndex = reader->ReadU32();
  }
  if (mFlags & 8) {
    mDefaultSampleDuration = reader->ReadU32();
  }
  if (mFlags & 0x10) {
    mDefaultSampleSize = reader->ReadU32();
  }
  if (mFlags & 0x20) {
    mDefaultSampleFlags = reader->ReadU32();
  }
}

Tfdt::Tfdt(Box& aBox)
{
  BoxReader reader(aBox);
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  if (version == 0) {
    mBaseMediaDecodeTime = reader->ReadU32();
  } else if (version == 1) {
    mBaseMediaDecodeTime = reader->ReadU64();
  }
  reader->DiscardRemaining();
}
}
