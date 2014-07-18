/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_H_
#define MP4_DEMUXER_H_

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/Interval.h"

namespace mozilla { class MediaByteRange; }

namespace mp4_demuxer
{

struct StageFrightPrivate;
typedef int64_t Microseconds;

class Stream
{

public:
  virtual bool ReadAt(int64_t offset, void* data, size_t size,
                      size_t* bytes_read) = 0;
  virtual bool Length(int64_t* size) = 0;

  virtual ~Stream() {}
};

enum TrackType { kVideo = 1, kAudio };

class MP4Demuxer
{
public:
  MP4Demuxer(Stream* aSource);
  ~MP4Demuxer();

  bool Init();
  Microseconds Duration();
  bool CanSeek();

  bool HasValidAudio();
  bool HasValidVideo();

  void SeekAudio(Microseconds aTime);
  void SeekVideo(Microseconds aTime);

  // DemuxAudioSample and DemuxVideoSample functions
  // return nullptr on end of stream or error.
  MP4Sample* DemuxAudioSample();
  MP4Sample* DemuxVideoSample();

  const AudioDecoderConfig& AudioConfig() { return mAudioConfig; }
  const VideoDecoderConfig& VideoConfig() { return mVideoConfig; }

  void ConvertByteRangesToTime(
    const nsTArray<mozilla::MediaByteRange>& aByteRanges,
    nsTArray<Interval<Microseconds> >* aIntervals);

private:
  AudioDecoderConfig mAudioConfig;
  VideoDecoderConfig mVideoConfig;

  nsAutoPtr<StageFrightPrivate> mPrivate;
};
}

#endif
