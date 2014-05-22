/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DECODER_DATA_H_
#define DECODER_DATA_H_

#include "mozilla/Types.h"
#include "mozilla/Vector.h"
#include "nsAutoPtr.h"

namespace stagefright
{
template <typename T> class sp;
class MetaData;
class MediaBuffer;
}

namespace mp4_demuxer
{

class MP4Demuxer;

class AudioDecoderConfig
{
public:
  AudioDecoderConfig()
    : mime_type(nullptr)
    , duration(0)
    , channel_count(0)
    , bits_per_sample(0)
    , samples_per_second(0)
    , aac_profile(0)
  {
  }

  const char* mime_type;
  int64_t duration;
  uint32_t channel_count;
  uint32_t bits_per_sample;
  uint32_t samples_per_second;
  int8_t frequency_index;
  mozilla::Vector<uint8_t> extra_data;
  mozilla::Vector<uint8_t> audio_specific_config;

  void Update(stagefright::sp<stagefright::MetaData>& aMetaData, const char* aMimeType);
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
  {
  }

  const char* mime_type;
  int64_t duration;
  int32_t display_width;
  int32_t display_height;

  mozilla::Vector<uint8_t> annex_b;

  void Update(stagefright::sp<stagefright::MetaData>& aMetaData, const char* aMimeType);
  bool IsValid();
};

class MP4Sample
{
public:
  MP4Sample();
  ~MP4Sample();
  void Update();

  stagefright::MediaBuffer* mMediaBuffer;

  int64_t composition_timestamp;
  int64_t duration;
  int64_t byte_offset;
  bool is_sync_point;

  uint8_t* data;
  size_t size;

  void Prepend(const uint8_t* aData, size_t aSize);

private:
  nsAutoPtr<uint8_t> extra_buffer;
};
}

#endif
