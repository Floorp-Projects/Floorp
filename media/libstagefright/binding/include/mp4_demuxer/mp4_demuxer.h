/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_H_
#define MP4_DEMUXER_H_

#include "MediaInfo.h"
#include "MediaResource.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/Interval.h"
#include "mp4_demuxer/Stream.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mp4_demuxer
{
class Index;
class MP4Metadata;
class SampleIterator;
using mozilla::Monitor;
typedef int64_t Microseconds;

class MP4Demuxer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MP4Demuxer)

  explicit MP4Demuxer(Stream* aSource, Monitor* aMonitor);

  bool Init();
  Microseconds Duration();
  bool CanSeek();

  bool HasValidAudio();
  bool HasValidVideo();

  void SeekAudio(Microseconds aTime);
  void SeekVideo(Microseconds aTime);

  // DemuxAudioSample and DemuxVideoSample functions
  // return nullptr on end of stream or error.
  already_AddRefed<mozilla::MediaRawData> DemuxAudioSample();
  already_AddRefed<mozilla::MediaRawData> DemuxVideoSample();

  const CryptoFile& Crypto() const;
  const mozilla::AudioInfo& AudioConfig() const { return *mAudioConfig->GetAsAudioInfo(); }
  const mozilla::VideoInfo& VideoConfig() const { return *mVideoConfig->GetAsVideoInfo(); }

  void UpdateIndex(const nsTArray<mozilla::MediaByteRange>& aByteRanges);

  void ConvertByteRangesToTime(
    const nsTArray<mozilla::MediaByteRange>& aByteRanges,
    nsTArray<Interval<Microseconds>>* aIntervals);

  int64_t GetEvictionOffset(Microseconds aTime);

  // Returns timestamp of next keyframe, or -1 if demuxer can't
  // report this.
  Microseconds GetNextKeyframeTime();

protected:
  ~MP4Demuxer();

private:
  mozilla::UniquePtr<mozilla::TrackInfo> mAudioConfig;
  mozilla::UniquePtr<mozilla::TrackInfo> mVideoConfig;

  nsRefPtr<Stream> mSource;
  nsTArray<mozilla::MediaByteRange> mCachedByteRanges;
  nsTArray<Interval<Microseconds>> mCachedTimeRanges;
  Monitor* mMonitor;
  Microseconds mNextKeyframeTime;
  mozilla::UniquePtr<MP4Metadata> mMetadata;
  mozilla::UniquePtr<SampleIterator> mAudioIterator;
  mozilla::UniquePtr<SampleIterator> mVideoIterator;
  nsTArray<nsRefPtr<Index>> mIndexes;
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_H_
