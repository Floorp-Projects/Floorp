/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/SinfParser.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

#include <algorithm>
#include <limits>

using namespace stagefright;
using namespace mozilla;
using namespace mozilla::media;

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

  if (mRanges[mIndex].ContainsStrict(aByteRange)) {
    return true;
  }

  if (aByteRange.mStart < mRanges[mIndex].mStart) {
    // Search backwards
    do {
      if (!mIndex) {
        return false;
      }
      --mIndex;
      if (mRanges[mIndex].ContainsStrict(aByteRange)) {
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
    if (mRanges[mIndex].ContainsStrict(aByteRange)) {
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
  mIndex->RegisterIterator(this);
}

SampleIterator::~SampleIterator()
{
  mIndex->UnregisterIterator(this);
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
  sample->mTimecode= TimeUnit::FromMicroseconds(s->mDecodeTime);
  sample->mTime = TimeUnit::FromMicroseconds(s->mCompositionRange.start);
  sample->mDuration = TimeUnit::FromMicroseconds(s->mCompositionRange.Length());
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
    AutoTArray<uint8_t, 256> cenc;
    cenc.SetLength(s->mCencRange.Length());
    if (!mIndex->mSource->ReadAt(s->mCencRange.mStart, cenc.Elements(), cenc.Length(),
                                 &bytesRead) || bytesRead != cenc.Length()) {
      return nullptr;
    }
    ByteReader reader(cenc);
    writer->mCrypto.mValid = true;
    writer->mCrypto.mIVSize = ivSize;

    CencSampleEncryptionInfoEntry* sampleInfo = GetSampleEncryptionEntry();
    if (sampleInfo) {
      writer->mCrypto.mKeyId.AppendElements(sampleInfo->mKeyId);
    }

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

CencSampleEncryptionInfoEntry* SampleIterator::GetSampleEncryptionEntry()
{
  nsTArray<Moof>& moofs = mIndex->mMoofParser->Moofs();
  Moof* currentMoof = &moofs[mCurrentMoof];
  SampleToGroupEntry* sampleToGroupEntry = nullptr;

  // Default to using the sample to group entries for the fragment, otherwise
  // fall back to the sample to group entries for the track.
  nsTArray<SampleToGroupEntry>* sampleToGroupEntries =
    currentMoof->mFragmentSampleToGroupEntries.Length() != 0
    ? &currentMoof->mFragmentSampleToGroupEntries
    : &mIndex->mMoofParser->mTrackSampleToGroupEntries;

  uint32_t seen = 0;

  for (SampleToGroupEntry& entry : *sampleToGroupEntries) {
    if (seen + entry.mSampleCount > mCurrentSample) {
      sampleToGroupEntry = &entry;
      break;
    }
    seen += entry.mSampleCount;
  }

  // ISO-14496-12 Section 8.9.2.3 and 8.9.4 : group description index
  // (1) ranges from 1 to the number of sample group entries in the track
  // level SampleGroupDescription Box, or (2) takes the value 0 to
  // indicate that this sample is a member of no group, in this case, the
  // sample is associated with the default values specified in
  // TrackEncryption Box, or (3) starts at 0x10001, i.e. the index value
  // 1, with the value 1 in the top 16 bits, to reference fragment-local
  // SampleGroupDescription Box.

  // According to the spec, ISO-14496-12, the sum of the sample counts in this
  // box should be equal to the total number of samples, and, if less, the
  // reader should behave as if an extra SampleToGroupEntry existed, with
  // groupDescriptionIndex 0.

  if (!sampleToGroupEntry || sampleToGroupEntry->mGroupDescriptionIndex == 0) {
    return nullptr;
  }

  nsTArray<CencSampleEncryptionInfoEntry>* entries =
                      &mIndex->mMoofParser->mTrackSampleEncryptionInfoEntries;

  uint32_t groupIndex = sampleToGroupEntry->mGroupDescriptionIndex;

  // If the first bit is set to a one, then we should use the sample group
  // descriptions from the fragment.
  if (groupIndex > SampleToGroupEntry::kFragmentGroupDescriptionIndexBase) {
    groupIndex -= SampleToGroupEntry::kFragmentGroupDescriptionIndexBase;
    entries = &currentMoof->mFragmentSampleEncryptionInfoEntries;
  }

  // The group_index is one based.
  return groupIndex > entries->Length()
         ? nullptr
         : &entries->ElementAt(groupIndex - 1);
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

Index::Index(const IndiceWrapper& aIndices,
             Stream* aSource,
             uint32_t aTrackId,
             bool aIsAudio)
  : mSource(aSource)
  , mIsAudio(aIsAudio)
{
  if (!aIndices.Length()) {
    mMoofParser = new MoofParser(aSource, aTrackId, aIsAudio);
  } else {
    if (!mIndex.SetCapacity(aIndices.Length(), fallible)) {
      // OOM.
      return;
    }
    media::IntervalSet<int64_t> intervalTime;
    MediaByteRange intervalRange;
    bool haveSync = false;
    bool progressive = true;
    int64_t lastOffset = 0;
    for (size_t i = 0; i < aIndices.Length(); i++) {
      Indice indice;
      if (!aIndices.GetIndice(i, indice)) {
        // Out of index?
        return;
      }
      if (indice.sync || mIsAudio) {
        haveSync = true;
      }
      if (!haveSync) {
        continue;
      }

      Sample sample;
      sample.mByteRange = MediaByteRange(indice.start_offset,
                                         indice.end_offset);
      sample.mCompositionRange = Interval<Microseconds>(indice.start_composition,
                                                        indice.end_composition);
      sample.mDecodeTime = indice.start_decode;
      sample.mSync = indice.sync || mIsAudio;
      // FIXME: Make this infallible after bug 968520 is done.
      MOZ_ALWAYS_TRUE(mIndex.AppendElement(sample, fallible));
      if (indice.start_offset < lastOffset) {
        NS_WARNING("Chunks in MP4 out of order, expect slow down");
        progressive = false;
      }
      lastOffset = indice.end_offset;

      // Pack audio samples in group of 128.
      if (sample.mSync && progressive && (!mIsAudio || !(i % 128))) {
        if (mDataOffset.Length()) {
          auto& last = mDataOffset.LastElement();
          last.mEndOffset = intervalRange.mEnd;
          NS_ASSERTION(intervalTime.Length() == 1, "Discontinuous samples between keyframes");
          last.mTime.start = intervalTime.GetStart();
          last.mTime.end = intervalTime.GetEnd();
        }
        if (!mDataOffset.AppendElement(MP4DataOffset(mIndex.Length() - 1,
                                                     indice.start_offset),
                                       fallible)) {
          // OOM.
          return;
        }
        intervalTime = media::IntervalSet<int64_t>();
        intervalRange = MediaByteRange();
      }
      intervalTime += media::Interval<int64_t>(sample.mCompositionRange.start,
                                               sample.mCompositionRange.end);
      intervalRange = intervalRange.Span(sample.mByteRange);
    }

    if (mDataOffset.Length() && progressive) {
      Indice indice;
      if (!aIndices.GetIndice(aIndices.Length() - 1, indice)) {
        return;
      }
      auto& last = mDataOffset.LastElement();
      last.mEndOffset = indice.end_offset;
      last.mTime = Interval<int64_t>(intervalTime.GetStart(), intervalTime.GetEnd());
    } else {
      mDataOffset.Clear();
    }
  }
}

Index::~Index() {}

void
Index::UpdateMoofIndex(const MediaByteRangeSet& aByteRanges)
{
  UpdateMoofIndex(aByteRanges, false);
}

void
Index::UpdateMoofIndex(const MediaByteRangeSet& aByteRanges, bool aCanEvict)
{
  if (!mMoofParser) {
    return;
  }
  size_t moofs = mMoofParser->Moofs().Length();
  bool canEvict = aCanEvict && moofs > 1;
  if (canEvict) {
    // Check that we can trim the mMoofParser. We can only do so if all
    // iterators have demuxed all possible samples.
    for (const SampleIterator* iterator : mIterators) {
      if ((iterator->mCurrentSample == 0 && iterator->mCurrentMoof == moofs) ||
          iterator->mCurrentMoof == moofs - 1) {
        continue;
      }
      canEvict = false;
      break;
    }
  }
  mMoofParser->RebuildFragmentedIndex(aByteRanges, &canEvict);
  if (canEvict) {
    // The moofparser got trimmed. Adjust all registered iterators.
    for (SampleIterator* iterator : mIterators) {
      iterator->mCurrentMoof -= moofs - 1;
    }
  }
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

TimeIntervals
Index::ConvertByteRangesToTimeRanges(const MediaByteRangeSet& aByteRanges)
{
  if (aByteRanges == mLastCachedRanges) {
    return mLastBufferedRanges;
  }
  mLastCachedRanges = aByteRanges;

  if (mDataOffset.Length()) {
    TimeIntervals timeRanges;
    for (const auto& range : aByteRanges) {
      uint32_t start = mDataOffset.IndexOfFirstElementGt(range.mStart - 1);
      if (!mIsAudio && start == mDataOffset.Length()) {
        continue;
      }
      uint32_t end = mDataOffset.IndexOfFirstElementGt(range.mEnd, MP4DataOffset::EndOffsetComparator());
      if (!mIsAudio && end < start) {
        continue;
      }
      if (mIsAudio && start &&
          range.Intersects(MediaByteRange(mDataOffset[start-1].mStartOffset,
                                          mDataOffset[start-1].mEndOffset))) {
        // Check if previous audio data block contains some available samples.
        for (size_t i = mDataOffset[start-1].mIndex; i < mIndex.Length(); i++) {
          if (range.ContainsStrict(mIndex[i].mByteRange)) {
            timeRanges +=
              TimeInterval(TimeUnit::FromMicroseconds(mIndex[i].mCompositionRange.start),
                           TimeUnit::FromMicroseconds(mIndex[i].mCompositionRange.end));
          }
        }
      }
      if (end > start) {
        timeRanges +=
          TimeInterval(TimeUnit::FromMicroseconds(mDataOffset[start].mTime.start),
                       TimeUnit::FromMicroseconds(mDataOffset[end-1].mTime.end));
      }
      if (end < mDataOffset.Length()) {
        // Find samples in partial block contained in the byte range.
        for (size_t i = mDataOffset[end].mIndex;
             i < mIndex.Length() && range.ContainsStrict(mIndex[i].mByteRange);
             i++) {
          timeRanges +=
            TimeInterval(TimeUnit::FromMicroseconds(mIndex[i].mCompositionRange.start),
                         TimeUnit::FromMicroseconds(mIndex[i].mCompositionRange.end));
        }
      }
    }
    mLastBufferedRanges = timeRanges;
    return timeRanges;
  }

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
  nsTArray<Interval<Microseconds>> timeRangesNormalized;
  Interval<Microseconds>::Normalize(timeRanges, &timeRangesNormalized);
  // convert timeRanges.
  media::TimeIntervals ranges;
  for (size_t i = 0; i < timeRangesNormalized.Length(); i++) {
    ranges +=
      media::TimeInterval(media::TimeUnit::FromMicroseconds(timeRangesNormalized[i].start),
                          media::TimeUnit::FromMicroseconds(timeRangesNormalized[i].end));
  }
  mLastBufferedRanges = ranges;
  return ranges;
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

void
Index::RegisterIterator(SampleIterator* aIterator)
{
  mIterators.AppendElement(aIterator);
}

void
Index::UnregisterIterator(SampleIterator* aIterator)
{
  mIterators.RemoveElement(aIterator);
}

}
