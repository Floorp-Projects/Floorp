/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/SinfParser.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

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
  explicit RangeFinder(const MediaByteRangeSet& ranges)
    : mRanges(ranges), mIndex(0)
  {
    // Ranges must be normalised for this to work
  }

  bool Contains(MediaByteRange aByteRange);

private:
  const MediaByteRangeSet& mRanges;
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

already_AddRefed<MediaRawData> SampleIterator::GetNext()
{
  Sample* s(Get());
  if (!s) {
    return nullptr;
  }

  int64_t length = std::numeric_limits<int64_t>::max();
  mIndex->mSource->Length(&length);
  if (s->mByteRange.mEnd > length) {
    // We don't have this complete sample.
    return nullptr;
  }

  RefPtr<MediaRawData> sample = new MediaRawData();
  sample->mTimecode= s->mDecodeTime;
  sample->mTime = s->mCompositionRange.start;
  sample->mDuration = s->mCompositionRange.Length();
  sample->mOffset = s->mByteRange.mStart;
  sample->mKeyframe = s->mSync;

  nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
  // Do the blocking read
  if (!writer->SetSize(s->mByteRange.Length())) {
    return nullptr;
  }

  size_t bytesRead;
  if (!mIndex->mSource->ReadAt(sample->mOffset, writer->Data(), sample->Size(),
                               &bytesRead) || bytesRead != sample->Size()) {
    return nullptr;
  }

  if (!s->mCencRange.IsEmpty()) {
    MoofParser* parser = mIndex->mMoofParser.get();

    if (!parser || !parser->mSinf.IsValid()) {
      return nullptr;
    }

    uint8_t ivSize = parser->mSinf.mDefaultIVSize;

    // The size comes from an 8 bit field
    nsAutoTArray<uint8_t, 256> cenc;
    cenc.SetLength(s->mCencRange.Length());
    if (!mIndex->mSource->ReadAt(s->mCencRange.mStart, cenc.Elements(), cenc.Length(),
                                 &bytesRead) || bytesRead != cenc.Length()) {
      return nullptr;
    }
    ByteReader reader(cenc);
    writer->mCrypto.mValid = true;
    writer->mCrypto.mIVSize = ivSize;

    if (!reader.ReadArray(writer->mCrypto.mIV, ivSize)) {
      return nullptr;
    }

    if (reader.CanRead16()) {
      uint16_t count = reader.ReadU16();

      if (reader.Remaining() < count * 6) {
        return nullptr;
      }

      for (size_t i = 0; i < count; i++) {
        writer->mCrypto.mPlainSizes.AppendElement(reader.ReadU16());
        writer->mCrypto.mEncryptedSizes.AppendElement(reader.ReadU32());
      }
    } else {
      // No subsample information means the entire sample is encrypted.
      writer->mCrypto.mPlainSizes.AppendElement(0);
      writer->mCrypto.mEncryptedSizes.AppendElement(sample->Size());
    }
  }

  Next();

  return sample.forget();
}

Sample* SampleIterator::Get()
{
  if (!mIndex->mMoofParser) {
    MOZ_ASSERT(!mCurrentMoof);
    return mCurrentSample < mIndex->mIndex.Length()
      ? &mIndex->mIndex[mCurrentSample]
      : nullptr;
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
  SampleIterator itr(*this);
  Sample* sample;
  while (!!(sample = itr.Get())) {
    if (sample->mSync) {
      return sample->mCompositionRange.start;
    }
    itr.Next();
  }
  return -1;
}

Index::Index(const nsTArray<Indice>& aIndex,
             Stream* aSource,
             uint32_t aTrackId,
             bool aIsAudio,
             Monitor* aMonitor)
  : mSource(aSource)
  , mMonitor(aMonitor)
{
  if (aIndex.IsEmpty()) {
    mMoofParser = new MoofParser(aSource, aTrackId, aIsAudio, aMonitor);
  } else {
    if (!mIndex.SetCapacity(aIndex.Length(), fallible)) {
      // OOM.
      return;
    }
    for (size_t i = 0; i < aIndex.Length(); i++) {
      const Indice& indice = aIndex[i];
      Sample sample;
      sample.mByteRange = MediaByteRange(indice.start_offset,
                                         indice.end_offset);
      sample.mCompositionRange = Interval<Microseconds>(indice.start_composition,
                                                        indice.end_composition);
      sample.mDecodeTime = indice.start_decode;
      sample.mSync = indice.sync;
      // FIXME: Make this infallible after bug 968520 is done.
      MOZ_ALWAYS_TRUE(mIndex.AppendElement(sample, fallible));
    }
  }
}

Index::~Index() {}

void
Index::UpdateMoofIndex(const MediaByteRangeSet& aByteRanges)
{
  if (!mMoofParser) {
    return;
  }

  mMoofParser->RebuildFragmentedIndex(aByteRanges);
}

Microseconds
Index::GetEndCompositionIfBuffered(const MediaByteRangeSet& aByteRanges)
{
  FallibleTArray<Sample>* index;
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
  const MediaByteRangeSet& aByteRanges,
  nsTArray<Interval<Microseconds>>* aTimeRanges)
{
  RangeFinder rangeFinder(aByteRanges);
  nsTArray<Interval<Microseconds>> timeRanges;

  nsTArray<FallibleTArray<Sample>*> indexes;
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
    FallibleTArray<Sample>* index = indexes[i];
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
