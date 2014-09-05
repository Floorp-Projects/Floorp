/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/DecoderData.h"
#include "media/stagefright/MetaData.h"
#include "media/stagefright/MediaBuffer.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/Utils.h"
#include "mozilla/ArrayUtils.h"
#include "include/ESDS.h"

using namespace stagefright;

namespace mp4_demuxer
{

static int32_t
FindInt32(sp<MetaData>& mMetaData, uint32_t mKey)
{
  int32_t value;
  if (!mMetaData->findInt32(mKey, &value))
    return 0;
  return value;
}

static int64_t
FindInt64(sp<MetaData>& mMetaData, uint32_t mKey)
{
  int64_t value;
  if (!mMetaData->findInt64(mKey, &value))
    return 0;
  return value;
}

template <typename T, size_t N>
static bool
FindData(sp<MetaData>& aMetaData, uint32_t aKey, mozilla::Vector<T, N>* aDest)
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
FindData(sp<MetaData>& aMetaData, uint32_t aKey, nsTArray<T>* aDest)
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

bool
CryptoFile::DoUpdate(sp<MetaData>& aMetaData)
{
  const void* data;
  size_t size;
  uint32_t type;

  // There's no point in checking that the type matches anything because it
  // isn't set consistently in the MPEG4Extractor.
  if (!aMetaData->findData(kKeyPssh, &type, &data, &size)) {
    return false;
  }

  ByteReader reader(reinterpret_cast<const uint8_t*>(data), size);
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

void
CryptoTrack::Update(sp<MetaData>& aMetaData)
{
  valid = aMetaData->findInt32(kKeyCryptoMode, &mode) &&
          aMetaData->findInt32(kKeyCryptoDefaultIVSize, &iv_size) &&
          FindData(aMetaData, kKeyCryptoKey, &key);
}

void
CryptoSample::Update(sp<MetaData>& aMetaData)
{
  CryptoTrack::Update(aMetaData);
  valid = valid && FindData(aMetaData, kKeyPlainSizes, &plain_sizes) &&
          FindData(aMetaData, kKeyEncryptedSizes, &encrypted_sizes) &&
          FindData(aMetaData, kKeyCryptoIV, &iv);
}

void
TrackConfig::Update(sp<MetaData>& aMetaData, const char* aMimeType)
{
  // aMimeType points to a string from MediaDefs.cpp so we don't need to copy it
  mime_type = aMimeType;
  duration = FindInt64(aMetaData, kKeyDuration);
  mTrackId = FindInt32(aMetaData, kKeyTrackID);
  crypto.Update(aMetaData);
}

void
AudioDecoderConfig::Update(sp<MetaData>& aMetaData, const char* aMimeType)
{
  TrackConfig::Update(aMetaData, aMimeType);
  channel_count = FindInt32(aMetaData, kKeyChannelCount);
  bits_per_sample = FindInt32(aMetaData, kKeySampleSize);
  samples_per_second = FindInt32(aMetaData, kKeySampleRate);
  frequency_index = Adts::GetFrequencyIndex(samples_per_second);
  aac_profile = FindInt32(aMetaData, kKeyAACProfile);

  if (FindData(aMetaData, kKeyESDS, &extra_data)) {
    ESDS esds(&extra_data[0], extra_data.length());

    const void* data;
    size_t size;
    if (esds.getCodecSpecificInfo(&data, &size) == OK) {
      audio_specific_config.append(reinterpret_cast<const uint8_t*>(data),
                                   size);
    }
  }
}

bool
AudioDecoderConfig::IsValid()
{
  return channel_count > 0 && samples_per_second > 0 && frequency_index > 0 &&
         (mime_type != MEDIA_MIMETYPE_AUDIO_AAC || aac_profile > 0);
}

void
VideoDecoderConfig::Update(sp<MetaData>& aMetaData, const char* aMimeType)
{
  TrackConfig::Update(aMetaData, aMimeType);
  display_width = FindInt32(aMetaData, kKeyDisplayWidth);
  display_height = FindInt32(aMetaData, kKeyDisplayHeight);

  if (FindData(aMetaData, kKeyAVCC, &extra_data) && extra_data.length() >= 7) {
    // Set size of the NAL length to 4. The demuxer formats its output with
    // this NAL length size.
    extra_data[4] |= 3;
    annex_b = AnnexB::ConvertExtraDataToAnnexB(extra_data);
  }
}

bool
VideoDecoderConfig::IsValid()
{
  return display_width > 0 && display_height > 0;
}

MP4Sample::MP4Sample()
  : mMediaBuffer(nullptr)
  , decode_timestamp(0)
  , composition_timestamp(0)
  , duration(0)
  , byte_offset(0)
  , is_sync_point(0)
  , data(nullptr)
  , size(0)
{
}

MP4Sample::~MP4Sample()
{
  if (mMediaBuffer) {
    mMediaBuffer->release();
  }
}

void
MP4Sample::Update()
{
  sp<MetaData> m = mMediaBuffer->meta_data();
  decode_timestamp = FindInt64(m, kKeyDecodingTime);
  composition_timestamp = FindInt64(m, kKeyTime);
  duration = FindInt64(m, kKeyDuration);
  byte_offset = FindInt64(m, kKey64BitFileOffset);
  is_sync_point = FindInt32(m, kKeyIsSyncFrame);
  data = reinterpret_cast<uint8_t*>(mMediaBuffer->data());
  size = mMediaBuffer->range_length();

  crypto.Update(m);
}

void
MP4Sample::Pad(size_t aPaddingBytes)
{
  size_t newSize = size + aPaddingBytes;

  // If the existing MediaBuffer has enough space then we just recycle it. If
  // not then we copy to a new buffer.
  uint8_t* newData = mMediaBuffer && newSize <= mMediaBuffer->size()
                       ? data
                       : new uint8_t[newSize];

  memset(newData + size, 0, aPaddingBytes);

  if (newData != data) {
    memcpy(newData, data, size);
    extra_buffer = data = newData;
    if (mMediaBuffer) {
      mMediaBuffer->release();
      mMediaBuffer = nullptr;
    }
  }
}

void
MP4Sample::Prepend(const uint8_t* aData, size_t aSize)
{
  size_t newSize = size + aSize;

  // If the existing MediaBuffer has enough space then we just recycle it. If
  // not then we copy to a new buffer.
  uint8_t* newData = mMediaBuffer && newSize <= mMediaBuffer->size()
                       ? data
                       : new uint8_t[newSize];

  memmove(newData + aSize, data, size);
  memmove(newData, aData, aSize);
  size = newSize;

  if (newData != data) {
    extra_buffer = data = newData;
    if (mMediaBuffer) {
      mMediaBuffer->release();
      mMediaBuffer = nullptr;
    }
  }
}
}
