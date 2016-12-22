/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/DecoderData.h"
#include <media/stagefright/foundation/ABitReader.h>
#include "media/stagefright/MetaData.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/Utils.h"
#include "mozilla/ArrayUtils.h"
#include "include/ESDS.h"

#ifdef MOZ_RUST_MP4PARSE
// OpusDecoder header is really needed only by MP4 in rust
#include "OpusDecoder.h"
#include "mp4parse.h"
#endif

using namespace stagefright;

namespace mp4_demuxer
{

static int32_t
FindInt32(const MetaData* mMetaData, uint32_t mKey)
{
  int32_t value;
  if (!mMetaData->findInt32(mKey, &value))
    return 0;
  return value;
}

static int64_t
FindInt64(const MetaData* mMetaData, uint32_t mKey)
{
  int64_t value;
  if (!mMetaData->findInt64(mKey, &value))
    return 0;
  return value;
}

template <typename T, size_t N>
static bool
FindData(const MetaData* aMetaData, uint32_t aKey, mozilla::Vector<T, N>* aDest)
{
  const void* data;
  size_t size;
  uint32_t type;

  aDest->clear();
  // There's no point in checking that the type matches anything because it
  // isn't set consistently in the MPEG4Extractor.
  if (!aMetaData->findData(aKey, &type, &data, &size) || size % sizeof(T)) {
    return false;
  }

  aDest->append(reinterpret_cast<const T*>(data), size / sizeof(T));
  return true;
}

template <typename T>
static bool
FindData(const MetaData* aMetaData, uint32_t aKey, nsTArray<T>* aDest)
{
  const void* data;
  size_t size;
  uint32_t type;

  aDest->Clear();
  // There's no point in checking that the type matches anything because it
  // isn't set consistently in the MPEG4Extractor.
  if (!aMetaData->findData(aKey, &type, &data, &size) || size % sizeof(T)) {
    return false;
  }

  aDest->AppendElements(reinterpret_cast<const T*>(data), size / sizeof(T));
  return true;
}

static bool
FindData(const MetaData* aMetaData, uint32_t aKey, mozilla::MediaByteBuffer* aDest)
{
  return FindData(aMetaData, aKey, static_cast<nsTArray<uint8_t>*>(aDest));
}

bool
CryptoFile::DoUpdate(const uint8_t* aData, size_t aLength)
{
  ByteReader reader(aData, aLength);
  while (reader.Remaining()) {
    PsshInfo psshInfo;
    if (!reader.ReadArray(psshInfo.uuid, 16)) {
      return false;
    }

    if (!reader.CanReadType<uint32_t>()) {
      return false;
    }
    auto length = reader.ReadType<uint32_t>();

    if (!reader.ReadArray(psshInfo.data, length)) {
      return false;
    }
    pssh.AppendElement(psshInfo);
  }
  return true;
}

static void
UpdateTrackInfo(mozilla::TrackInfo& aConfig,
                const MetaData* aMetaData,
                const char* aMimeType)
{
  mozilla::CryptoTrack& crypto = aConfig.mCrypto;
  aConfig.mMimeType = aMimeType;
  aConfig.mDuration = FindInt64(aMetaData, kKeyDuration);
  aConfig.mMediaTime = FindInt64(aMetaData, kKeyMediaTime);
  aConfig.mTrackId = FindInt32(aMetaData, kKeyTrackID);
  aConfig.mCrypto.mValid = aMetaData->findInt32(kKeyCryptoMode, &crypto.mMode) &&
    aMetaData->findInt32(kKeyCryptoDefaultIVSize, &crypto.mIVSize) &&
    FindData(aMetaData, kKeyCryptoKey, &crypto.mKeyId);
}

void
MP4AudioInfo::Update(const MetaData* aMetaData,
                     const char* aMimeType)
{
  UpdateTrackInfo(*this, aMetaData, aMimeType);
  mChannels = FindInt32(aMetaData, kKeyChannelCount);
  mBitDepth = FindInt32(aMetaData, kKeySampleSize);
  mRate = FindInt32(aMetaData, kKeySampleRate);
  mProfile = FindInt32(aMetaData, kKeyAACProfile);

  if (FindData(aMetaData, kKeyESDS, mExtraData)) {
    ESDS esds(mExtraData->Elements(), mExtraData->Length());

    const void* data;
    size_t size;
    if (esds.getCodecSpecificInfo(&data, &size) == OK) {
      const uint8_t* cdata = reinterpret_cast<const uint8_t*>(data);
      mCodecSpecificConfig->AppendElements(cdata, size);
      if (size > 1) {
        ABitReader br(cdata, size);
        mExtendedProfile = br.getBits(5);

        if (mExtendedProfile == 31) {  // AAC-ELD => additional 6 bits
          mExtendedProfile = 32 + br.getBits(6);
        }
      }
    }
  }
}

bool
MP4AudioInfo::IsValid() const
{
  return mChannels > 0 && mRate > 0 &&
         // Accept any mime type here, but if it's aac, validate the profile.
         (!mMimeType.Equals(MEDIA_MIMETYPE_AUDIO_AAC) ||
          mProfile > 0 || mExtendedProfile > 0);
}

void
MP4VideoInfo::Update(const MetaData* aMetaData, const char* aMimeType)
{
  UpdateTrackInfo(*this, aMetaData, aMimeType);
  mDisplay.width = FindInt32(aMetaData, kKeyDisplayWidth);
  mDisplay.height = FindInt32(aMetaData, kKeyDisplayHeight);
  mImage.width = FindInt32(aMetaData, kKeyWidth);
  mImage.height = FindInt32(aMetaData, kKeyHeight);
  mRotation = VideoInfo::ToSupportedRotation(FindInt32(aMetaData, kKeyRotation));

  FindData(aMetaData, kKeyAVCC, mExtraData);
  if (!mExtraData->Length()) {
    if (FindData(aMetaData, kKeyESDS, mExtraData)) {
      ESDS esds(mExtraData->Elements(), mExtraData->Length());

      const void* data;
      size_t size;
      if (esds.getCodecSpecificInfo(&data, &size) == OK) {
        const uint8_t* cdata = reinterpret_cast<const uint8_t*>(data);
        mCodecSpecificConfig->AppendElements(cdata, size);
      }
    }
  }

}

#ifdef MOZ_RUST_MP4PARSE
static void
UpdateTrackProtectedInfo(mozilla::TrackInfo& aConfig,
                         const mp4parser_sinf_info& aSinf)
{
  if (aSinf.is_encrypted != 0) {
    aConfig.mCrypto.mValid = true;
    aConfig.mCrypto.mMode = aSinf.is_encrypted;
    aConfig.mCrypto.mIVSize = aSinf.iv_size;
    aConfig.mCrypto.mKeyId.AppendElements(aSinf.kid.data, aSinf.kid.length);
  }
}

void
MP4AudioInfo::Update(const mp4parse_track_info* track,
                     const mp4parse_track_audio_info* audio)
{
  UpdateTrackProtectedInfo(*this, audio->protected_data);

  if (track->codec == MP4PARSE_CODEC_OPUS) {
    mMimeType = NS_LITERAL_CSTRING("audio/opus");
    // The Opus decoder expects the container's codec delay or
    // pre-skip value, in microseconds, as a 64-bit int at the
    // start of the codec-specific config blob.
    MOZ_ASSERT(audio->codec_specific_config.data);
    MOZ_ASSERT(audio->codec_specific_config.length >= 12);
    uint16_t preskip =
      LittleEndian::readUint16(audio->codec_specific_config.data + 10);
    OpusDataDecoder::AppendCodecDelay(mCodecSpecificConfig,
        mozilla::FramesToUsecs(preskip, 48000).value());
  } else if (track->codec == MP4PARSE_CODEC_AAC) {
    mMimeType = MEDIA_MIMETYPE_AUDIO_AAC;
  } else if (track->codec == MP4PARSE_CODEC_FLAC) {
    mMimeType = MEDIA_MIMETYPE_AUDIO_FLAC;
  } else if (track->codec == MP4PARSE_CODEC_MP3) {
    mMimeType = MEDIA_MIMETYPE_AUDIO_MPEG;
  }

  mRate = audio->sample_rate;
  mChannels = audio->channels;
  mBitDepth = audio->bit_depth;
  mExtendedProfile = audio->profile;
  mDuration = track->duration;
  mMediaTime = track->media_time;
  mTrackId = track->track_id;

  // In stagefright, mProfile is kKeyAACProfile, mExtendedProfile is kKeyAACAOT.
  // Both are from audioObjectType in AudioSpecificConfig.
  if (audio->profile <= 4) {
    mProfile = audio->profile;
  }

  const uint8_t* cdata = audio->codec_specific_config.data;
  size_t size = audio->codec_specific_config.length;
  if (size > 0) {
    mCodecSpecificConfig->AppendElements(cdata, size);
  }
}

void
MP4VideoInfo::Update(const mp4parse_track_info* track,
                     const mp4parse_track_video_info* video)
{
  UpdateTrackProtectedInfo(*this, video->protected_data);
  if (track->codec == MP4PARSE_CODEC_AVC) {
    mMimeType = MEDIA_MIMETYPE_VIDEO_AVC;
  } else if (track->codec == MP4PARSE_CODEC_VP9) {
    mMimeType = NS_LITERAL_CSTRING("video/vp9");
  }
  mTrackId = track->track_id;
  mDuration = track->duration;
  mMediaTime = track->media_time;
  mDisplay.width = video->display_width;
  mDisplay.height = video->display_height;
  mImage.width = video->image_width;
  mImage.height = video->image_height;
  if (video->extra_data.data) {
    mExtraData->AppendElements(video->extra_data.data, video->extra_data.length);
  }
}
#endif

bool
MP4VideoInfo::IsValid() const
{
  return (mDisplay.width > 0 && mDisplay.height > 0) ||
    (mImage.width > 0 && mImage.height > 0);
}

}
