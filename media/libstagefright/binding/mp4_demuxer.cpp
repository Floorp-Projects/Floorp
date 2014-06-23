/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/mp4_demuxer.h"

#include <stdint.h>

using namespace stagefright;

namespace mp4_demuxer
{

struct StageFrightPrivate
{
  sp<MPEG4Extractor> mExtractor;

  sp<MediaSource> mAudio;
  MediaSource::ReadOptions mAudioOptions;

  sp<MediaSource> mVideo;
  MediaSource::ReadOptions mVideoOptions;
};

class DataSourceAdapter : public DataSource
{
public:
  DataSourceAdapter(Stream* aSource) : mSource(aSource) {}

  ~DataSourceAdapter() {}

  virtual status_t initCheck() const { return NO_ERROR; }

  virtual ssize_t readAt(off64_t offset, void* data, size_t size)
  {
    MOZ_ASSERT(((ssize_t)size) >= 0);
    size_t bytesRead;
    if (!mSource->ReadAt(offset, data, size, &bytesRead))
      return ERROR_IO;

    if (bytesRead == 0)
      return ERROR_END_OF_STREAM;

    MOZ_ASSERT(((ssize_t)bytesRead) >= 0);
    return bytesRead;
  }

  virtual status_t getSize(off64_t* size)
  {
    if (!mSource->Length(size))
      return ERROR_UNSUPPORTED;
    return NO_ERROR;
  }

  virtual uint32_t flags() { return kWantsPrefetching | kIsHTTPBasedSource; }

  virtual status_t reconnectAtOffset(off64_t offset) { return NO_ERROR; }

private:
  nsAutoPtr<Stream> mSource;
};

MP4Demuxer::MP4Demuxer(Stream* source)
  : mPrivate(new StageFrightPrivate())
{
  mPrivate->mExtractor = new MPEG4Extractor(new DataSourceAdapter(source));
}

MP4Demuxer::~MP4Demuxer()
{
  if (mPrivate->mAudio.get()) {
    mPrivate->mAudio->stop();
  }
  if (mPrivate->mVideo.get()) {
    mPrivate->mVideo->stop();
  }
}

bool
MP4Demuxer::Init()
{
  sp<MediaExtractor> e = mPrivate->mExtractor;
  for (size_t i = 0; i < e->countTracks(); i++) {
    sp<MetaData> metaData = e->getTrackMetaData(i);

    const char* mimeType;
    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }

    if (!mPrivate->mAudio.get() && !strncmp(mimeType, "audio/", 6)) {
      mPrivate->mAudio = e->getTrack(i);
      mPrivate->mAudio->start();
      mAudioConfig.Update(metaData, mimeType);
    } else if (!mPrivate->mVideo.get() && !strncmp(mimeType, "video/", 6)) {
      mPrivate->mVideo = e->getTrack(i);
      mPrivate->mVideo->start();
      mVideoConfig.Update(metaData, mimeType);
    }
  }

  return mPrivate->mAudio.get() || mPrivate->mVideo.get();
}

bool
MP4Demuxer::HasValidAudio()
{
  return mPrivate->mAudio.get() && mAudioConfig.IsValid();
}

bool
MP4Demuxer::HasValidVideo()
{
  return mPrivate->mVideo.get() && mVideoConfig.IsValid();
}

Microseconds
MP4Demuxer::Duration()
{
  return std::max(mVideoConfig.duration, mAudioConfig.duration);
}

bool
MP4Demuxer::CanSeek()
{
  return mPrivate->mExtractor->flags() & MediaExtractor::CAN_SEEK;
}

void
MP4Demuxer::SeekAudio(Microseconds aTime)
{
  mPrivate->mAudioOptions.setSeekTo(
    aTime, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
}

void
MP4Demuxer::SeekVideo(Microseconds aTime)
{
  mPrivate->mVideoOptions.setSeekTo(
    aTime, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
}

MP4Sample*
MP4Demuxer::DemuxAudioSample()
{
  nsAutoPtr<MP4Sample> sample(new MP4Sample());
  status_t status =
    mPrivate->mAudio->read(&sample->mMediaBuffer, &mPrivate->mAudioOptions);
  mPrivate->mAudioOptions.clearSeekTo();

  if (status < 0) {
    return nullptr;
  }

  sample->Update();
  if (!Adts::ConvertEsdsToAdts(mAudioConfig.channel_count,
                               mAudioConfig.frequency_index,
                               mAudioConfig.aac_profile, sample)) {
    return nullptr;
  }

  return sample.forget();
}

MP4Sample*
MP4Demuxer::DemuxVideoSample()
{
  nsAutoPtr<MP4Sample> sample(new MP4Sample());
  status_t status =
    mPrivate->mVideo->read(&sample->mMediaBuffer, &mPrivate->mVideoOptions);
  mPrivate->mVideoOptions.clearSeekTo();

  if (status < 0) {
    return nullptr;
  }

  sample->Update();

  return sample.forget();
}

} // namespace mp4_demuxer
