/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DECODER_DATA_H_
#define DECODER_DATA_H_

#include "mozilla/Types.h"
#include "nsAutoPtr.h"

namespace android
{
template <typename T> class sp;
class MetaData;
class MediaBuffer;
}

namespace mp4_demuxer
{

class MP4Demuxer;
class Adts;

class AudioDecoderConfig
{
public:
  AudioDecoderConfig()
    : mime_type(nullptr)
    , duration(0)
    , channel_count(0)
    , bytes_per_sample(0)
    , samples_per_second(0)
    , extra_data(nullptr)
    , extra_data_size(0)
    , aac_profile(0)
  {
  }

  const char* mime_type;
  int64_t duration;
  uint32_t channel_count;
  uint32_t bytes_per_sample;
  uint32_t samples_per_second;
  int8_t frequency_index;

  // Decoder specific extra data which in the case of H.264 is the AVCC config
  // for decoders which require AVCC format as opposed to Annex B format.
  const void* extra_data;
  size_t extra_data_size;

  void Update(android::sp<android::MetaData>& aMetaData, const char* aMimeType);
  bool IsValid();

private:
  friend class MP4Demuxer;
  int8_t aac_profile;
};

class VideoDecoderConfig
{
public:
  VideoDecoderConfig()
    : mime_type(nullptr)
    , duration(0)
    , display_width(0)
    , display_height(0)
    , extra_data(nullptr)
    , extra_data_size(0)
  {
  }

  const char* mime_type;
  int64_t duration;
  int32_t display_width;
  int32_t display_height;
  nsAutoPtr<uint8_t> extra_data;
  size_t extra_data_size;

  void Update(android::sp<android::MetaData>& aMetaData, const char* aMimeType);
  bool IsValid();
};

class MP4Sample
{
public:
  MP4Sample();
  ~MP4Sample();
  void Update();

  android::MediaBuffer* mMediaBuffer;

  int64_t composition_timestamp;
  int64_t duration;
  int64_t byte_offset;
  bool is_sync_point;

  uint8_t* data;
  size_t size;

private:
  friend class Adts;
  nsAutoPtr<uint8_t> adts_buffer;
};
}

#endif
