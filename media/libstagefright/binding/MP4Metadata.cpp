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
#include "mozilla/SizePrintfMacros.h"
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

using namespace stagefright;
using mozilla::media::TimeUnit;

namespace mp4_demuxer
{

static LazyLogModule sLog("MP4Metadata");

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

  static MP4Metadata::ResultAndByteBuffer Metadata(Stream* aSource);

  MP4Metadata::ResultAndTrackCount
  GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  MP4Metadata::ResultAndTrackInfo GetTrackInfo(
    mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;
  bool CanSeek() const;

  MP4Metadata::ResultAndCryptoFile Crypto() const;

  MediaResult ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

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
    MOZ_LOG(sLog, LogLevel::Error, ("Index overflow in indice"));
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
    MOZ_LOG(sLog, LogLevel::Error, ("Index overflow in indice"));
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
 : mStagefright(MakeUnique<MP4MetadataStagefright>(aSource))
 , mRust(MakeUnique<MP4MetadataRust>(aSource))
 , mPreferRust(MediaPrefs::EnableRustMP4Parser())
 , mReportedAudioTrackTelemetry(false)
 , mReportedVideoTrackTelemetry(false)
#ifndef RELEASE_OR_BETA
 , mRustTestMode(MediaPrefs::RustTestMode())
#endif
{
}

MP4Metadata::~MP4Metadata()
{
}

/*static*/ MP4Metadata::ResultAndByteBuffer
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

MP4Metadata::ResultAndTrackCount
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const
{
  MP4Metadata::ResultAndTrackCount numTracks =
    mStagefright->GetNumberTracks(aType);

  if (!mRust) {
    return numTracks;
  }

  MP4Metadata::ResultAndTrackCount numTracksRust =
    mRust->GetNumberTracks(aType);
  MOZ_LOG(sLog, LogLevel::Info, ("%s tracks found: stagefright=(%s)%u rust=(%s)%u",
                                 TrackTypeToString(aType),
                                 numTracks.Result().Description().get(),
                                 numTracks.Ref(),
                                 numTracksRust.Result().Description().get(),
                                 numTracksRust.Ref()));

  // Consider '0' and 'error' the same for comparison purposes.
  // (Mostly because Stagefright never returns errors, but Rust may.)
  bool numTracksMatch =
    (numTracks.Ref() != NumberTracksError() ? numTracks.Ref() : 0) ==
    (numTracksRust.Ref() != NumberTracksError() ? numTracksRust.Ref() : 0);

  if (aType == mozilla::TrackInfo::kAudioTrack && !mReportedAudioTrackTelemetry) {
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_TRACK_MATCH_AUDIO,
                          numTracksMatch);
    mReportedAudioTrackTelemetry = true;
  } else if (aType == mozilla::TrackInfo::kVideoTrack && !mReportedVideoTrackTelemetry) {
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_TRACK_MATCH_VIDEO,
                            numTracksMatch);
    mReportedVideoTrackTelemetry = true;
  }

  if (!numTracksMatch &&
      MediaPrefs::MediaWarningsAsErrorsStageFrightVsRust()) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Different numbers of tracks: "
                                      "Stagefright=%u (%s) Rust=%u (%s)",
                                      numTracks.Ref(),
                                      numTracks.Result().Description().get(),
                                      numTracksRust.Ref(),
                                      numTracksRust.Result().Description().get())),
            NumberTracksError()};
  }

  // If we prefer Rust, just return it.
  if (mPreferRust || ShouldPreferRust()) {
    MOZ_LOG(sLog, LogLevel::Info, ("Preferring rust demuxer"));
    mPreferRust = true;
    return numTracksRust;
  }

  // If numbers are different, return the stagefright number with a warning.
  if (!numTracksMatch) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Different numbers of tracks: "
                                      "Stagefright=%u (%s) Rust=%u (%s)",
                                      numTracks.Ref(),
                                      numTracks.Result().Description().get(),
                                      numTracksRust.Ref(),
                                      numTracksRust.Result().Description().get())),
            numTracks.Ref()};
  }

  // Numbers are effectively the same.

  // Error(s) -> Combine both messages to get more details out.
  if (numTracks.Ref() == NumberTracksError() ||
      numTracksRust.Ref() == NumberTracksError()) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Errors: "
                                      "Stagefright=(%s) Rust=(%s)",
                                      numTracks.Result().Description().get(),
                                      numTracksRust.Result().Description().get())),
            numTracks.Ref()};
  }

  // Same non-error numbers, just return any.
  // (Choosing Rust here, in case it carries a warning, we'd want to know that.)
  return numTracksRust;
}

bool MP4Metadata::ShouldPreferRust() const {
  if (!mRust) {
    return false;
  }
  // See if there's an Opus track.
  MP4Metadata::ResultAndTrackCount numTracks =
    mRust->GetNumberTracks(TrackInfo::kAudioTrack);
  if (numTracks.Ref() != NumberTracksError()) {
    for (auto i = 0; i < numTracks.Ref(); i++) {
      MP4Metadata::ResultAndTrackInfo info =
        mRust->GetTrackInfo(TrackInfo::kAudioTrack, i);
      if (!info.Ref()) {
        return false;
      }
      if (info.Ref()->mMimeType.EqualsASCII("audio/opus") ||
          info.Ref()->mMimeType.EqualsASCII("audio/flac")) {
        return true;
      }
    }
  }

  numTracks = mRust->GetNumberTracks(TrackInfo::kVideoTrack);
  if (numTracks.Ref() != NumberTracksError()) {
    for (auto i = 0; i < numTracks.Ref(); i++) {
      MP4Metadata::ResultAndTrackInfo info =
        mRust->GetTrackInfo(TrackInfo::kVideoTrack, i);
      if (!info.Ref()) {
        return false;
      }
      if (info.Ref()->mMimeType.EqualsASCII("video/vp9")) {
        return true;
      }
    }
  }
  // Otherwise, fall back.
  return false;
}

static const char*
GetDifferentField(const mozilla::TrackInfo& info,
                  const mozilla::TrackInfo& infoRust)
{
  if (infoRust.mId != info.mId) { return "Id"; }
  if (infoRust.mKind == info.mKind) { return "Kind"; }
  if (infoRust.mLabel == info.mLabel) { return "Label"; }
  if (infoRust.mLanguage == info.mLanguage) { return "Language"; }
  if (infoRust.mEnabled == info.mEnabled) { return "Enabled"; }
  if (infoRust.mTrackId == info.mTrackId) { return "TrackId"; }
  if (infoRust.mMimeType == info.mMimeType) { return "MimeType"; }
  if (infoRust.mDuration == info.mDuration) { return "Duration"; }
  if (infoRust.mMediaTime == info.mMediaTime) { return "MediaTime"; }
  if (infoRust.mCrypto.mValid == info.mCrypto.mValid) { return "Crypto-Valid"; }
  if (infoRust.mCrypto.mMode == info.mCrypto.mMode) { return "Crypto-Mode"; }
  if (infoRust.mCrypto.mIVSize == info.mCrypto.mIVSize) { return "Crypto-IVSize"; }
  if (infoRust.mCrypto.mKeyId == info.mCrypto.mKeyId) { return "Crypto-KeyId"; }
  switch (info.GetType()) {
  case mozilla::TrackInfo::kAudioTrack: {
    const AudioInfo *audioRust = infoRust.GetAsAudioInfo();
    const AudioInfo *audio = info.GetAsAudioInfo();
    if (audioRust->mRate == audio->mRate) { return "Rate"; }
    if (audioRust->mChannels == audio->mChannels) { return "Channels"; }
    if (audioRust->mBitDepth == audio->mBitDepth) { return "BitDepth"; }
    if (audioRust->mProfile == audio->mProfile) { return "Profile"; }
    if (audioRust->mExtendedProfile == audio->mExtendedProfile) { return "ExtendedProfile"; }
    break;
  }
  case mozilla::TrackInfo::kVideoTrack: {
    const VideoInfo *videoRust = infoRust.GetAsVideoInfo();
    const VideoInfo *video = info.GetAsVideoInfo();
    if (videoRust->mDisplay == video->mDisplay) { return "Display"; }
    if (videoRust->mImage == video->mImage) { return "Image"; }
    if (*videoRust->mExtraData == *video->mExtraData) { return "ExtraData"; }
    // mCodecSpecificConfig is for video/mp4-es, not video/avc. Since video/mp4-es
    // is supported on b2g only, it could be removed from TrackInfo.
    if (*videoRust->mCodecSpecificConfig == *video->mCodecSpecificConfig) { return "CodecSpecificConfig"; }
    break;
  }
  default:
    break;
  }

  return nullptr;
}

MP4Metadata::ResultAndTrackInfo
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                          size_t aTrackNumber) const
{
  MP4Metadata::ResultAndTrackInfo info =
    mStagefright->GetTrackInfo(aType, aTrackNumber);

  if (!mRust) {
    return info;
  }

  MP4Metadata::ResultAndTrackInfo infoRust =
    mRust->GetTrackInfo(aType, aTrackNumber);

#ifndef RELEASE_OR_BETA
  if (mRustTestMode && info.Ref() && infoRust.Ref()) {
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mId == info.Ref()->mId);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mKind == info.Ref()->mKind);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mLabel == info.Ref()->mLabel);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mLanguage == info.Ref()->mLanguage);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mEnabled == info.Ref()->mEnabled);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mTrackId == info.Ref()->mTrackId);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mMimeType == info.Ref()->mMimeType);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mDuration == info.Ref()->mDuration);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mMediaTime == info.Ref()->mMediaTime);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mCrypto.mValid == info.Ref()->mCrypto.mValid);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mCrypto.mMode == info.Ref()->mCrypto.mMode);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mCrypto.mIVSize == info.Ref()->mCrypto.mIVSize);
    MOZ_DIAGNOSTIC_ASSERT(infoRust.Ref()->mCrypto.mKeyId == info.Ref()->mCrypto.mKeyId);
    switch (aType) {
    case mozilla::TrackInfo::kAudioTrack: {
      AudioInfo *audioRust = infoRust.Ref()->GetAsAudioInfo();
      AudioInfo *audio = info.Ref()->GetAsAudioInfo();
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mRate == audio->mRate);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mChannels == audio->mChannels);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mBitDepth == audio->mBitDepth);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mProfile == audio->mProfile);
      MOZ_DIAGNOSTIC_ASSERT(audioRust->mExtendedProfile == audio->mExtendedProfile);
      MOZ_DIAGNOSTIC_ASSERT(*audioRust->mExtraData == *audio->mExtraData);
      MOZ_DIAGNOSTIC_ASSERT(*audioRust->mCodecSpecificConfig == *audio->mCodecSpecificConfig);
      break;
    }
    case mozilla::TrackInfo::kVideoTrack: {
      VideoInfo *videoRust = infoRust.Ref()->GetAsVideoInfo();
      VideoInfo *video = info.Ref()->GetAsVideoInfo();
      MOZ_DIAGNOSTIC_ASSERT(videoRust->mDisplay == video->mDisplay);
      MOZ_DIAGNOSTIC_ASSERT(videoRust->mImage == video->mImage);
      MOZ_DIAGNOSTIC_ASSERT(videoRust->mRotation == video->mRotation);
      MOZ_DIAGNOSTIC_ASSERT(*videoRust->mExtraData == *video->mExtraData);
      // mCodecSpecificConfig is for video/mp4-es, not video/avc. Since video/mp4-es
      // is supported on b2g only, it could be removed from TrackInfo.
      MOZ_DIAGNOSTIC_ASSERT(*videoRust->mCodecSpecificConfig == *video->mCodecSpecificConfig);
      break;
    }
    default:
      break;
    }
  }
#endif

  if (info.Ref() && infoRust.Ref()) {
    const char* diff = GetDifferentField(*info.Ref(), *infoRust.Ref());
    if (diff) {
      return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                          RESULT_DETAIL("Different field '%s' between "
                                        "Stagefright (%s) and Rust (%s)",
                                        diff,
                                        info.Result().Description().get(),
                                        infoRust.Result().Description().get())),
              MediaPrefs::MediaWarningsAsErrorsStageFrightVsRust()
              ? mozilla::UniquePtr<mozilla::TrackInfo>(nullptr)
              : mPreferRust ? Move(infoRust.Ref()) : Move(info.Ref())};
    }
  }

  if (mPreferRust) {
    return infoRust;
  }

  return info;
}

bool
MP4Metadata::CanSeek() const
{
  return mStagefright->CanSeek();
}

MP4Metadata::ResultAndCryptoFile
MP4Metadata::Crypto() const
{
  MP4Metadata::ResultAndCryptoFile crypto = mStagefright->Crypto();
  MP4Metadata::ResultAndCryptoFile rustCrypto = mRust->Crypto();

#ifndef RELEASE_OR_BETA
  if (mRustTestMode) {
    MOZ_DIAGNOSTIC_ASSERT(rustCrypto.Ref()->pssh == crypto.Ref()->pssh);
  }
#endif

  if (rustCrypto.Ref()->pssh != crypto.Ref()->pssh) {
    return {MediaResult(
             NS_ERROR_DOM_MEDIA_METADATA_ERR,
             RESULT_DETAIL("Mismatch between Stagefright (%s) and Rust (%s) crypto file",
                           crypto.Result().Description().get(),
                           rustCrypto.Result().Description().get())),
            mPreferRust ? rustCrypto.Ref() : crypto.Ref()};
  }

  if (mPreferRust) {
    return rustCrypto;
  }

  return crypto;
}

MP4Metadata::ResultAndIndice
MP4Metadata::GetTrackIndice(mozilla::TrackID aTrackID)
{
  FallibleTArray<Index::Indice> indiceSF;
  if (!mPreferRust
#ifndef RELEASE_OR_BETA
      || mRustTestMode
#endif
     ) {
    MediaResult rv = mStagefright->ReadTrackIndex(indiceSF, aTrackID);
    if (NS_FAILED(rv)) {
      return {Move(rv), nullptr};
    }
  }

  mp4parse_byte_data indiceRust = {};
  if (mPreferRust
#ifndef RELEASE_OR_BETA
      || mRustTestMode
#endif
     ) {
    MediaResult rvRust = mRust->ReadTrackIndice(&indiceRust, aTrackID);
    if (NS_FAILED(rvRust)) {
      return {Move(rvRust), nullptr};
    }
  }

#ifndef RELEASE_OR_BETA
  if (mRustTestMode) {
    MOZ_DIAGNOSTIC_ASSERT(indiceRust.length == indiceSF.Length());
    for (uint32_t i = 0; i < indiceRust.length; i++) {
      MOZ_DIAGNOSTIC_ASSERT(indiceRust.indices[i].start_offset == indiceSF[i].start_offset);
      MOZ_DIAGNOSTIC_ASSERT(indiceRust.indices[i].end_offset == indiceSF[i].end_offset);
      MOZ_DIAGNOSTIC_ASSERT(llabs(indiceRust.indices[i].start_composition - int64_t(indiceSF[i].start_composition)) <= 1);
      MOZ_DIAGNOSTIC_ASSERT(llabs(indiceRust.indices[i].end_composition - int64_t(indiceSF[i].end_composition)) <= 1);
      MOZ_DIAGNOSTIC_ASSERT(llabs(indiceRust.indices[i].start_decode - int64_t(indiceSF[i].start_decode)) <= 1);
      MOZ_DIAGNOSTIC_ASSERT(indiceRust.indices[i].sync == indiceSF[i].sync);
    }
  }
#endif

  UniquePtr<IndiceWrapper> indice;
  if (mPreferRust) {
    indice = mozilla::MakeUnique<IndiceWrapperRust>(indiceRust);
  } else {
    indice = mozilla::MakeUnique<IndiceWrapperStagefright>(indiceSF);
  }

  return {NS_OK, Move(indice)};
}

static inline MediaResult
ConvertIndex(FallibleTArray<Index::Indice>& aDest,
             const nsTArray<stagefright::MediaSource::Indice>& aIndex,
             int64_t aMediaTime)
{
  if (!aDest.SetCapacity(aIndex.Length(), mozilla::fallible)) {
    return MediaResult{NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("Could not resize to %" PRIuSIZE " indices",
                                     aIndex.Length())};
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
    MOZ_LOG(sLog, LogLevel::Debug, ("s_o: %" PRIu64 ", e_o: %" PRIu64 ", s_c: %" PRIu64 ", e_c: %" PRIu64 ", s_d: %" PRIu64 ", sync: %d\n",
                                    indice.start_offset, indice.end_offset, indice.start_composition, indice.end_composition,
                                    indice.start_decode, indice.sync));
  }
  return NS_OK;
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

MP4Metadata::ResultAndTrackCount
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
  return {NS_OK, total};
}

MP4Metadata::ResultAndTrackInfo
MP4MetadataStagefright::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                     size_t aTrackNumber) const
{
  size_t tracks = mMetadataExtractor->countTracks();
  if (!tracks) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("No %s tracks",
                                      TrackTypeToStr(aType))),
            nullptr};
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
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot access %s track #%zu",
                                      TrackTypeToStr(aType),
                                      aTrackNumber)),
            nullptr};
  }

  UniquePtr<mozilla::TrackInfo> e = CheckTrack(mimeType, metaData.get(), index);

  if (e) {
    metaData = mMetadataExtractor->getMetaData();
    int64_t movieDuration;
    if (!e->mDuration.IsPositive() &&
        metaData->findInt64(kKeyMovieDuration, &movieDuration)) {
      // No duration in track, use movie extend header box one.
      e->mDuration = TimeUnit::FromMicroseconds(movieDuration);
    }
  }

  return {NS_OK, Move(e)};
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

MP4Metadata::ResultAndCryptoFile
MP4MetadataStagefright::Crypto() const
{
  return {NS_OK, &mCrypto};
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

MediaResult
MP4MetadataStagefright::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID)
{
  size_t numTracks = mMetadataExtractor->countTracks();
  int32_t trackNumber = GetTrackNumber(aTrackID);
  if (trackNumber < 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot find track id %d",
                                     int(aTrackID)));
  }
  sp<MediaSource> track = mMetadataExtractor->getTrack(trackNumber);
  if (!track.get()) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot access track id %d",
                                     int(aTrackID)));
  }
  sp<MetaData> metadata = mMetadataExtractor->getTrackMetaData(trackNumber);
  int64_t mediaTime;
  if (!metadata->findInt64(kKeyMediaTime, &mediaTime)) {
    mediaTime = 0;
  }

  return ConvertIndex(aDest, track->exportIndex(), mediaTime);
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

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4MetadataStagefright::Metadata(Stream* aSource)
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

bool
RustStreamAdaptor::Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read)
{
  if (!mOffset.isValid()) {
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

  if (MOZ_LOG_TEST(sLog, LogLevel::Debug)) {
    mp4parse_log(true);
  }

  mp4parse_status rv = mp4parse_read(mRustParser.get());
  MOZ_LOG(sLog, LogLevel::Debug, ("rust parser returned %d\n", rv));
  Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_SUCCESS,
                        rv == mp4parse_status_OK);
  if (rv != mp4parse_status_OK) {
    MOZ_ASSERT(rv > 0);
    Telemetry::Accumulate(Telemetry::MEDIA_RUST_MP4PARSE_ERROR_CODE, rv);
  }

  UpdateCrypto();
}

MP4MetadataRust::~MP4MetadataRust()
{
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
    MOZ_LOG(sLog, LogLevel::Warning,
        ("rust parser error %d counting tracks", rv));
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Rust parser error %d", rv)),
            MP4Metadata::NumberTracksError()};
  }
  MOZ_LOG(sLog, LogLevel::Info, ("rust parser found %u tracks", tracks));

  uint32_t total = 0;
  for (uint32_t i = 0; i < tracks; ++i) {
    mp4parse_track_info track_info;
    rv = mp4parse_get_track_info(mRustParser.get(), i, &track_info);
    if (rv != mp4parse_status_OK) {
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
    MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_info returned %d", rv));
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
      if (rv != mp4parse_status_OK) {
        MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_audio_info returned error %d", rv));
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
        MOZ_LOG(sLog, LogLevel::Warning, ("mp4parse_get_track_video_info returned error %d", rv));
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
      MOZ_LOG(sLog, LogLevel::Warning, ("unhandled track type %d", aType));
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
  MOZ_LOG(sLog, LogLevel::Debug, ("TrackInfo didn't validate"));

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
