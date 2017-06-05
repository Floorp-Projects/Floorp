/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INDEX_H_
#define INDEX_H_

#include "MediaData.h"
#include "MediaResource.h"
#include "TimeUnits.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/Stream.h"
#include "nsISupportsImpl.h"

template<class T> class nsAutoPtr;

namespace mp4_demuxer
{

class Index;
class IndiceWrapper;

typedef int64_t Microseconds;

class SampleIterator
{
public:
  explicit SampleIterator(Index* aIndex);
  ~SampleIterator();
  already_AddRefed<mozilla::MediaRawData> GetNext();
  void Seek(Microseconds aTime);
  Microseconds GetNextKeyframeTime();
private:
  Sample* Get();

  CencSampleEncryptionInfoEntry* GetSampleEncryptionEntry();

  void Next();
  RefPtr<Index> mIndex;
  friend class Index;
  size_t mCurrentMoof;
  size_t mCurrentSample;
};

class Index
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Index)

  struct Indice
  {
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t start_composition;
    uint64_t end_composition;
    uint64_t start_decode;
    bool sync;
  };

  struct MP4DataOffset
  {
    MP4DataOffset(uint32_t aIndex, int64_t aStartOffset)
      : mIndex(aIndex)
      , mStartOffset(aStartOffset)
      , mEndOffset(0)
    {}

    bool operator==(int64_t aStartOffset) const {
      return mStartOffset == aStartOffset;
    }

    bool operator!=(int64_t aStartOffset) const {
      return mStartOffset != aStartOffset;
    }

    bool operator<(int64_t aStartOffset) const {
      return mStartOffset < aStartOffset;
    }

    struct EndOffsetComparator {
      bool Equals(const MP4DataOffset& a, const int64_t& b) const {
        return a.mEndOffset == b;
      }

      bool LessThan(const MP4DataOffset& a, const int64_t& b) const {
        return a.mEndOffset < b;
      }
    };

    uint32_t mIndex;
    int64_t mStartOffset;
    int64_t mEndOffset;
    Interval<Microseconds> mTime;
  };

  Index(const IndiceWrapper& aIndices,
        Stream* aSource,
        uint32_t aTrackId,
        bool aIsAudio);

  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges,
                       bool aCanEvict);
  void UpdateMoofIndex(const mozilla::MediaByteRangeSet& aByteRanges);
  Microseconds GetEndCompositionIfBuffered(
    const mozilla::MediaByteRangeSet& aByteRanges);
  mozilla::media::TimeIntervals ConvertByteRangesToTimeRanges(
    const mozilla::MediaByteRangeSet& aByteRanges);
  uint64_t GetEvictionOffset(Microseconds aTime);
  bool IsFragmented() { return mMoofParser; }

  friend class SampleIterator;

private:
  ~Index();
  void RegisterIterator(SampleIterator* aIterator);
  void UnregisterIterator(SampleIterator* aIterator);

  Stream* mSource;
  FallibleTArray<Sample> mIndex;
  FallibleTArray<MP4DataOffset> mDataOffset;
  nsAutoPtr<MoofParser> mMoofParser;
  nsTArray<SampleIterator*> mIterators;

  // ConvertByteRangesToTimeRanges cache
  mozilla::MediaByteRangeSet mLastCachedRanges;
  mozilla::media::TimeIntervals mLastBufferedRanges;
  bool mIsAudio;
};
}

#endif
