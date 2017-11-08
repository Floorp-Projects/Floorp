/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mp4parse.h"

#include <limits>
#include <stdint.h>
#include <vector>

struct FreeMP4Parser { void operator()(mp4parse_parser* aPtr) { mp4parse_free(aPtr); } };

using mozilla::media::TimeUnit;

namespace mp4_demuxer
{

LazyLogModule gMP4MetadataLog("MP4Metadata");

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

  static MP4Metadata::ResultAndByteBuffer Metadata(Stream* aSource);

  MP4Metadata::ResultAndTrackCount
  GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  MP4Metadata::ResultAndTrackInfo GetTrackInfo(
    mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;
  bool CanSeek() const;

  MP4Metadata::ResultAndCryptoFile Crypto() const;

  MediaResult ReadTrackIndice(mp4parse_byte_data* aIndices, mozilla::TrackID aTrackID);

  bool Init();

private:
  void UpdateCrypto();
  Maybe<uint32_t> TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;

  CryptoFile mCrypto;
  RefPtr<Stream> mSource;
  RustStreamAdaptor mRustSource;
  mozilla::UniquePtr<mp4parse_parser, FreeMP4Parser> mRustParser;
};

class IndiceWrapperStagefright : public IndiceWrapper {
public:
  size_t Length() const override;

  bool GetIndice(size_t aIndex, Index::Indice& aIndice) const override;

  explicit IndiceWrapperStagefright(FallibleTArray<Index::Indice>& aIndice);

protected:
  FallibleTArray<Index::Indice> mIndice;
};

IndiceWrapperStagefright::IndiceWrapperStagefright(FallibleTArray<Index::Indice>& aIndice)
{
  mIndice.SwapElements(aIndice);
}

size_t
IndiceWrapperStagefright::Length() const
{
  return mIndice.Length();
}

bool
IndiceWrapperStagefright::GetIndice(size_t aIndex, Index::Indice& aIndice) const
{
  if (aIndex >= mIndice.Length()) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Index overflow in indice"));
    return false;
  }

  aIndice = mIndice[aIndex];
  return true;
}

// the owner of mIndice is rust mp4 paser, so lifetime of this class
// SHOULD NOT longer than rust parser.
class IndiceWrapperRust : public IndiceWrapper
{
public:
  size_t Length() const override;

  bool GetIndice(size_t aIndex, Index::Indice& aIndice) const override;

  explicit IndiceWrapperRust(mp4parse_byte_data& aRustIndice);

protected:
  UniquePtr<mp4parse_byte_data> mIndice;
};

IndiceWrapperRust::IndiceWrapperRust(mp4parse_byte_data& aRustIndice)
  : mIndice(mozilla::MakeUnique<mp4parse_byte_data>())
{
  mIndice->length = aRustIndice.length;
  mIndice->indices = aRustIndice.indices;
}

size_t
IndiceWrapperRust::Length() const
{
  return mIndice->length;
}

bool
IndiceWrapperRust::GetIndice(size_t aIndex, Index::Indice& aIndice) const
{
  if (aIndex >= mIndice->length) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Index overflow in indice"));
   return false;
  }

  const mp4parse_indice* indice = &mIndice->indices[aIndex];
  aIndice.start_offset = indice->start_offset;
  aIndice.end_offset = indice->end_offset;
  aIndice.start_composition = indice->start_composition;
  aIndice.end_composition = indice->end_composition;
  aIndice.start_decode = indice->start_decode;
  aIndice.sync = indice->sync;
  return true;
}

MP4Metadata::MP4Metadata(Stream* aSource)
 : mRust(MakeUnique<MP4MetadataRust>(aSource))
 , mReportedAudioTrackTelemetry(false)
 , mReportedVideoTrackTelemetry(false)
{
  mRust->Init();
}

MP4Metadata::~MP4Metadata()
{
}

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4Metadata::Metadata(Stream* aSource)
{
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  RefPtr<mozilla::MediaByteBuffer> buffer = parser->Metadata();
  if (!buffer) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse metadata")),
            nullptr};
  }
  return {NS_OK, Move(buffer)};
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

MP4Metadata::ResultAndTrackCount
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  MP4Metadata::ResultAndTrackCount numTracksRust =
    mRust->GetNumberTracks(aType);
  MOZ_LOG(gMP4MetadataLog, LogLevel::Info, ("%s tracks found: (%s)%u",
                                            TrackTypeToString(aType),
                                            numTracksRust.Result().Description().get(),
                                            numTracksRust.Ref()));

  return numTracksRust;
}

MP4Metadata::ResultAndTrackInfo
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                          size_t aTrackNumber) const
{
  MP4Metadata::ResultAndTrackInfo infoRust =
    mRust->GetTrackInfo(aType, aTrackNumber);

  return Move(infoRust);
}

bool
MP4Metadata::CanSeek() const
{
  // It always returns true in SF.
  return true;
}

MP4Metadata::ResultAndCryptoFile
MP4Metadata::Crypto() const
{
  MP4Metadata::ResultAndCryptoFile rustCrypto = mRust->Crypto();

  return rustCrypto;
}

MP4Metadata::ResultAndIndice
MP4Metadata::GetTrackIndice(mozilla::TrackID aTrackID)
{
  mp4parse_byte_data indiceRust = {};
  MediaResult rvRust = mRust->ReadTrackIndice(&indiceRust, aTrackID);
  if (NS_FAILED(rvRust)) {
    return {Move(rvRust), nullptr};
  }
  UniquePtr<IndiceWrapper> indice;
  indice = mozilla::MakeUnique<IndiceWrapperRust>(indiceRust);

  return {NS_OK, Move(indice)};
}

bool
RustStreamAdaptor::Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read)
{
  if (!mOffset.isValid()) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Overflow in source stream offset"));
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
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("Error reading source data"));
    return -1;
  }
  return bytes_read;
}

MP4MetadataRust::MP4MetadataRust(Stream* aSource)
  : mSource(aSource)
  , mRustSource(aSource)
{
}

MP4MetadataRust::~MP4MetadataRust()
{
}

bool
MP4MetadataRust::Init()
{
  mp4parse_io io = { read_source, &mRustSource };
  mRustParser.reset(mp4parse_new(&io));
  MOZ_ASSERT(mRustParser);

  if (MOZ_LOG_TEST(gMP4MetadataLog, LogLevel::Debug)) {
    mp4parse_log(true);
  }

  mp4parse_status rv = mp4parse_read(mRustParser.get());
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug, ("rust parser returned %d\n", rv));
  Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_SUCCESS,
                        rv == mp4parse_status_OK);
  if (rv != mp4parse_status_OK && rv != mp4parse_status_OOM) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Info, ("Rust mp4 parser fails to parse this stream."));
    MOZ_ASSERT(rv > 0);
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_ERROR_CODE, rv);
    return false;
  }

  UpdateCrypto();

  return true;
}

void
MP4MetadataRust::UpdateCrypto()
{
  mp4parse_pssh_info info = {};
  if (mp4parse_get_pssh_info(mRustParser.get(), &info) != mp4parse_status_OK) {
    return;
  }

  if (info.data.length == 0) {
    return;
  }

  mCrypto.Update(info.data.data, info.data.length);
}

bool
TrackTypeEqual(TrackInfo::TrackType aLHS, mp4parse_track_type aRHS)
{
  switch (aLHS) {
  case TrackInfo::kAudioTrack:
    return aRHS == mp4parse_track_type_AUDIO;
  case TrackInfo::kVideoTrack:
    return aRHS == mp4parse_track_type_VIDEO;
  default:
    return false;
  }
}

MP4Metadata::ResultAndTrackCount
MP4MetadataRust::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mRustParser.get(), &tracks);
  if (rv != mp4parse_status_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning,
        ("rust parser error %d counting tracks", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Rust parser error %d", rv)),
            MP4Metadata::NumberTracksError()};
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Info, ("rust parser found %u tracks", tracks));

  uint32_t total = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    mp4parse_track_info track_info;
    rv = mp4parse_get_track_info(mRustParser.get(), i, &track_info);
    if (rv != mp4parse_status_OK) {
      continue;
    }
    if (track_info.codec == mp4parse_codec::mp4parse_codec_UNKNOWN) {
      continue;
    }
    if (TrackTypeEqual(aType, track_info.track_type)) {
        total += 1;
    }
  }

  return {NS_OK, total};
}

Maybe<uint32_t>
MP4MetadataRust::TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const
{
  uint32_t tracks;
  auto rv = mp4parse_get_track_count(mRustParser.get(), &tracks);
  if (rv != mp4parse_status_OK) {
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
    if (rv != mp4parse_status_OK) {
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

MP4Metadata::ResultAndTrackInfo
MP4MetadataRust::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                              size_t aTrackNumber) const
{
  Maybe<uint32_t> trackIndex = TrackTypeToGlobalTrackIndex(aType, aTrackNumber);
  if (trackIndex.isNothing()) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("No %s tracks",
                                      TrackTypeToStr(aType))),
            nullptr};
  }

  mp4parse_track_info info;
  auto rv = mp4parse_get_track_info(mRustParser.get(), trackIndex.value(), &info);
  if (rv != mp4parse_status_OK) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_info returned %d", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot find %s track #%zu",
                                      TrackTypeToStr(aType),
                                      aTrackNumber)),
            nullptr};
  }
#ifdef DEBUG
  const char* codec_string = "unrecognized";
  switch (info.codec) {
    case mp4parse_codec_UNKNOWN: codec_string = "unknown"; break;
    case mp4parse_codec_AAC: codec_string = "aac"; break;
    case mp4parse_codec_OPUS: codec_string = "opus"; break;
    case mp4parse_codec_FLAC: codec_string = "flac"; break;
    case mp4parse_codec_AVC: codec_string = "h.264"; break;
    case mp4parse_codec_VP9: codec_string = "vp9"; break;
    case mp4parse_codec_MP3: codec_string = "mp3"; break;
    case mp4parse_codec_MP4V: codec_string = "mp4v"; break;
    case mp4parse_codec_JPEG: codec_string = "jpeg"; break;
    case mp4parse_codec_AC3: codec_string = "ac-3"; break;
    case mp4parse_codec_EC3: codec_string = "ec-3"; break;
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug,
    ("track codec %s (%u)\n", codec_string, info.codec));
#endif

  // This specialization interface is crazy.
  UniquePtr<mozilla::TrackInfo> e;
  switch (aType) {
    case TrackInfo::TrackType::kAudioTrack: {
      mp4parse_track_audio_info audio;
      auto rv = mp4parse_get_track_audio_info(mRustParser.get(), trackIndex.value(), &audio);
      if (rv != mp4parse_status_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_audio_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType),
                                          aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4AudioInfo>();
      track->Update(&info, &audio);
      e = Move(track);
    }
    break;
    case TrackInfo::TrackType::kVideoTrack: {
      mp4parse_track_video_info video;
      auto rv = mp4parse_get_track_video_info(mRustParser.get(), trackIndex.value(), &video);
      if (rv != mp4parse_status_OK) {
        MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("mp4parse_get_track_video_info returned error %d", rv));
        return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                            RESULT_DETAIL("Cannot parse %s track #%zu",
                                          TrackTypeToStr(aType),
                                          aTrackNumber)),
                nullptr};
      }
      auto track = mozilla::MakeUnique<MP4VideoInfo>();
      track->Update(&info, &video);
      e = Move(track);
    }
    break;
    default:
      MOZ_LOG(gMP4MetadataLog, LogLevel::Warning, ("unhandled track type %d", aType));
      return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                          RESULT_DETAIL("Cannot handle %s track #%zu",
                                        TrackTypeToStr(aType),
                                        aTrackNumber)),
              nullptr};
  }

  // No duration in track, use fragment_duration.
  if (e && !e->mDuration.IsPositive()) {
    mp4parse_fragment_info info;
    auto rv = mp4parse_get_fragment_info(mRustParser.get(), &info);
    if (rv == mp4parse_status_OK) {
      e->mDuration = TimeUnit::FromMicroseconds(info.fragment_duration);
    }
  }

  if (e && e->IsValid()) {
    return {NS_OK, Move(e)};
  }
  MOZ_LOG(gMP4MetadataLog, LogLevel::Debug, ("TrackInfo didn't validate"));

  return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                      RESULT_DETAIL("Invalid %s track #%zu",
                                    TrackTypeToStr(aType),
                                    aTrackNumber)),
          nullptr};
}

bool
MP4MetadataRust::CanSeek() const
{
  MOZ_ASSERT(false, "Not yet implemented");
  return false;
}

MP4Metadata::ResultAndCryptoFile
MP4MetadataRust::Crypto() const
{
  return {NS_OK, &mCrypto};
}

MediaResult
MP4MetadataRust::ReadTrackIndice(mp4parse_byte_data* aIndices, mozilla::TrackID aTrackID)
{
  uint8_t fragmented = false;
  auto rv = mp4parse_is_fragmented(mRustParser.get(), aTrackID, &fragmented);
  if (rv != mp4parse_status_OK) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot parse whether track id %d is "
                                     "fragmented, mp4parse_error=%d",
                                     int(aTrackID), int(rv)));
  }

  if (fragmented) {
    return NS_OK;
  }

  rv = mp4parse_get_indice_table(mRustParser.get(), aTrackID, aIndices);
  if (rv != mp4parse_status_OK) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot parse index table in track id %d, "
                                     "mp4parse_error=%d",
                                     int(aTrackID), int(rv)));
  }

  return NS_OK;
}

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4MetadataRust::Metadata(Stream* aSource)
{
  MOZ_ASSERT(false, "Not yet implemented");
  return {NS_ERROR_NOT_IMPLEMENTED, nullptr};
}

} // namespace mp4_demuxer
