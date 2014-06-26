/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/DecoderData.h"
#include "media/stagefright/MetaData.h"
#include "media/stagefright/MediaBuffer.h"
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

void
AudioDecoderConfig::Update(sp<MetaData>& aMetaData, const char* aMimeType)
{
  // aMimeType points to a string from MediaDefs.cpp so we don't need to copy it
  mime_type = aMimeType;
  duration = FindInt64(aMetaData, kKeyDuration);
  channel_count = FindInt32(aMetaData, kKeyChannelCount);
  bits_per_sample = FindInt32(aMetaData, kKeySampleSize);
  samples_per_second = FindInt32(aMetaData, kKeySampleRate);
  frequency_index = Adts::GetFrequencyIndex(samples_per_second);
  aac_profile = FindInt32(aMetaData, kKeyAACProfile);

  const void* data;
  size_t size;
  uint32_t type;

  if (aMetaData->findData(kKeyESDS, &type, &data, &size)) {
    extra_data.clear();
    extra_data.append(reinterpret_cast<const uint8_t*>(data), size);

    ESDS esds(&extra_data[0], extra_data.length());
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
         aac_profile > 0;
}

void
VideoDecoderConfig::Update(sp<MetaData>& aMetaData, const char* aMimeType)
{
  // aMimeType points to a string from MediaDefs.cpp so we don't need to copy it
  mime_type = aMimeType;
  duration = FindInt64(aMetaData, kKeyDuration);
  display_width = FindInt32(aMetaData, kKeyDisplayWidth);
  display_height = FindInt32(aMetaData, kKeyDisplayHeight);

  const void* data;
  size_t size;
  uint32_t type;

  if (aMetaData->findData(kKeyAVCC, &type, &data, &size) && size >= 7) {
    extra_data.clear();
    extra_data.append(reinterpret_cast<const uint8_t*>(data), size);
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
  composition_timestamp = FindInt64(m, kKeyTime);
  duration = FindInt64(m, kKeyDuration);
  byte_offset = FindInt64(m, kKey64BitFileOffset);
  is_sync_point = FindInt32(m, kKeyIsSyncFrame);
  data = reinterpret_cast<uint8_t*>(mMediaBuffer->data());
  size = mMediaBuffer->range_length();
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
