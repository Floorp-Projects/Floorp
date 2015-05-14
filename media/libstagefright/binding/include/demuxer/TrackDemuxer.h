/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACK_DEMUXER_H_
#define TRACK_DEMUXER_H_

template <class T> struct already_AddRefed;

namespace mozilla {

class MediaRawData;
class MediaByteRange;

class TrackDemuxer {
public:
  typedef int64_t Microseconds;

  TrackDemuxer() {}
  virtual ~TrackDemuxer() {}

  virtual void Seek(Microseconds aTime) = 0;

  // DemuxSample returns nullptr on end of stream or error.
  virtual already_AddRefed<MediaRawData> DemuxSample() = 0;

  // Returns timestamp of next keyframe, or -1 if demuxer can't
  // report this.
  virtual Microseconds GetNextKeyframeTime() = 0;
};

}

#endif
