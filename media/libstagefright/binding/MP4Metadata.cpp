/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/MP4Metadata.h"

#include <limits>
#include <stdint.h>
#include <vector>

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
  RefPtr<Stream> mSource;
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

  if (metaData.get()) {
    UpdateCrypto(metaData.get());
  }
}

MP4Metadata::~MP4Metadata()
{
}

#ifdef MOZ_RUST_MP4PARSE
// Helper to test the rust parser on a data source.
static bool try_rust(const UniquePtr<mp4parse_state, FreeMP4ParseState>& aRustState, RefPtr<Stream> aSource, int32_t* aCount)
{
  static LazyLogModule sLog("MP4Metadata");
  int64_t length;
  if (!aSource->Length(&length) || length <= 0) {
    MOZ_LOG(sLog, LogLevel::Warning, ("Couldn't get source length"));
    return false;
  }
  MOZ_LOG(sLog, LogLevel::Debug,
         ("Source length %d bytes\n", (long long int)length));
  size_t bytes_read = 0;
  auto buffer = std::vector<uint8_t>(length);
  bool rv = aSource->ReadAt(0, buffer.data(), length, &bytes_read);
  if (!rv || bytes_read != size_t(length)) {
    MOZ_LOG(sLog, LogLevel::Warning, ("Error copying mp4 data"));
    return false;
  }
  *aCount = mp4parse_read(aRustState.get(), buffer.data(), bytes_read);
  MOZ_LOG(sLog, LogLevel::Info, ("rust parser found %d tracks", int(*aCount)));
  return true;
}
#endif

uint32_t
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
#ifdef MOZ_RUST_MP4PARSE
  // Try in rust first.
  mRustState.reset(mp4parse_new());
  int32_t rust_tracks = 0;
  bool rust_mp4parse_success = try_rust(mRustState, mSource, &rust_tracks);
  Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_SUCCESS,
                        rust_mp4parse_success);
#endif
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
#ifdef MOZ_RUST_MP4PARSE
  uint32_t rust_total = 0;
  const char* rust_track_type = nullptr;
  if (rust_mp4parse_success && rust_tracks > 0) {
    for (int32_t i = 0; i < rust_tracks; ++i) {
      mp4parse_track_info track_info;
      int32_t r = mp4parse_get_track_info(mRustState.get(), i, &track_info);
      switch (aType) {
      case mozilla::TrackInfo::kAudioTrack:
        rust_track_type = "audio";
        if (r == 0 && track_info.track_type == MP4PARSE_TRACK_TYPE_AAC) {
          rust_total += 1;
        }
        break;
      case mozilla::TrackInfo::kVideoTrack:
        rust_track_type = "video";
        if (r == 0 && track_info.track_type == MP4PARSE_TRACK_TYPE_H264) {
          rust_total += 1;
        }
        break;
      default:
        break;
      }
    }
  }
  static LazyLogModule sLog("MP4Metadata");
  MOZ_LOG(sLog, LogLevel::Info, ("%s tracks found: stagefright=%u rust=%u",
                                 rust_track_type, total, rust_total));
#endif
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
  if (!track.get()) {
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
  if (!track.get()) {
    return false;
  }
  sp<MetaData> metadata =
    mPrivate->mMetadataExtractor->getTrackMetaData(trackNumber);
  int64_t mediaTime;
  if (!metadata->findInt64(kKeyMediaTime, &mediaTime)) {
    mediaTime = 0;
  }
  bool rv = ConvertIndex(aDest, track->exportIndex(), mediaTime);

  return rv;
}

int32_t
MP4Metadata::GetTrackNumber(mozilla::TrackID aTrackID)
{
  size_t numTracks = mPrivate->mMetadataExtractor->countTracks();
  for (size_t i = 0; i < numTracks; i++) {
    sp<MetaData> metaData = mPrivate->mMetadataExtractor->getTrackMetaData(i);
    if (!metaData.get()) {
      continue;
    }
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
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  return parser->HasMetadata();
}

/*static*/ already_AddRefed<mozilla::MediaByteBuffer>
MP4Metadata::Metadata(Stream* aSource)
{
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  return parser->Metadata();
}

} // namespace mp4_demuxer
