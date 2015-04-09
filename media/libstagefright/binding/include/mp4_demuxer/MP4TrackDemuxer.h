/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_TRACK_DEMUXER_H_
#define MP4_TRACK_DEMUXER_H_

#include "mozilla/Attributes.h"
#include "demuxer/TrackDemuxer.h"

namespace mp4_demuxer
{

class MP4AudioDemuxer : public mozilla::TrackDemuxer {
public:
  explicit MP4AudioDemuxer(MP4Demuxer* aDemuxer) : mDemuxer(aDemuxer) {}
  virtual void Seek(Microseconds aTime) override;
  virtual already_AddRefed<mozilla::MediaRawData> DemuxSample() override;
  virtual Microseconds GetNextKeyframeTime() override;

private:
  nsRefPtr<MP4Demuxer> mDemuxer;
};

class MP4VideoDemuxer : public mozilla::TrackDemuxer {
public:
  explicit MP4VideoDemuxer(MP4Demuxer* aDemuxer) : mDemuxer(aDemuxer) {}
  virtual void Seek(Microseconds aTime) override;
  virtual already_AddRefed<mozilla::MediaRawData> DemuxSample() override;
  virtual Microseconds GetNextKeyframeTime() override;

private:
  nsRefPtr<MP4Demuxer> mDemuxer;
};

}

#endif
