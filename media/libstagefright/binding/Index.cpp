/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/MoofParser.h"
#include "media/stagefright/MediaSource.h"
#include "MediaResource.h"

#include <algorithm>
#include <limits>

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
  explicit RangeFinder(const nsTArray<mozilla::MediaByteRange>& ranges)
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

SampleIterator::SampleIterator(Index* aIndex)
  : mIndex(aIndex)
  , mCurrentMoof(0)
  , mCurrentSample(0)
{
}

MP4Sample* SampleIterator::GetNext()
{
  Sample* s(Get());
  if (!s) {
    return nullptr;
  }

  nsAutoPtr<MP4Sample> sample(new MP4Sample());
  sample->decode_timestamp = s->mDecodeTime;
  sample->composition_timestamp = s->mCompositionRange.start;
  sample->duration = s->mCompositionRange.Length();
  sample->byte_offset = s->mByteRange.mStart;
  sample->is_sync_point = s->mSync;
  sample->size = s->mByteRange.Length();

  // Do the blocking read
  sample->data = sample->extra_buffer = new uint8_t[sample->size];

  size_t bytesRead;
  if (!mIndex->mSource->ReadAt(sample->byte_offset, sample->data, sample->size,
                               &bytesRead) || bytesRead != sample->size) {
    return nullptr;
  }

  if (!s->mCencRange.IsNull()) {
    // The size comes from an 8 bit field
    nsAutoTArray<uint8_t, 256> cenc;
    cenc.SetLength(s->mCencRange.Length());
    if (!mIndex->mSource->ReadAt(s->mCencRange.mStart, &cenc[0], cenc.Length(),
                                 &bytesRead) || bytesRead != cenc.Length()) {
      return nullptr;
    }
    ByteReader reader(cenc);
    sample->crypto.valid = true;
    reader.ReadArray(sample->crypto.iv, 16);
    if (reader.Remaining()) {
      uint16_t count = reader.ReadU16();
      for (size_t i = 0; i < count; i++) {
        sample->crypto.plain_sizes.AppendElement(reader.ReadU16());
        sample->crypto.encrypted_sizes.AppendElement(reader.ReadU32());
      }
      reader.ReadArray(sample->crypto.iv, 16);
      sample->crypto.iv_size = 16;
    }
  }

  Next();

  return sample.forget();
}

Sample* SampleIterator::Get()
{
  if (!mIndex->mMoofParser) {
    return nullptr;
  }

  nsTArray<Moof>& moofs = mIndex->mMoofParser->Moofs();
  while (true) {
    if (mCurrentMoof == moofs.Length()) {
      if (!mIndex->mMoofParser->BlockingReadNextMoof()) {
        return nullptr;
      }
      MOZ_ASSERT(mCurrentMoof < moofs.Length());
    }
    if (mCurrentSample < moofs[mCurrentMoof].mIndex.Length()) {
      break;
    }
    mCurrentSample = 0;
    ++mCurrentMoof;
  }
  return &moofs[mCurrentMoof].mIndex[mCurrentSample];
}

void SampleIterator::Next()
{
  ++mCurrentSample;
}

void SampleIterator::Seek(Microseconds aTime)
{
  size_t syncMoof = 0;
  size_t syncSample = 0;
  mCurrentMoof = 0;
  mCurrentSample = 0;
  Sample* sample;
  while (!!(sample = Get())) {
    if (sample->mCompositionRange.start > aTime) {
      break;
    }
    if (sample->mSync) {
      syncMoof = mCurrentMoof;
      syncSample = mCurrentSample;
    }
    if (sample->mCompositionRange.start == aTime) {
      break;
    }
    Next();
  }
  mCurrentMoof = syncMoof;
  mCurrentSample = syncSample;
}

Microseconds
SampleIterator::GetNextKeyframeTime()
{
  nsTArray<Moof>& moofs = mIndex->mMoofParser->Moofs();
  size_t sample = mCurrentSample + 1;
  size_t moof = mCurrentMoof;
  while (true) {
    while (true) {
      if (moof == moofs.Length()) {
        return -1;
      }
      if (sample < moofs[moof].mIndex.Length()) {
        break;
      }
      sample = 0;
      ++moof;
    }
    if (moofs[moof].mIndex[sample].mSync) {
      return moofs[moof].mIndex[sample].mDecodeTime;
    }
    ++sample;
  }
  MOZ_ASSERT(false); // should not be reached.
}

Index::Index(const stagefright::Vector<MediaSource::Indice>& aIndex,
             Stream* aSource, uint32_t aTrackId, Microseconds aTimestampOffset,
             Monitor* aMonitor)
  : mSource(aSource)
  , mMonitor(aMonitor)
{
  if (aIndex.isEmpty()) {
    mMoofParser = new MoofParser(aSource, aTrackId, aTimestampOffset, aMonitor);
  } else {
    for (size_t i = 0; i < aIndex.size(); i++) {
      const MediaSource::Indice& indice = aIndex[i];
      Sample sample;
      sample.mByteRange = MediaByteRange(indice.start_offset,
                                         indice.end_offset);
      sample.mCompositionRange = Interval<Microseconds>(indice.start_composition,
                                                        indice.end_composition);
      sample.mSync = indice.sync;
      mIndex.AppendElement(sample);
    }
  }
}

Index::~Index() {}

void
Index::UpdateMoofIndex(const nsTArray<MediaByteRange>& aByteRanges)
{
  if (!mMoofParser) {
    return;
  }

  mMoofParser->RebuildFragmentedIndex(aByteRanges);
}

Microseconds
Index::GetEndCompositionIfBuffered(const nsTArray<MediaByteRange>& aByteRanges)
{
  nsTArray<Sample>* index;
  if (mMoofParser) {
    if (!mMoofParser->ReachedEnd() || mMoofParser->Moofs().IsEmpty()) {
      return 0;
    }
    index = &mMoofParser->Moofs().LastElement().mIndex;
  } else {
    index = &mIndex;
  }

  Microseconds lastComposition = 0;
  RangeFinder rangeFinder(aByteRanges);
  for (size_t i = index->Length(); i--;) {
    const Sample& sample = (*index)[i];
    if (!rangeFinder.Contains(sample.mByteRange)) {
      return 0;
    }
    lastComposition = std::max(lastComposition, sample.mCompositionRange.end);
    if (sample.mSync) {
      return lastComposition;
    }
  }
  return 0;
}

void
Index::ConvertByteRangesToTimeRanges(
  const nsTArray<MediaByteRange>& aByteRanges,
  nsTArray<Interval<Microseconds>>* aTimeRanges)
{
  RangeFinder rangeFinder(aByteRanges);
  nsTArray<Interval<Microseconds>> timeRanges;

  nsTArray<nsTArray<Sample>*> indexes;
  if (mMoofParser) {
    // We take the index out of the moof parser and move it into a local
    // variable so we don't get concurrency issues. It gets freed when we
    // exit this function.
    for (int i = 0; i < mMoofParser->Moofs().Length(); i++) {
      Moof& moof = mMoofParser->Moofs()[i];

      // We need the entire moof in order to play anything
      if (rangeFinder.Contains(moof.mRange)) {
        if (rangeFinder.Contains(moof.mMdatRange)) {
          Interval<Microseconds>::SemiNormalAppend(timeRanges, moof.mTimeRange);
        } else {
          indexes.AppendElement(&moof.mIndex);
        }
      }
    }
  } else {
    indexes.AppendElement(&mIndex);
  }

  bool hasSync = false;
  for (size_t i = 0; i < indexes.Length(); i++) {
    nsTArray<Sample>* index = indexes[i];
    for (size_t j = 0; j < index->Length(); j++) {
      const Sample& sample = (*index)[j];
      if (!rangeFinder.Contains(sample.mByteRange)) {
        // We process the index in decode order so we clear hasSync when we hit
        // a range that isn't buffered.
        hasSync = false;
        continue;
      }

      hasSync |= sample.mSync;
      if (!hasSync) {
        continue;
      }

      Interval<Microseconds>::SemiNormalAppend(timeRanges,
                                               sample.mCompositionRange);
    }
  }

  // This fixes up when the compositon order differs from the byte range order
  Interval<Microseconds>::Normalize(timeRanges, aTimeRanges);
}

uint64_t
Index::GetEvictionOffset(Microseconds aTime)
{
  uint64_t offset = std::numeric_limits<uint64_t>::max();
  if (mMoofParser) {
    // We need to keep the whole moof if we're keeping any of it because the
    // parser doesn't keep parsed moofs.
    for (int i = 0; i < mMoofParser->Moofs().Length(); i++) {
      Moof& moof = mMoofParser->Moofs()[i];

      if (moof.mTimeRange.Length() && moof.mTimeRange.end > aTime) {
        offset = std::min(offset, uint64_t(std::min(moof.mRange.mStart,
                                                    moof.mMdatRange.mStart)));
      }
    }
  } else {
    // We've already parsed and stored the moov so we don't need to keep it.
    // All we need to keep is the sample data itself.
    for (size_t i = 0; i < mIndex.Length(); i++) {
      const Sample& sample = mIndex[i];
      if (aTime >= sample.mCompositionRange.end) {
        offset = std::min(offset, uint64_t(sample.mByteRange.mEnd));
      }
    }
  }
  return offset;
}
}
