/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_H_
#define MP4_DEMUXER_H_

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/Interval.h"
#include "nsISupportsImpl.h"
#include "mozilla/Monitor.h"

namespace mozilla { class MediaByteRange; }

namespace mp4_demuxer
{

using mozilla::Monitor;
struct StageFrightPrivate;
typedef int64_t Microseconds;

class Stream
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Stream);

  virtual bool ReadAt(int64_t offset, void* data, size_t size,
                      size_t* bytes_read) = 0;
  virtual bool CachedReadAt(int64_t offset, void* data, size_t size,
                            size_t* bytes_read) = 0;
  virtual bool Length(int64_t* size) = 0;

  virtual void DiscardBefore(int64_t offset) {}

protected:
  virtual ~Stream() {}
};

enum TrackType { kVideo = 1, kAudio };

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

  const CryptoFile& Crypto() { return mCrypto; }
  const mozilla::AudioInfo& AudioConfig() { return mAudioConfig; }
  const mozilla::VideoInfo& VideoConfig() { return mVideoConfig; }

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
  void UpdateCrypto(const stagefright::MetaData* aMetaData);
  MP4AudioInfo mAudioConfig;
  MP4VideoInfo mVideoConfig;
  CryptoFile mCrypto;

  nsAutoPtr<StageFrightPrivate> mPrivate;
  nsRefPtr<Stream> mSource;
  nsTArray<mozilla::MediaByteRange> mCachedByteRanges;
  nsTArray<Interval<Microseconds>> mCachedTimeRanges;
  Monitor* mMonitor;
  Microseconds mNextKeyframeTime;
};

} // namespace mozilla

#endif // MP4_DEMUXER_H_
