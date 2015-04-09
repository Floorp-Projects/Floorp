/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DECODER_DATA_H_
#define DECODER_DATA_H_

#include "MediaData.h"
#include "mozilla/Types.h"
#include "mozilla/Vector.h"
#include "nsRefPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsString.h"

namespace stagefright
{
class MetaData;
}

namespace mp4_demuxer
{

class MP4Demuxer;

struct PsshInfo
{
  PsshInfo() {}
  PsshInfo(const PsshInfo& aOther) : uuid(aOther.uuid), data(aOther.data) {}
  nsTArray<uint8_t> uuid;
  nsTArray<uint8_t> data;
};

class CryptoFile
{
public:
  CryptoFile() : valid(false) {}
  CryptoFile(const CryptoFile& aCryptoFile) : valid(aCryptoFile.valid)
  {
    pssh.AppendElements(aCryptoFile.pssh);
  }

  void Update(const uint8_t* aData, size_t aLength)
  {
    valid = DoUpdate(aData, aLength);
  }

  bool valid;
  nsTArray<PsshInfo> pssh;

private:
  bool DoUpdate(const uint8_t* aData, size_t aLength);
};

class TrackConfig
{
public:
  enum TrackType {
    kUndefinedTrack,
    kAudioTrack,
    kVideoTrack,
  };
  explicit TrackConfig(TrackType aType)
    : mTrackId(0)
    , duration(0)
    , media_time(0)
    , mType(aType)
  {
  }

  nsAutoCString mime_type;
  uint32_t mTrackId;
  int64_t duration;
  int64_t media_time;
  mozilla::CryptoTrack crypto;
  TrackType mType;

  bool IsAudioConfig() const
  {
    return mType == kAudioTrack;
  }
  bool IsVideoConfig() const
  {
    return mType == kVideoTrack;
  }
  void Update(const stagefright::MetaData* aMetaData,
              const char* aMimeType);
};

class AudioDecoderConfig : public TrackConfig
{
public:
  AudioDecoderConfig()
    : TrackConfig(kAudioTrack)
    , channel_count(0)
    , bits_per_sample(0)
    , samples_per_second(0)
    , frequency_index(0)
    , aac_profile(0)
    , extended_profile(0)
    , extra_data(new mozilla::DataBuffer)
    , audio_specific_config(new mozilla::DataBuffer)
  {
  }

  uint32_t channel_count;
  uint32_t bits_per_sample;
  uint32_t samples_per_second;
  int8_t frequency_index;
  int8_t aac_profile;
  int8_t extended_profile;
  nsRefPtr<mozilla::DataBuffer> extra_data;
  nsRefPtr<mozilla::DataBuffer> audio_specific_config;

  void Update(const stagefright::MetaData* aMetaData,
              const char* aMimeType);
  bool IsValid();

private:
  friend class MP4Demuxer;
};

class VideoDecoderConfig : public TrackConfig
{
public:
  VideoDecoderConfig()
    : TrackConfig(kVideoTrack)
    , display_width(0)
    , display_height(0)
    , image_width(0)
    , image_height(0)
    , extra_data(new mozilla::DataBuffer)
  {
  }

  int32_t display_width;
  int32_t display_height;

  int32_t image_width;
  int32_t image_height;

  nsRefPtr<mozilla::DataBuffer> extra_data;   // Unparsed AVCDecoderConfig payload.

  void Update(const stagefright::MetaData* aMetaData,
              const char* aMimeType);
  bool IsValid();
};

typedef int64_t Microseconds;
}

#endif
