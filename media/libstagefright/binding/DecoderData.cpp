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
FindData(const MetaData* aMetaData, uint32_t aKey, ByteBuffer* aDest)
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

void
TrackConfig::Update(const MetaData* aMetaData, const char* aMimeType)
{
  mime_type = aMimeType;
  duration = FindInt64(aMetaData, kKeyDuration);
  media_time = FindInt64(aMetaData, kKeyMediaTime);
  mTrackId = FindInt32(aMetaData, kKeyTrackID);
  crypto.valid = aMetaData->findInt32(kKeyCryptoMode, &crypto.mode) &&
    aMetaData->findInt32(kKeyCryptoDefaultIVSize, &crypto.iv_size) &&
    FindData(aMetaData, kKeyCryptoKey, &crypto.key);
}

void
AudioDecoderConfig::Update(const MetaData* aMetaData, const char* aMimeType)
{
  TrackConfig::Update(aMetaData, aMimeType);
  channel_count = FindInt32(aMetaData, kKeyChannelCount);
  bits_per_sample = FindInt32(aMetaData, kKeySampleSize);
  samples_per_second = FindInt32(aMetaData, kKeySampleRate);
  frequency_index = Adts::GetFrequencyIndex(samples_per_second);
  aac_profile = FindInt32(aMetaData, kKeyAACProfile);

  if (FindData(aMetaData, kKeyESDS, extra_data)) {
    ESDS esds(extra_data->Elements(), extra_data->Length());

    const void* data;
    size_t size;
    if (esds.getCodecSpecificInfo(&data, &size) == OK) {
      const uint8_t* cdata = reinterpret_cast<const uint8_t*>(data);
      audio_specific_config->AppendElements(cdata, size);
      if (size > 1) {
        ABitReader br(cdata, size);
        extended_profile = br.getBits(5);

        if (extended_profile == 31) {  // AAC-ELD => additional 6 bits
          extended_profile = 32 + br.getBits(6);
        }
      }
    }
  }
}

bool
AudioDecoderConfig::IsValid()
{
  return channel_count > 0 && samples_per_second > 0 && frequency_index > 0 &&
         (!mime_type.Equals(MEDIA_MIMETYPE_AUDIO_AAC) ||
          aac_profile > 0 || extended_profile > 0);
}

void
VideoDecoderConfig::Update(const MetaData* aMetaData, const char* aMimeType)
{
  TrackConfig::Update(aMetaData, aMimeType);
  display_width = FindInt32(aMetaData, kKeyDisplayWidth);
  display_height = FindInt32(aMetaData, kKeyDisplayHeight);
  image_width = FindInt32(aMetaData, kKeyWidth);
  image_height = FindInt32(aMetaData, kKeyHeight);

  FindData(aMetaData, kKeyAVCC, extra_data);
}

bool
VideoDecoderConfig::IsValid()
{
  return display_width > 0 && display_height > 0;
}

MP4Sample::MP4Sample()
  : decode_timestamp(0)
  , composition_timestamp(0)
  , duration(0)
  , byte_offset(0)
  , is_sync_point(0)
  , data(nullptr)
  , size(0)
  , extra_data(nullptr)
{
}

MP4Sample*
MP4Sample::Clone() const
{
  nsAutoPtr<MP4Sample> s(new MP4Sample());
  s->decode_timestamp = decode_timestamp;
  s->composition_timestamp = composition_timestamp;
  s->duration = duration;
  s->byte_offset = byte_offset;
  s->is_sync_point = is_sync_point;
  s->size = size;
  s->crypto = crypto;
  s->extra_data = extra_data;
  s->extra_buffer = s->data = new (fallible) uint8_t[size];
  if (!s->extra_buffer) {
    return nullptr;
  }
  memcpy(s->data, data, size);
  return s.forget();
}

MP4Sample::~MP4Sample()
{
}

bool
MP4Sample::Pad(size_t aPaddingBytes)
{
  size_t newSize = size + aPaddingBytes;

  uint8_t* newData = new (fallible) uint8_t[newSize];
  if (!newData) {
    return false;
  }

  memset(newData + size, 0, aPaddingBytes);
  memcpy(newData, data, size);
  extra_buffer = data = newData;

  return true;
}

bool
MP4Sample::Prepend(const uint8_t* aData, size_t aSize)
{
  size_t newSize = size + aSize;

  uint8_t* newData = new (fallible) uint8_t[newSize];
  if (!newData) {
    return false;
  }

  memmove(newData + aSize, data, size);
  memmove(newData, aData, aSize);
  size = newSize;
  extra_buffer = data = newData;

  return true;
}

bool
MP4Sample::Replace(const uint8_t* aData, size_t aSize)
{
  uint8_t* newData = new (fallible) uint8_t[aSize];
  if (!newData) {
    return false;
  }

  memcpy(newData, aData, aSize);
  size = aSize;
  extra_buffer = data = newData;

  return true;
}
}
