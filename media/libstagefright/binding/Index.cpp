/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/MoofParser.h"
#include "media/stagefright/MediaSource.h"
#include "MediaResource.h"

using namespace stagefright;
using namespace mozilla;

namespace mp4_demuxer
{

class MOZ_STACK_CLASS RangeFinder
{
public:
  // Given that we're processing this in order we don't use a binary search
  // to find the apropriate time range. Instead we search linearly from the
  // last used point.
  RangeFinder(const nsTArray<mozilla::MediaByteRange>& ranges)
    : mRanges(ranges), mIndex(0)
  {
    // Ranges must be normalised for this to work
  }

  bool Contains(MediaByteRange aByteRange);

private:
  const nsTArray<MediaByteRange>& mRanges;
  size_t mIndex;
};

bool
RangeFinder::Contains(MediaByteRange aByteRange)
{
  if (!mRanges.Length()) {
    return false;
  }

  if (mRanges[mIndex].Contains(aByteRange)) {
    return true;
  }

  if (aByteRange.mStart < mRanges[mIndex].mStart) {
    // Search backwards
    do {
      if (!mIndex) {
        return false;
      }
      --mIndex;
      if (mRanges[mIndex].Contains(aByteRange)) {
        return true;
      }
    } while (aByteRange.mStart < mRanges[mIndex].mStart);

    return false;
  }

  while (aByteRange.mEnd > mRanges[mIndex].mEnd) {
    if (mIndex == mRanges.Length() - 1) {
      return false;
    }
    ++mIndex;
    if (mRanges[mIndex].Contains(aByteRange)) {
      return true;
    }
  }

  return false;
}

Index::Index(const stagefright::Vector<MediaSource::Indice>& aIndex,
             Stream* aSource, uint32_t aTrackId)
  : mMonitor("mp4_demuxer::Index")
{
  if (aIndex.isEmpty()) {
    mMoofParser = new MoofParser(aSource, aTrackId);
  } else {
    mIndex.AppendElements(&aIndex[0], aIndex.size());
  }
}

Index::~Index() {}

void
Index::ConvertByteRangesToTimeRanges(
  const nsTArray<MediaByteRange>& aByteRanges,
  nsTArray<Interval<Microseconds>>* aTimeRanges)
{
  RangeFinder rangeFinder(aByteRanges);
  nsTArray<Interval<Microseconds>> timeRanges;

  nsTArray<stagefright::MediaSource::Indice> moofIndex;
  nsTArray<stagefright::MediaSource::Indice>* index;
  if (mMoofParser) {
    {
      MonitorAutoLock mon(mMonitor);
      mMoofParser->RebuildFragmentedIndex(aByteRanges);

      // We take the index out of the moof parser and move it into a local
      // variable so we don't get concurrency issues. It gets freed when we
      // exit this function.
      for (int i = 0; i < mMoofParser->mMoofs.Length(); i++) {
        Moof& moof = mMoofParser->mMoofs[i];
        if (rangeFinder.Contains(moof.mRange) &&
            rangeFinder.Contains(moof.mMdatRange)) {
          timeRanges.AppendElements(moof.mTimeRanges);
        } else {
          moofIndex.AppendElements(mMoofParser->mMoofs[i].mIndex);
        }
      }
    }
    index = &moofIndex;
  } else {
    index = &mIndex;
  }

  bool hasSync = false;
  for (size_t i = 0; i < index->Length(); i++) {
    const MediaSource::Indice& indice = (*index)[i];
    if (!rangeFinder.Contains(MediaByteRange(indice.start_offset,
                                             indice.end_offset))) {
      // We process the index in decode order so we clear hasSync when we hit
      // a range that isn't buffered.
      hasSync = false;
      continue;
    }

    hasSync |= indice.sync;
    if (!hasSync) {
      continue;
    }

    // This is an optimisation for when the file is decoded in composition
    // order. It means that Normalise() below doesn't need to do a sort.
    size_t s = timeRanges.Length();
    if (s && timeRanges[s - 1].end == indice.start_composition) {
      timeRanges[s - 1].end = indice.end_composition;
    } else {
      timeRanges.AppendElement(Interval<Microseconds>(indice.start_composition,
                                                      indice.end_composition));
    }
  }

  // This fixes up when the compositon order differs from the byte range order
  Interval<Microseconds>::Normalize(timeRanges, aTimeRanges);
}
}
