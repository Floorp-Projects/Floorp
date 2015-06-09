/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/mp4_demuxer.h"

#include <stdint.h>
#include <algorithm>
#include <limits>

namespace mp4_demuxer
{

MP4Demuxer::MP4Demuxer(Stream* source, Monitor* aMonitor)
  : mSource(source)
  , mMonitor(aMonitor)
  , mNextKeyframeTime(-1)
{
}

MP4Demuxer::~MP4Demuxer()
{
}

bool
MP4Demuxer::Init()
{
  mMonitor->AssertCurrentThreadOwns();

  // Check that we have an entire moov before attempting any new reads to make
  // the retry system work.
  if (!MP4Metadata::HasCompleteMetadata(mSource)) {
    return false;
  }

  mMetadata = mozilla::MakeUnique<MP4Metadata>(mSource);

  if (!mMetadata->GetNumberTracks(mozilla::TrackInfo::kAudioTrack) &&
      !mMetadata->GetNumberTracks(mozilla::TrackInfo::kVideoTrack)) {
    return false;
  }

  auto audioInfo = mMetadata->GetTrackInfo(mozilla::TrackInfo::kAudioTrack, 0);
  if (audioInfo) {
    mAudioConfig = mozilla::Move(audioInfo);
    FallibleTArray<Index::Indice> indices;
    if (!mMetadata->ReadTrackIndex(indices, mAudioConfig->mTrackId)) {
      return false;
    }
    nsRefPtr<Index> index =
      new Index(indices,
                mSource,
                mAudioConfig->mTrackId,
                /* aIsAudio = */ true,
                mMonitor);
    mIndexes.AppendElement(index);
    mAudioIterator = mozilla::MakeUnique<SampleIterator>(index);
  }

  auto videoInfo = mMetadata->GetTrackInfo(mozilla::TrackInfo::kVideoTrack, 0);
  if (videoInfo) {
    mVideoConfig = mozilla::Move(videoInfo);
    FallibleTArray<Index::Indice> indices;
    if (!mMetadata->ReadTrackIndex(indices, mVideoConfig->mTrackId)) {
      return false;
    }

    nsRefPtr<Index> index =
      new Index(indices,
                mSource,
                mVideoConfig->mTrackId,
                /* aIsAudio = */ false,
                mMonitor);
    mIndexes.AppendElement(index);
    mVideoIterator = mozilla::MakeUnique<SampleIterator>(index);
  }

  return mAudioIterator || mVideoIterator;
}

bool
MP4Demuxer::HasValidAudio()
{
  mMonitor->AssertCurrentThreadOwns();
  return mAudioIterator && mAudioConfig && mAudioConfig->IsValid();
}

bool
MP4Demuxer::HasValidVideo()
{
  mMonitor->AssertCurrentThreadOwns();
  return mVideoIterator && mVideoConfig && mVideoConfig->IsValid();
}

Microseconds
MP4Demuxer::Duration()
{
  mMonitor->AssertCurrentThreadOwns();
  int64_t videoDuration = mVideoConfig ? mVideoConfig->mDuration : 0;
  int64_t audioDuration = mAudioConfig ? mAudioConfig->mDuration : 0;
  return std::max(videoDuration, audioDuration);
}

bool
MP4Demuxer::CanSeek()
{
  mMonitor->AssertCurrentThreadOwns();
  return mMetadata->CanSeek();
}

void
MP4Demuxer::SeekAudio(Microseconds aTime)
{
  mMonitor->AssertCurrentThreadOwns();
  MOZ_ASSERT(mAudioIterator);
  mAudioIterator->Seek(aTime);
}

void
MP4Demuxer::SeekVideo(Microseconds aTime)
{
  mMonitor->AssertCurrentThreadOwns();
  MOZ_ASSERT(mVideoIterator);
  mVideoIterator->Seek(aTime);
}

already_AddRefed<mozilla::MediaRawData>
MP4Demuxer::DemuxAudioSample()
{
  mMonitor->AssertCurrentThreadOwns();
  MOZ_ASSERT(mAudioIterator);
  nsRefPtr<mozilla::MediaRawData> sample(mAudioIterator->GetNext());
  if (sample) {
    if (sample->mCrypto.mValid) {
      nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
      writer->mCrypto.mMode = mAudioConfig->mCrypto.mMode;
      writer->mCrypto.mIVSize = mAudioConfig->mCrypto.mIVSize;
      writer->mCrypto.mKeyId.AppendElements(mAudioConfig->mCrypto.mKeyId);
    }
  }
  return sample.forget();
}

already_AddRefed<MediaRawData>
MP4Demuxer::DemuxVideoSample()
{
  mMonitor->AssertCurrentThreadOwns();
  MOZ_ASSERT(mVideoIterator);
  nsRefPtr<mozilla::MediaRawData> sample(mVideoIterator->GetNext());
  if (sample) {
    sample->mExtraData = mVideoConfig->GetAsVideoInfo()->mExtraData;
    if (sample->mCrypto.mValid) {
      nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
      writer->mCrypto.mMode = mVideoConfig->mCrypto.mMode;
      writer->mCrypto.mKeyId.AppendElements(mVideoConfig->mCrypto.mKeyId);
    }
    if (sample->mTime >= mNextKeyframeTime) {
      mNextKeyframeTime = mVideoIterator->GetNextKeyframeTime();
    }
  }
  return sample.forget();
}

void
MP4Demuxer::UpdateIndex(const nsTArray<mozilla::MediaByteRange>& aByteRanges)
{
  mMonitor->AssertCurrentThreadOwns();
  for (int i = 0; i < mIndexes.Length(); i++) {
    mIndexes[i]->UpdateMoofIndex(aByteRanges);
  }
}

void
MP4Demuxer::ConvertByteRangesToTime(
  const nsTArray<mozilla::MediaByteRange>& aByteRanges,
  nsTArray<Interval<Microseconds>>* aIntervals)
{
  mMonitor->AssertCurrentThreadOwns();
  if (mIndexes.IsEmpty()) {
    return;
  }

  Microseconds lastComposition = 0;
  nsTArray<Microseconds> endCompositions;
  for (int i = 0; i < mIndexes.Length(); i++) {
    Microseconds endComposition =
      mIndexes[i]->GetEndCompositionIfBuffered(aByteRanges);
    endCompositions.AppendElement(endComposition);
    lastComposition = std::max(lastComposition, endComposition);
  }

  if (aByteRanges != mCachedByteRanges) {
    mCachedByteRanges = aByteRanges;
    mCachedTimeRanges.Clear();
    for (int i = 0; i < mIndexes.Length(); i++) {
      nsTArray<Interval<Microseconds>> ranges;
      mIndexes[i]->ConvertByteRangesToTimeRanges(aByteRanges, &ranges);
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
  if (mIndexes.IsEmpty()) {
    return 0;
  }

  uint64_t offset = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < mIndexes.Length(); i++) {
    offset = std::min(offset, mIndexes[i]->GetEvictionOffset(aTime));
  }
  return offset == std::numeric_limits<uint64_t>::max() ? 0 : offset;
}

Microseconds
MP4Demuxer::GetNextKeyframeTime()
{
  mMonitor->AssertCurrentThreadOwns();
  return mNextKeyframeTime;
}

const CryptoFile&
MP4Demuxer::Crypto() const
{
  return mMetadata->Crypto();
}

} // namespace mp4_demuxer
