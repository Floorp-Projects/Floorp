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

bool
MP4VideoInfo::IsValid() const
{
  return mDisplay.width > 0 && mDisplay.height > 0;
}

}
