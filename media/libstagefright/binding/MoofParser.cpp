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
      mInitRange = MediaByteRange(0, box.Range().mEnd);
      ParseMoov(box);
    } else if (box.IsType("moof")) {
      Moof moof(box, mTrex, mMdhd);

      if (!mMoofs.IsEmpty()) {
        // Stitch time ranges together in the case of a (hopefully small) time
        // range gap between moofs.
        mMoofs.LastElement().FixRounding(moof);
      }

      mMoofs.AppendElement(moof);
    }
    mOffset = box.NextOffset();
  }
}

Interval<Microseconds>
MoofParser::GetCompositionRange()
{
  Interval<Microseconds> compositionRange;
  for (size_t i = 0; i < mMoofs.Length(); i++) {
    compositionRange = compositionRange.Extents(mMoofs[i].mTimeRange);
  }
  return compositionRange;
}

bool
MoofParser::ReachedEnd()
{
  int64_t length;
  return mSource->Length(&length) && mOffset == length;
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
      if (!mTrex.mTrackId || tkhd.mTrackId == mTrex.mTrackId) {
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
      if (!mTrex.mTrackId || trex.mTrackId == mTrex.mTrackId) {
        mTrex = trex;
      }
    }
  }
}

Moof::Moof(Box& aBox, Trex& aTrex, Mdhd& aMdhd) :
    mRange(aBox.Range()), mMaxRoundingError(0)
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
      if (!aTrex.mTrackId || tfhd.mTrackId == aTrex.mTrackId) {
        tfdt = Tfdt(box);
      }
    } else if (box.IsType("trun")) {
      if (!aTrex.mTrackId || tfhd.mTrackId == aTrex.mTrackId) {
        ParseTrun(box, tfhd, tfdt, aMdhd);
      }
    }
  }
}

void
Moof::FixRounding(const Moof& aMoof) {
  Microseconds gap = aMoof.mTimeRange.start - mTimeRange.end;
  if (gap > 0 && gap <= mMaxRoundingError) {
    mTimeRange.end = aMoof.mTimeRange.start;
  }
}

class CtsComparator
{
public:
  bool Equals(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start == aB->mCompositionRange.start;
  }
  bool
  LessThan(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start < aB->mCompositionRange.start;
  }
};

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
  if (sampleCount == 0) {
    return;
  }

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

    Sample sample;
    sample.mByteRange = MediaByteRange(offset, offset + sampleSize);
    offset += sampleSize;

    sample.mCompositionRange = Interval<Microseconds>(
      aMdhd.ToMicroseconds(decodeTime + ctsOffset),
      aMdhd.ToMicroseconds(decodeTime + ctsOffset + sampleDuration));
    decodeTime += sampleDuration;

    sample.mSync = !(sampleFlags & 0x1010000);

    mIndex.AppendElement(sample);

    mMdatRange = mMdatRange.Extents(sample.mByteRange);
  }
  mMaxRoundingError += aMdhd.ToMicroseconds(sampleCount);

  nsTArray<Sample*> ctsOrder;
  for (int i = 0; i < mIndex.Length(); i++) {
    ctsOrder.AppendElement(&mIndex[i]);
  }
  ctsOrder.Sort(CtsComparator());

  for (int i = 0; i < ctsOrder.Length(); i++) {
    if (i + 1 < ctsOrder.Length()) {
      ctsOrder[i]->mCompositionRange.end = ctsOrder[i + 1]->mCompositionRange.start;
    }
  }
  mTimeRange = Interval<Microseconds>(ctsOrder[0]->mCompositionRange.start,
      ctsOrder.LastElement()->mCompositionRange.end);
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
