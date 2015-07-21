/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/MP4TrackDemuxer.h"

using namespace mozilla;

namespace mp4_demuxer {

void
MP4AudioDemuxer::Seek(Microseconds aTime)
{
  mDemuxer->SeekAudio(aTime);
}

already_AddRefed<MediaRawData>
MP4AudioDemuxer::DemuxSample()
{
  nsRefPtr<MediaRawData> sample(mDemuxer->DemuxAudioSample());
  return sample.forget();
}

Microseconds
MP4AudioDemuxer::GetNextKeyframeTime()
{
  return -1;
}

void
MP4VideoDemuxer::Seek(Microseconds aTime)
{
  mDemuxer->SeekVideo(aTime);
}

already_AddRefed<MediaRawData>
MP4VideoDemuxer::DemuxSample()
{
  nsRefPtr<MediaRawData> sample(mDemuxer->DemuxVideoSample());
  return sample.forget();
}

Microseconds
MP4VideoDemuxer::GetNextKeyframeTime()
{
  return mDemuxer->GetNextKeyframeTime();
}

}
