/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "mp4_demuxer/Index.h"
#include "MediaResource.h"

#include <stdint.h>
#include <algorithm>
#include <limits>

using namespace stagefright;

namespace mp4_demuxer
{

struct StageFrightPrivate
{
  sp<MPEG4Extractor> mExtractor;

  sp<MediaSource> mAudio;
  MediaSource::ReadOptions mAudioOptions;
  nsAutoPtr<SampleIterator> mAudioIterator;

  sp<MediaSource> mVideo;
  MediaSource::ReadOptions mVideoOptions;
  nsAutoPtr<SampleIterator> mVideoIterator;

  nsTArray<nsRefPtr<Index>> mIndexes;
};

class DataSourceAdapter : public DataSource
{
public:
  explicit DataSourceAdapter(Stream* aSource) : mSource(aSource) {}

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

    MOZ_ASSERT(((ssize_t)bytesRead) > 0);
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
  nsRefPtr<Stream> mSource;
};

MP4Demuxer::MP4Demuxer(Stream* source, Microseconds aTimestampOffset, Monitor* aMonitor)
  : mPrivate(new StageFrightPrivate()), mSource(source),
    mTimestampOffset(aTimestampOffset), mMonitor(aMonitor)
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
  mMonitor->AssertCurrentThreadOwns();
  sp<MediaExtractor> e = mPrivate->mExtractor;

  // Read the number of tracks. If we can't find any, make sure to bail now before
  // attempting any new reads to make the retry system work.
  size_t trackCount = e->countTracks();
  if (trackCount == 0) {
    return false;
  }

  for (size_t i = 0; i < trackCount; i++) {
    sp<MetaData> metaData = e->getTrackMetaData(i);

    const char* mimeType;
    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }

    if (!mPrivate->mAudio.get() && !strncmp(mimeType, "audio/", 6)) {
      sp<MediaSource> track = e->getTrack(i);
      if (track->start() != OK) {
        return false;
      }
      mPrivate->mAudio = track;
      mAudioConfig.Update(metaData, mimeType);
      nsRefPtr<Index> index = new Index(mPrivate->mAudio->exportIndex(),
                                        mSource, mAudioConfig.mTrackId,
                                        mTimestampOffset, mMonitor);
      mPrivate->mIndexes.AppendElement(index);
      if (index->IsFragmented() && !mAudioConfig.crypto.valid) {
        mPrivate->mAudioIterator = new SampleIterator(index);
      }
    } else if (!mPrivate->mVideo.get() && !strncmp(mimeType, "video/", 6)) {
      sp<MediaSource> track = e->getTrack(i);
      if (track->start() != OK) {
        return false;
      }
      mPrivate->mVideo = track;
      mVideoConfig.Update(metaData, mimeType);
      nsRefPtr<Index> index = new Index(mPrivate->mVideo->exportIndex(),
                                        mSource, mVideoConfig.mTrackId,
                                        mTimestampOffset, mMonitor);
      mPrivate->mIndexes.AppendElement(index);
      if (index->IsFragmented() && !mVideoConfig.crypto.valid) {
        mPrivate->mVideoIterator = new SampleIterator(index);
      }
    }
  }
  sp<MetaData> metaData = e->getMetaData();
  mCrypto.Update(metaData);

  int64_t movieDuration;
  if (!mVideoConfig.duration && !mAudioConfig.duration &&
      metaData->findInt64(kKeyMovieDuration, &movieDuration)) {
    // No duration were found in either tracks, use movie extend header box one.
    mVideoConfig.duration = mAudioConfig.duration = movieDuration;
  }

  return mPrivate->mAudio.get() || mPrivate->mVideo.get();
}

bool
MP4Demuxer::HasValidAudio()
{
  mMonitor->AssertCurrentThreadOwns();
  return mPrivate->mAudio.get() && mAudioConfig.IsValid();
}

bool
MP4Demuxer::HasValidVideo()
{
  mMonitor->AssertCurrentThreadOwns();
  return mPrivate->mVideo.get() && mVideoConfig.IsValid();
}

Microseconds
MP4Demuxer::Duration()
{
  mMonitor->AssertCurrentThreadOwns();
  return std::max(mVideoConfig.duration, mAudioConfig.duration);
}

bool
MP4Demuxer::CanSeek()
{
  mMonitor->AssertCurrentThreadOwns();
  return mPrivate->mExtractor->flags() & MediaExtractor::CAN_SEEK;
}

void
MP4Demuxer::SeekAudio(Microseconds aTime)
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mAudioIterator) {
    mPrivate->mAudioIterator->Seek(aTime);
  } else {
    mPrivate->mAudioOptions.setSeekTo(
      aTime, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
  }
}

void
MP4Demuxer::SeekVideo(Microseconds aTime)
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mVideoIterator) {
    mPrivate->mVideoIterator->Seek(aTime);
  } else {
    mPrivate->mVideoOptions.setSeekTo(
      aTime, MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC);
  }
}

MP4Sample*
MP4Demuxer::DemuxAudioSample()
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mAudioIterator) {
    nsAutoPtr<MP4Sample> sample(mPrivate->mAudioIterator->GetNext());
    if (sample) {
      if (sample->crypto.valid) {
        sample->crypto.mode = mAudioConfig.crypto.mode;
        sample->crypto.iv_size = mAudioConfig.crypto.iv_size;
        sample->crypto.key.AppendElements(mAudioConfig.crypto.key);
      }
    }
    return sample.forget();
  }

  nsAutoPtr<MP4Sample> sample(new MP4Sample());
  status_t status =
    mPrivate->mAudio->read(&sample->mMediaBuffer, &mPrivate->mAudioOptions);
  mPrivate->mAudioOptions.clearSeekTo();

  if (status < 0) {
    return nullptr;
  }

  sample->Update(mAudioConfig.media_time, mTimestampOffset);

  return sample.forget();
}

MP4Sample*
MP4Demuxer::DemuxVideoSample()
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mVideoIterator) {
    nsAutoPtr<MP4Sample> sample(mPrivate->mVideoIterator->GetNext());
    if (sample) {
      sample->extra_data = mVideoConfig.extra_data;
      if (sample->crypto.valid) {
        sample->crypto.mode = mVideoConfig.crypto.mode;
        sample->crypto.key.AppendElements(mVideoConfig.crypto.key);
      }
    }
    return sample.forget();
  }

  nsAutoPtr<MP4Sample> sample(new MP4Sample());
  status_t status =
    mPrivate->mVideo->read(&sample->mMediaBuffer, &mPrivate->mVideoOptions);
  mPrivate->mVideoOptions.clearSeekTo();

  if (status < 0) {
    return nullptr;
  }

  sample->Update(mVideoConfig.media_time, mTimestampOffset);
  sample->extra_data = mVideoConfig.extra_data;

  return sample.forget();
}

void
MP4Demuxer::UpdateIndex(const nsTArray<mozilla::MediaByteRange>& aByteRanges)
{
  mMonitor->AssertCurrentThreadOwns();
  for (int i = 0; i < mPrivate->mIndexes.Length(); i++) {
    mPrivate->mIndexes[i]->UpdateMoofIndex(aByteRanges);
  }
}

void
MP4Demuxer::ConvertByteRangesToTime(
  const nsTArray<mozilla::MediaByteRange>& aByteRanges,
  nsTArray<Interval<Microseconds>>* aIntervals)
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mIndexes.IsEmpty()) {
    return;
  }

  Microseconds lastComposition = 0;
  nsTArray<Microseconds> endCompositions;
  for (int i = 0; i < mPrivate->mIndexes.Length(); i++) {
    Microseconds endComposition =
      mPrivate->mIndexes[i]->GetEndCompositionIfBuffered(aByteRanges);
    endCompositions.AppendElement(endComposition);
    lastComposition = std::max(lastComposition, endComposition);
  }

  if (aByteRanges != mCachedByteRanges) {
    mCachedByteRanges = aByteRanges;
    mCachedTimeRanges.Clear();
    for (int i = 0; i < mPrivate->mIndexes.Length(); i++) {
      nsTArray<Interval<Microseconds>> ranges;
      mPrivate->mIndexes[i]->ConvertByteRangesToTimeRanges(aByteRanges, &ranges);
      if (lastComposition && endCompositions[i]) {
        Interval<Microseconds>::SemiNormalAppend(
          ranges, Interval<Microseconds>(endCompositions[i], lastComposition));
      }

      if (i) {
        nsTArray<Interval<Microseconds>> intersection;
        Interval<Microseconds>::Intersection(mCachedTimeRanges, ranges, &intersection);
        mCachedTimeRanges = intersection;
      } else {
        mCachedTimeRanges = ranges;
      }
    }
  }
  aIntervals->AppendElements(mCachedTimeRanges);
}

int64_t
MP4Demuxer::GetEvictionOffset(Microseconds aTime)
{
  mMonitor->AssertCurrentThreadOwns();
  if (mPrivate->mIndexes.IsEmpty()) {
    return 0;
  }

  uint64_t offset = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < mPrivate->mIndexes.Length(); i++) {
    offset = std::min(offset, mPrivate->mIndexes[i]->GetEvictionOffset(aTime));
  }
  return offset == std::numeric_limits<uint64_t>::max() ? -1 : offset;
}

} // namespace mp4_demuxer
