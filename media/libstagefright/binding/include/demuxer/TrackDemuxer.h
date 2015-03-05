/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACK_DEMUXER_H_
#define TRACK_DEMUXER_H_

namespace mp4_demuxer { class MP4Sample; }

namespace mozilla {

class MediaByteRange;

typedef int64_t Microseconds;

class MediaSample {
public:
  explicit MediaSample(mp4_demuxer::MP4Sample* aMp4Sample) : mMp4Sample(aMp4Sample)
  {
  }

  nsAutoPtr<mp4_demuxer::MP4Sample> mMp4Sample;
};

class TrackDemuxer {
public:
  TrackDemuxer() {}
  virtual ~TrackDemuxer() {}

  virtual void Seek(Microseconds aTime) = 0;

  // DemuxSample returns nullptr on end of stream or error.
  virtual MediaSample* DemuxSample() = 0;

  // Returns timestamp of next keyframe, or -1 if demuxer can't
  // report this.
  virtual Microseconds GetNextKeyframeTime() = 0;
};

}

#endif
