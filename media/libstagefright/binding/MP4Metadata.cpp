/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mozilla/Monitor.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/MP4Metadata.h"

#include <limits>
#include <stdint.h>

using namespace stagefright;

namespace mp4_demuxer
{

struct StageFrightPrivate
{
  StageFrightPrivate()
    : mCanSeek(false) {}
  sp<MediaExtractor> mMetadataExtractor;

  bool mCanSeek;
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

static inline bool
ConvertIndex(FallibleTArray<Index::Indice>& aDest,
             const nsTArray<stagefright::MediaSource::Indice>& aIndex,
             int64_t aMediaTime)
{
  if (!aDest.SetCapacity(aIndex.Length(), mozilla::fallible)) {
    return false;
  }
  for (size_t i = 0; i < aIndex.Length(); i++) {
    Index::Indice indice;
    const stagefright::MediaSource::Indice& s_indice = aIndex[i];
    indice.start_offset = s_indice.start_offset;
    indice.end_offset = s_indice.end_offset;
    indice.start_composition = s_indice.start_composition - aMediaTime;
    indice.end_composition = s_indice.end_composition - aMediaTime;
    indice.start_decode = s_indice.start_decode;
    indice.sync = s_indice.sync;
    // FIXME: Make this infallible after bug 968520 is done.
    MOZ_ALWAYS_TRUE(aDest.AppendElement(indice, mozilla::fallible));
  }
  return true;
}

MP4Metadata::MP4Metadata(Stream* aSource)
  : mPrivate(new StageFrightPrivate)
  , mSource(aSource)
{
  mPrivate->mMetadataExtractor =
    new MPEG4Extractor(new DataSourceAdapter(mSource));
  mPrivate->mCanSeek =
    mPrivate->mMetadataExtractor->flags() & MediaExtractor::CAN_SEEK;
  sp<MetaData> metaData = mPrivate->mMetadataExtractor->getMetaData();

  UpdateCrypto(metaData.get());
}

MP4Metadata::~MP4Metadata()
{
}

uint32_t
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  size_t tracks = mPrivate->mMetadataExtractor->countTracks();
  uint32_t total = 0;
  for (size_t i = 0; i < tracks; i++) {
    sp<MetaData> metaData = mPrivate->mMetadataExtractor->getTrackMetaData(i);

    const char* mimeType;
    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }
    switch (aType) {
      case mozilla::TrackInfo::kAudioTrack:
        if (!strncmp(mimeType, "audio/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          total++;
        }
        break;
      case mozilla::TrackInfo::kVideoTrack:
        if (!strncmp(mimeType, "video/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          total++;
        }
        break;
      default:
        break;
    }
  }
  return total;
}

mozilla::UniquePtr<mozilla::TrackInfo>
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                          size_t aTrackNumber) const
{
  size_t tracks = mPrivate->mMetadataExtractor->countTracks();
  if (!tracks) {
    return nullptr;
  }
  int32_t index = -1;
  const char* mimeType;
  sp<MetaData> metaData;

  size_t i = 0;
  while (i < tracks) {
    metaData = mPrivate->mMetadataExtractor->getTrackMetaData(i);

    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }
    switch (aType) {
      case mozilla::TrackInfo::kAudioTrack:
        if (!strncmp(mimeType, "audio/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          index++;
        }
        break;
      case mozilla::TrackInfo::kVideoTrack:
        if (!strncmp(mimeType, "video/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          index++;
        }
        break;
      default:
        break;
    }
    if (index == aTrackNumber) {
      break;
    }
    i++;
  }
  if (index < 0) {
    return nullptr;
  }

  UniquePtr<mozilla::TrackInfo> e = CheckTrack(mimeType, metaData.get(), index);

  if (e) {
    metaData = mPrivate->mMetadataExtractor->getMetaData();
    int64_t movieDuration;
    if (!e->mDuration &&
        metaData->findInt64(kKeyMovieDuration, &movieDuration)) {
      // No duration in track, use movie extend header box one.
      e->mDuration = movieDuration;
    }
  }

  return e;
}

mozilla::UniquePtr<mozilla::TrackInfo>
MP4Metadata::CheckTrack(const char* aMimeType,
                        stagefright::MetaData* aMetaData,
                        int32_t aIndex) const
{
  sp<MediaSource> track = mPrivate->mMetadataExtractor->getTrack(aIndex);
  if (!track.get() || track->start() != OK) {
    return nullptr;
  }

  UniquePtr<mozilla::TrackInfo> e;

  if (!strncmp(aMimeType, "audio/", 6)) {
    auto info = mozilla::MakeUnique<MP4AudioInfo>();
    info->Update(aMetaData, aMimeType);
    e = Move(info);
  } else if (!strncmp(aMimeType, "video/", 6)) {
    auto info = mozilla::MakeUnique<MP4VideoInfo>();
    info->Update(aMetaData, aMimeType);
    e = Move(info);
  }

  track->stop();

  if (e && e->IsValid()) {
    return e;
  }
  return nullptr;
}

bool
MP4Metadata::CanSeek() const
{
  return mPrivate->mCanSeek;
}

void
MP4Metadata::UpdateCrypto(const MetaData* aMetaData)
{
  const void* data;
  size_t size;
  uint32_t type;

  // There's no point in checking that the type matches anything because it
  // isn't set consistently in the MPEG4Extractor.
  if (!aMetaData->findData(kKeyPssh, &type, &data, &size)) {
    return;
  }
  mCrypto.Update(reinterpret_cast<const uint8_t*>(data), size);
}

bool
MP4Metadata::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID)
{
  size_t numTracks = mPrivate->mMetadataExtractor->countTracks();
  int32_t trackNumber = GetTrackNumber(aTrackID);
  if (trackNumber < 0) {
    return false;
  }
  sp<MediaSource> track = mPrivate->mMetadataExtractor->getTrack(trackNumber);
  if (!track.get() || track->start() != OK) {
    return false;
  }
  sp<MetaData> metadata =
    mPrivate->mMetadataExtractor->getTrackMetaData(trackNumber);
  int64_t mediaTime;
  if (!metadata->findInt64(kKeyMediaTime, &mediaTime)) {
    mediaTime = 0;
  }
  bool rv = ConvertIndex(aDest, track->exportIndex(), mediaTime);

  track->stop();

  return rv;
}

int32_t
MP4Metadata::GetTrackNumber(mozilla::TrackID aTrackID)
{
  size_t numTracks = mPrivate->mMetadataExtractor->countTracks();
  for (size_t i = 0; i < numTracks; i++) {
    sp<MetaData> metaData = mPrivate->mMetadataExtractor->getTrackMetaData(i);
    int32_t value;
    if (metaData->findInt32(kKeyTrackID, &value) && value == aTrackID) {
      return i;
    }
  }
  return -1;
}

/*static*/ bool
MP4Metadata::HasCompleteMetadata(Stream* aSource)
{
  // The MoofParser requires a monitor, but we don't need one here.
  mozilla::Monitor monitor("MP4Metadata::HasCompleteMetadata");
  mozilla::MonitorAutoLock mon(monitor);
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false, &monitor);
  return parser->HasMetadata();
}

/*static*/ already_AddRefed<mozilla::MediaByteBuffer>
MP4Metadata::Metadata(Stream* aSource)
{
  // The MoofParser requires a monitor, but we don't need one here.
  mozilla::Monitor monitor("MP4Metadata::HasCompleteMetadata");
  mozilla::MonitorAutoLock mon(monitor);
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false, &monitor);
  return parser->Metadata();
}

} // namespace mp4_demuxer
