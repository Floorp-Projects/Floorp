/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INDEX_H_
#define INDEX_H_

#include "MediaResource.h"
#include "mozilla/Monitor.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/Stream.h"
#include "nsISupportsImpl.h"

template<class T> class nsRefPtr;
template<class T> class nsAutoPtr;

namespace mp4_demuxer
{

class Index;
class MoofParser;
struct Sample;

typedef int64_t Microseconds;

class SampleIterator
{
public:
  explicit SampleIterator(Index* aIndex);
  already_AddRefed<mozilla::MediaRawData> GetNext();
  void Seek(Microseconds aTime);
  Microseconds GetNextKeyframeTime();

private:
  Sample* Get();
  void Next();
  nsRefPtr<Index> mIndex;
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
    bool sync;
  };

  Index(const nsTArray<Indice>& aIndex,
        Stream* aSource,
        uint32_t aTrackId,
        bool aIsAudio,
        mozilla::Monitor* aMonitor);

  void UpdateMoofIndex(const nsTArray<mozilla::MediaByteRange>& aByteRanges);
  Microseconds GetEndCompositionIfBuffered(
    const nsTArray<mozilla::MediaByteRange>& aByteRanges);
  void ConvertByteRangesToTimeRanges(
    const nsTArray<mozilla::MediaByteRange>& aByteRanges,
    nsTArray<Interval<Microseconds>>* aTimeRanges);
  uint64_t GetEvictionOffset(Microseconds aTime);
  bool IsFragmented() { return mMoofParser; }

  friend class SampleIterator;

private:
  ~Index();

  Stream* mSource;
  nsTArray<Sample> mIndex;
  nsAutoPtr<MoofParser> mMoofParser;
  mozilla::Monitor* mMonitor;
};
}

#endif
