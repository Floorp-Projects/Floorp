/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "VideoUtils.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/Stream.h"
#include "MediaPrefs.h"

#include <limits>
#include <stdint.h>
#include <vector>

#ifdef MOZ_RUST_MP4PARSE
// OpusDecoder header is really needed only by MP4 in rust
#include "OpusDecoder.h"
#include "mp4parse.h"

struct FreeMP4Parser { void operator()(mp4parse_parser* aPtr) { mp4parse_free(aPtr); } };
#endif

using namespace stagefright;

namespace mp4_demuxer
{

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

class MP4MetadataStagefright
{
public:
  explicit MP4MetadataStagefright(Stream* aSource);
  ~MP4MetadataStagefright();

  static bool HasCompleteMetadata(Stream* aSource);
  static already_AddRefed<mozilla::MediaByteBuffer> Metadata(Stream* aSource);
  uint32_t GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  mozilla::UniquePtr<mozilla::TrackInfo> GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                                      size_t aTrackNumber) const;
  bool CanSeek() const;

  const CryptoFile& Crypto() const;

  bool ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

private:
  int32_t GetTrackNumber(mozilla::TrackID aTrackID);
  void UpdateCrypto(const stagefright::MetaData* aMetaData);
  mozilla::UniquePtr<mozilla::TrackInfo> CheckTrack(const char* aMimeType,
                                                    stagefright::MetaData* aMetaData,
                                                    int32_t aIndex) const;
  CryptoFile mCrypto;
  RefPtr<Stream> mSource;
  sp<MediaExtractor> mMetadataExtractor;
  bool mCanSeek;
};

#ifdef MOZ_RUST_MP4PARSE

// Wrap an mp4_demuxer::Stream to remember the read offset.

class RustStreamAdaptor {
public:
  explicit RustStreamAdaptor(Stream* aSource)
    : mSource(aSource)
    , mOffset(0)
  {
  }

  ~RustStreamAdaptor() {}

  bool Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read);

private:
  Stream* mSource;
  CheckedInt<size_t> mOffset;
};

class MP4MetadataRust
{
public:
  explicit MP4MetadataRust(Stream* aSource);
  ~MP4MetadataRust();

  static bool HasCompleteMetadata(Stream* aSource);
  static already_AddRefed<mozilla::MediaByteBuffer> Metadata(Stream* aSource);
  uint32_t GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  mozilla::UniquePtr<mozilla::TrackInfo> GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                                      size_t aTrackNumber) const;
  bool CanSeek() const;

  const CryptoFile& Crypto() const;

  bool ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

private:
  Maybe<uint32_t> TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;

  CryptoFile mCrypto;
  RefPtr<Stream> mSource;
  RustStreamAdaptor mRustSource;
  mozilla::UniquePtr<mp4parse_parser, FreeMP4Parser> mRustParser;
};
#endif

MP4Metadata::MP4Metadata(Stream* aSource)
 : mStagefright(MakeUnique<MP4MetadataStagefright>(aSource))
#ifdef MOZ_RUST_MP4PARSE
 , mRust(MakeUnique<MP4MetadataRust>(aSource))
 , mPreferRust(false)
 , mReportedAudioTrackTelemetry(false)
 , mReportedVideoTrackTelemetry(false)
#ifndef RELEASE_OR_BETA
 , mRustTestMode(MediaPrefs::RustTestMode())
#endif
#endif
{
}

MP4Metadata::~MP4Metadata()
{
}

/*static*/ bool
MP4Metadata::HasCompleteMetadata(Stream* aSource)
{
  return MP4MetadataStagefright::HasCompleteMetadata(aSource);
}

/*static*/ already_AddRefed<mozilla::MediaByteBuffer>
MP4Metadata::Metadata(Stream* aSource)
{
  return MP4MetadataStagefright::Metadata(aSource);
}

static const char *
TrackTypeToString(mozilla::TrackInfo::TrackType aType)
{
  switch (aType) {
  case mozilla::TrackInfo::kAudioTrack:
    return "audio";
  case mozilla::TrackInfo::kVideoTrack:
    return "video";
  default:
    return "unknown";
  }
}

uint32_t
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  static LazyLogModule sLog("MP4Metadata");

  uint32_t numTracks = mStagefright->GetNumberTracks(aType);

#ifdef MOZ_RUST_MP4PARSE
  if (!mRust) {
    return numTracks;
  }

  uint32_t numTracksRust = mRust->GetNumberTracks(aType);
  MOZ_LOG(sLog, LogLevel::Info, ("%s tracks found: stagefright=%u rust=%u",
                                 TrackTypeToString(aType), numTracks, numTracksRust));

  bool numTracksMatch = numTracks == numTracksRust;

  if (aType == mozilla::TrackInfo::kAudioTrack && !mReportedAudioTrackTelemetry) {
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_TRACK_MATCH_AUDIO,
                          numTracksMatch);
    mReportedAudioTrackTelemetry = true;
  } else if (aType == mozilla::TrackInfo::kVideoTrack && !mReportedVideoTrackTelemetry) {
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_TRACK_MATCH_VIDEO,
                            numTracksMatch);
    mReportedVideoTrackTelemetry = true;
  }

  if (mPreferRust || ShouldPreferRust()) {
    MOZ_LOG(sLog, LogLevel::Info, ("Preferring rust demuxer"));
    mPreferRust = true;
    return numTracksRust;
  }
#endif // MOZ_RUST_MP4PARSE

  return numTracks;
}

#ifdef MOZ_RUST_MP4PARSE
bool MP4Metadata::ShouldPreferRust() const {
  if (!mRust) {
    return false;
  }
  // See if there's an Opus track.
  uint32_t numTracks = mRust->GetNumberTracks(TrackInfo::kAudioTrack);
  for (auto i = 0; i < numTracks; i++) {
    auto info = mRust->GetTrackInfo(TrackInfo::kAudioTrack, i);
    if (!info) {
      return false;
    }
    if (info->mMimeType.EqualsASCII("audio/opus") ||
        info->mMimeType.EqualsASCII("audio/flac")) {
      return true;
    }
  }

  numTracks = mRust->GetNumberTracks(TrackInfo::kVideoTrack);
  for (auto i = 0; i < numTracks; i++) {
    auto info = mRust->GetTrackInfo(TrackInfo::kVideoTrack, i);
    if (!info) {
      return false;
    }
    if (info->mMimeType.EqualsASCII("video/vp9")) {
      return true;
    }
  }
  // Otherwise, fall back.
  return false;
}
#endif // MOZ_RUST_MP4PARSE

mozilla::UniquePtr<mozilla::TrackInfo>
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                          size_t aTrackNumber) const
{
  mozilla::UniquePtr<mozilla::TrackInfo> info =
      mStagefright->GetTrackInfo(aType, aTrackNumber);

#ifdef MOZ_RUST_MP4PARSE
  if (!mRust) {
    return info;
  }

  mozilla::UniquePtr<mozilla::TrackInfo> infoRust =
      mRust->GetTrackInfo(aType, aTrackNumber);

#ifndef RELEASE_OR_BETA
  if (mRustTestMode && info) {
    MOZ_DIAGNOSTIC_ASSERT(infoRust);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mId == info->mId);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mKind == info->mKind);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mLabel == info->mLabel);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mLanguage == info->mLanguage);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mEnabled == info->mEnabled);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mTrackId == info->mTrackId);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mMimeType == info->mMimeType);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mDuration == info->mDuration);
    MOZ_DIAGNOSTIC_ASSERT(infoRust->mMediaTime == info->mMediaTime);
    switch (aType) {
    case mozilla::TrackInfo::kAudioTrack: {
      AudioInfo *audioRust = infoRust->GetAsAudioInfo(), *audio = info->GetAsAudioInfo();
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mRate == audio->mRate);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mChannels == audio->mChannels);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mBitDepth == audio->mBitDepth);
      // TODO: These fields aren't implemented in the Rust demuxer yet.
      //MOZ_DIAGNOSTIC_ASSERT(audioRust->mProfile != audio->mProfile);
      //MOZ_DIAGNOSTIC_ASSERT(audioRust->mExtendedProfile != audio->mExtendedProfile);
      break;
    }
    case mozilla::TrackInfo::kVideoTrack: {
      VideoInfo *videoRust = infoRust->GetAsVideoInfo(), *video = info->GetAsVideoInfo();
      MOZ_DIAGNOSTIC_ASSERT(videoRust->mDisplay == video->mDisplay);
      MOZ_DIAGNOSTIC_ASSERT(videoRust->mImage == video->mImage);
      break;
    }
    default:
      break;
    }
  }
#endif

  if (!mPreferRust) {
    return info;
  }
  MOZ_ASSERT(infoRust);
  return infoRust;
#endif

  return info;
}

bool
MP4Metadata::CanSeek() const
{
  return mStagefright->CanSeek();
}

const CryptoFile&
MP4Metadata::Crypto() const
{
  return mStagefright->Crypto();
}

bool
MP4Metadata::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID)
{
#ifdef MOZ_RUST_MP4PARSE
  if (mRust && mPreferRust && mRust->ReadTrackIndex(aDest, aTrackID)) {
    return true;
  }
  aDest.Clear();
#endif
  return mStagefright->ReadTrackIndex(aDest, aTrackID);
}

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

MP4MetadataStagefright::MP4MetadataStagefright(Stream* aSource)
  : mSource(aSource)
  , mMetadataExtractor(new MPEG4Extractor(new DataSourceAdapter(mSource)))
  , mCanSeek(mMetadataExtractor->flags() & MediaExtractor::CAN_SEEK)
{
  sp<MetaData> metaData = mMetadataExtractor->getMetaData();

  if (metaData.get()) {
    UpdateCrypto(metaData.get());
  }
}

MP4MetadataStagefright::~MP4MetadataStagefright()
{
}

uint32_t
MP4MetadataStagefright::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  size_t tracks = mMetadataExtractor->countTracks();
  uint32_t total = 0;
  for (size_t i = 0; i < tracks; i++) {
    sp<MetaData> metaData = mMetadataExtractor->getTrackMetaData(i);

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
MP4MetadataStagefright::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                     size_t aTrackNumber) const
{
  size_t tracks = mMetadataExtractor->countTracks();
  if (!tracks) {
    return nullptr;
  }
  int32_t index = -1;
  const char* mimeType;
  sp<MetaData> metaData;

  size_t i = 0;
  while (i < tracks) {
    metaData = mMetadataExtractor->getTrackMetaData(i);

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
    metaData = mMetadataExtractor->getMetaData();
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
MP4MetadataStagefright::CheckTrack(const char* aMimeType,
                                   stagefright::MetaData* aMetaData,
                                   int32_t aIndex) const
{
  sp<MediaSource> track = mMetadataExtractor->getTrack(aIndex);
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
MP4MetadataStagefright::CanSeek() const
{
  return mCanSeek;
}

const CryptoFile&
MP4MetadataStagefright::Crypto() const
{
  return mCrypto;
}

void
MP4MetadataStagefright::UpdateCrypto(const MetaData* aMetaData)
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
MP4MetadataStagefright::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID)
{
  size_t numTracks = mMetadataExtractor->countTracks();
  int32_t trackNumber = GetTrackNumber(aTrackID);
  if (trackNumber < 0) {
    return false;
  }
  sp<MediaSource> track = mMetadataExtractor->getTrack(trackNumber);
  if (!track.get()) {
    return false;
  }
  sp<MetaData> metadata = mMetadataExtractor->getTrackMetaData(trackNumber);
  int64_t mediaTime;
  if (!metadata->findInt64(kKeyMediaTime, &mediaTime)) {
    mediaTime = 0;
  }
  bool rv = ConvertIndex(aDest, track->exportIndex(), mediaTime);

  return rv;
}

int32_t
MP4MetadataStagefright::GetTrackNumber(mozilla::TrackID aTrackID)
{
  size_t numTracks = mMetadataExtractor->countTracks();
  for (size_t i = 0; i < numTracks; i++) {
    sp<MetaData> metaData = mMetadataExtractor->getTrackMetaData(i);
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
MP4MetadataStagefright::HasCompleteMetadata(Stream* aSource)
{
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  return parser->HasMetadata();
}

/*static*/ already_AddRefed<mozilla::MediaByteBuffer>
MP4MetadataStagefright::Metadata(Stream* aSource)
{
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  return parser->Metadata();
}

#ifdef MOZ_RUST_MP4PARSE
bool
RustStreamAdaptor::Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read)
{
  if (!mOffset.isValid()) {
    static LazyLogModule sLog("MP4Metadata");
    MOZ_LOG(sLog, LogLevel::Error, ("Overflow in source stream offset"));
    return false;
  }
  bool rv = mSource->ReadAt(mOffset.value(), buffer, size, bytes_read);
  if (rv) {
    mOffset += *bytes_read;
  }
  return rv;
}

// Wrapper to allow rust to call our read adaptor.
static intptr_t
read_source(uint8_t* buffer, uintptr_t size, void* userdata)
{
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(userdata);

  auto source = reinterpret_cast<RustStreamAdaptor*>(userdata);
  size_t bytes_read = 0;
  bool rv = source->Read(buffer, size, &bytes_read);
  if (!rv) {
    static LazyLogModule sLog("MP4Metadata");
    MOZ_LOG(sLog, LogLevel::Warning, ("Error reading source data"));
    return -1;
  }
  return bytes_read;
}

MP4MetadataRust::MP4MetadataRust(Stream* aSource)
  : mSource(aSource)
  , mRustSource(aSource)
{
  mp4parse_io io = { read_source, &mRustSource };
  mRustParser.reset(mp4parse_new(&io));
  MOZ_ASSERT(mRustParser);

  static LazyLogModule sLog("MP4Metadata");
  mp4parse_error rv = mp4parse_read(mRustParser.get());
  MOZ_LOG(sLog, LogLevel::Debug, ("rust parser returned %d\n", rv));
  Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_SUCCESS,
                        rv == MP4PARSE_OK);
  if (rv != MP4PARSE_OK) {
    MOZ_ASSERT(rv > 0);
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_ERROR_CODE, rv);
  }
}

MP4MetadataRust::~MP4MetadataRust()
{
}

bool
TrackTypeEqual(TrackInfo::TrackType aLHS, mp4parse_track_type aRHS)
{
  switch (aLHS) {
  case TrackInfo::kAudioTrack:
    return aRHS == MP4PARSE_TRACK_TYPE_AUDIO;
  case TrackInfo::kVideoTrack:
    return aRHS == MP4PARSE_TRACK_TYPE_VIDEO;
  default:
    return false;
  }
}

uint32_t
MP4MetadataRust::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  static LazyLogModule sLog("MP4Metadata");

  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mRustParser.get(), &tracks);
  if (rv != MP4PARSE_OK) {
    MOZ_LOG(sLog, LogLevel::Warning,
        ("rust parser error %d counting tracks", rv));
    return 0;
  }
  MOZ_LOG(sLog, LogLevel::Info, ("rust parser found %u tracks", tracks));

  uint32_t total = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    mp4parse_track_info track_info;
    rv = mp4parse_get_track_info(mRustParser.get(), i, &track_info);
    if (rv != MP4PARSE_OK) {
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
        total += 1;
    }
  }

  return total;
}

Maybe<uint32_t>
MP4MetadataRust::TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const
{
  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mRustParser.get(), &tracks);
  if (rv != MP4PARSE_OK) {
    return Nothing();
  }

  /* The MP4Metadata API uses a per-TrackType index of tracks, but mp4parse
     (and libstagefright) use a global track index.  Convert the index by
     counting the tracks of the requested type and returning the global
     track index when a match is found. */
  uint32_t perType = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    mp4parse_track_info track_info;
    rv = mp4parse_get_track_info(mRustParser.get(), i, &track_info);
    if (rv != MP4PARSE_OK) {
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
      if (perType == aTrackNumber) {
        return Some(i);
      }
      perType += 1;
    }
  }

  return Nothing();
}

mozilla::UniquePtr<mozilla::TrackInfo>
MP4MetadataRust::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                              size_t aTrackNumber) const
{
  static LazyLogModule sLog("MP4Metadata");

  Maybe<uint32_t> trackIndex = TrackTypeToGlobalTrackIndex(aType, aTrackNumber);
  if (trackIndex.isNothing()) {
    return nullptr;
  }

  mp4parse_track_info info;
  auto rv = mp4parse_get_track_info(mRustParser.get(), trackIndex.value(), &info);
  if (rv != MP4PARSE_OK) {
    MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_info returned %d", rv));
    return nullptr;
  }
#ifdef DEBUG
  const char* codec_string = "unrecognized";
  switch (info.codec) {
    case MP4PARSE_CODEC_UNKNOWN: codec_string = "unknown"; break;
    case MP4PARSE_CODEC_AAC: codec_string = "aac"; break;
    case MP4PARSE_CODEC_OPUS: codec_string = "opus"; break;
    case MP4PARSE_CODEC_FLAC: codec_string = "flac"; break;
    case MP4PARSE_CODEC_AVC: codec_string = "h.264"; break;
    case MP4PARSE_CODEC_VP9: codec_string = "vp9"; break;
  }
  MOZ_LOG(sLog, LogLevel::Debug, ("track codec %s (%u)\n",
        codec_string, info.codec));
#endif

  // This specialization interface is crazy.
  UniquePtr<mozilla::TrackInfo> e;
  switch (aType) {
    case TrackInfo::TrackType::kAudioTrack: {
      mp4parse_track_audio_info audio;
      auto rv = mp4parse_get_track_audio_info(mRustParser.get(), trackIndex.value(), &audio);
      if (rv != MP4PARSE_OK) {
        MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_audio_info returned error %d", rv));
        return nullptr;
      }
      auto track = mozilla::MakeUnique<mozilla::AudioInfo>();
      if (info.codec == MP4PARSE_CODEC_OPUS) {
        track->mMimeType = NS_LITERAL_CSTRING("audio/opus");
        // The Opus decoder expects the container's codec delay or
        // pre-skip value, in microseconds, as a 64-bit int at the
        // start of the codec-specific config blob.
        MOZ_ASSERT(audio.codec_specific_config.data);
        MOZ_ASSERT(audio.codec_specific_config.length >= 12);
        uint16_t preskip =
          LittleEndian::readUint16(audio.codec_specific_config.data + 10);
        MOZ_LOG(sLog, LogLevel::Debug,
            ("Copying opus pre-skip value of %d as CodecDelay.",(int)preskip));
        OpusDataDecoder::AppendCodecDelay(track->mCodecSpecificConfig,
            mozilla::FramesToUsecs(preskip, 48000).value());
      } else if (info.codec == MP4PARSE_CODEC_AAC) {
        track->mMimeType = MEDIA_MIMETYPE_AUDIO_AAC;
      } else if (info.codec == MP4PARSE_CODEC_FLAC) {
        track->mMimeType = MEDIA_MIMETYPE_AUDIO_FLAC;
      }
      track->mCodecSpecificConfig->AppendElements(
          audio.codec_specific_config.data,
          audio.codec_specific_config.length);
      track->mRate = audio.sample_rate;
      track->mChannels = audio.channels;
      track->mBitDepth = audio.bit_depth;
      track->mDuration = info.duration;
      track->mMediaTime = info.media_time;
      track->mTrackId = info.track_id;
      e = Move(track);
    }
    break;
    case TrackInfo::TrackType::kVideoTrack: {
      mp4parse_track_video_info video;
      auto rv = mp4parse_get_track_video_info(mRustParser.get(), trackIndex.value(), &video);
      if (rv != MP4PARSE_OK) {
        MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_video_info returned error %d", rv));
        return nullptr;
      }
      auto track = mozilla::MakeUnique<MP4VideoInfo>();
      track->Update(&info, &video);
      e = Move(track);
    }
    break;
    default:
      MOZ_LOG(sLog, LogLevel::Warning, ("unhandled track type %d", aType));
      return nullptr;
      break;
  }

  // No duration in track, use fragment_duration.
  if (e && !e->mDuration) {
    mp4parse_fragment_info info;
    auto rv = mp4parse_get_fragment_info(mRustParser.get(), &info);
    if (rv == MP4PARSE_OK) {
      e->mDuration = info.fragment_duration;
    }
  }

  if (e && e->IsValid()) {
    return e;
  }
  MOZ_LOG(sLog, LogLevel::Debug, ("TrackInfo didn't validate"));

  return nullptr;
}

bool
MP4MetadataRust::CanSeek() const
{
  MOZ_ASSERT(false, "Not yet implemented");
  return false;
}

const CryptoFile&
MP4MetadataRust::Crypto() const
{
  MOZ_ASSERT(false, "Not yet implemented");
  return mCrypto;
}

bool
MP4MetadataRust::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID)
{
  uint8_t fragmented = false;
  auto rv = mp4parse_is_fragmented(mRustParser.get(), aTrackID, &fragmented);
  if (rv != MP4PARSE_OK) {
    return false;
  }

  if (fragmented) {
    return true;
  }

  // For non-fragmented mp4.
  NS_WARNING("Not yet implemented");

  return false;
}

/*static*/ bool
MP4MetadataRust::HasCompleteMetadata(Stream* aSource)
{
  MOZ_ASSERT(false, "Not yet implemented");
  return false;
}

/*static*/ already_AddRefed<mozilla::MediaByteBuffer>
MP4MetadataRust::Metadata(Stream* aSource)
{
  MOZ_ASSERT(false, "Not yet implemented");
  return nullptr;
}
#endif

} // namespace mp4_demuxer
