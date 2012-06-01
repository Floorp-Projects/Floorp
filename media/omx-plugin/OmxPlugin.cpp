/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <unistd.h>
#include <fcntl.h>

#include <DataSource.h>
#include <MediaErrors.h>
#include <MediaExtractor.h>
#include <MediaSource.h>
#include <MetaData.h>
#include <OMXCodec.h>
#include <OMX.h>
#include <HardwareAPI.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>
#include <ui/Region.h>
#include <binder/IMemory.h>

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_IVCommon.h>
#include <OMX_Component.h>

#include "mozilla/Types.h"
#include "MPAPI.h"

#include "android/log.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "OmxPlugin" , ## args)

using namespace MPAPI;

namespace android {

// MediaStreamSource is a DataSource that reads from a MPAPI media stream.

class MediaStreamSource : public DataSource {
  PluginHost *mPluginHost;
public:
  MediaStreamSource(PluginHost *aPluginHost, Decoder *aDecoder);

  virtual status_t initCheck() const;
  virtual ssize_t readAt(off64_t offset, void *data, size_t size);
  virtual ssize_t readAt(off_t offset, void *data, size_t size) {
    return readAt(static_cast<off64_t>(offset), data, size);
  }
  virtual status_t getSize(off_t *size) {
    off64_t size64;
    status_t status = getSize(&size64);
    *size = size64;
    return status;
  }
  virtual status_t getSize(off64_t *size);
  virtual uint32_t flags() {
    return kWantsPrefetching;
  }

  virtual ~MediaStreamSource();

private:
  Decoder *mDecoder;

  MediaStreamSource(const MediaStreamSource &);
  MediaStreamSource &operator=(const MediaStreamSource &);
};

MediaStreamSource::MediaStreamSource(PluginHost *aPluginHost, Decoder *aDecoder) :
  mPluginHost(aPluginHost)
{
  mDecoder = aDecoder;
}

MediaStreamSource::~MediaStreamSource()
{
}

status_t MediaStreamSource::initCheck() const
{
  return OK;
}

ssize_t MediaStreamSource::readAt(off64_t offset, void *data, size_t size)
{
  char *ptr = reinterpret_cast<char *>(data);
  size_t todo = size;
  while (todo > 0) {
    uint32_t bytesRead;
    if (!mPluginHost->Read(mDecoder, ptr, offset, todo, &bytesRead)) {
      return ERROR_IO;
    }
    offset += bytesRead;
    todo -= bytesRead;
    ptr += bytesRead;
  }
  return size;
}

status_t MediaStreamSource::getSize(off64_t *size)
{
  uint64_t length = mPluginHost->GetLength(mDecoder);
  if (length == static_cast<uint64_t>(-1))
    return ERROR_UNSUPPORTED;

  *size = length;

  return OK;
}

}  // namespace android

using namespace android;

class OmxDecoder {
  PluginHost *mPluginHost;
  Decoder *mDecoder;
  sp<MediaSource> mVideoTrack;
  sp<MediaSource> mVideoSource;
  sp<MediaSource> mAudioTrack;
  sp<MediaSource> mAudioSource;
  int32_t mVideoWidth;
  int32_t mVideoHeight;
  int32_t mVideoColorFormat;
  int32_t mVideoStride;
  int32_t mVideoSliceHeight;
  int32_t mVideoRotation;
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;
  int64_t mDurationUs;
  MediaBuffer *mVideoBuffer;
  VideoFrame mVideoFrame;
  MediaBuffer *mAudioBuffer;
  AudioFrame mAudioFrame;

  void ReleaseVideoBuffer();
  void ReleaseAudioBuffer();

  void PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  bool ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  bool ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize,
                    int32_t aAudioChannels, int32_t aAudioSampleRate);
public:
  OmxDecoder(PluginHost *aPluginHost, Decoder *aDecoder);
  ~OmxDecoder();

  bool Init();
  bool SetVideoFormat();
  bool SetAudioFormat();

  void GetDuration(int64_t *durationUs) {
    *durationUs = mDurationUs;
  }

  void GetVideoParameters(int32_t *width, int32_t *height) {
    *width = mVideoWidth;
    *height = mVideoHeight;
  }

  void GetAudioParameters(int32_t *numChannels, int32_t *sampleRate) {
    *numChannels = mAudioChannels;
    *sampleRate = mAudioSampleRate;
  }

  bool HasVideo() {
    return mVideoSource != NULL;
  }

  bool HasAudio() {
    return mAudioSource != NULL;
  }

  bool ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs);
  bool ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs);
};

OmxDecoder::OmxDecoder(PluginHost *aPluginHost, Decoder *aDecoder) :
  mPluginHost(aPluginHost), mDecoder(aDecoder)
{
  mVideoWidth = mVideoHeight = mVideoColorFormat = mVideoStride = mVideoSliceHeight = mVideoRotation = 0;
  mVideoBuffer = NULL;
  mAudioBuffer = NULL;
}

OmxDecoder::~OmxDecoder()
{
  ReleaseVideoBuffer();
  ReleaseAudioBuffer();
}

class AutoStopMediaSource {
  sp<MediaSource> mMediaSource;
public:
  AutoStopMediaSource(sp<MediaSource> aMediaSource) : mMediaSource(aMediaSource) {
  }

  ~AutoStopMediaSource() {
    mMediaSource->stop();
  }
};

static sp<IOMX> sOMX = NULL;
static sp<IOMX> GetOMX() {
  if(sOMX.get() == NULL) {
    sOMX = new OMX;
    }
  return sOMX;
}

bool OmxDecoder::Init() {
  //register sniffers, if they are not registered in this process.
  DataSource::RegisterDefaultSniffers();

  sp<DataSource> dataSource = new MediaStreamSource(mPluginHost, mDecoder);
  if (dataSource->initCheck()) {
    return false;
  }

  mPluginHost->SetMetaDataReadMode(mDecoder);

  sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
  if (extractor == NULL) {
    return false;
  }

  sp<MediaSource> videoTrack;
  sp<MediaSource> audioTrack;
  const char *audioMime = NULL;
  bool audioMetaFound = false;

  for (size_t i = 0; i < extractor->countTracks(); ++i) {
    sp<MetaData> meta = extractor->getTrackMetaData(i);

    int32_t bitRate;
    if (!meta->findInt32(kKeyBitRate, &bitRate))
      bitRate = 0;

    const char *mime;
    if (!meta->findCString(kKeyMIMEType, &mime)) {
      continue;
    }

    if (videoTrack == NULL && !strncasecmp(mime, "video/", 6)) {
      videoTrack = extractor->getTrack(i);
    } else if (audioTrack == NULL && !strncasecmp(mime, "audio/", 6)) {
      audioTrack = extractor->getTrack(i);
      audioMime = mime;
      if (!meta->findInt32(kKeyChannelCount, &mAudioChannels) ||
          !meta->findInt32(kKeySampleRate, &mAudioSampleRate)) {
        return false;
      }
      audioMetaFound = true;
      LOG("channelCount: %d sampleRate: %d",
           mAudioChannels, mAudioSampleRate);
    }
  }

  if (videoTrack == NULL && audioTrack == NULL) {
    return false;
  }

  mPluginHost->SetPlaybackReadMode(mDecoder);

  int64_t totalDurationUs = 0;

  sp<MediaSource> videoSource;
  if (videoTrack != NULL) {
    videoSource = OMXCodec::Create(GetOMX(),
                                   videoTrack->getFormat(),
                                   false, // decoder
                                   videoTrack,
                                   NULL,
                                   0); // flags (prefer hw codecs)
    if (videoSource == NULL) {
      return false;
    }

    if (videoSource->start() != OK) {
      return false;
    }

    int64_t durationUs;
    if (videoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  sp<MediaSource> audioSource;
  if (audioTrack != NULL) {
    if (!strcasecmp(audioMime, "audio/raw")) {
      audioSource = audioTrack;
    } else {
      audioSource = OMXCodec::Create(GetOMX(),
                                     audioTrack->getFormat(),
                                     false, // decoder
                                     audioTrack);
    }
    if (audioSource == NULL) {
      return false;
    }
    if (audioSource->start() != OK) {
      return false;
    }

    int64_t durationUs;
    if (audioTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  // set decoder state
  mVideoTrack = videoTrack;
  mVideoSource = videoSource;
  mAudioTrack = audioTrack;
  mAudioSource = audioSource;
  mDurationUs = totalDurationUs;

  if (mVideoSource.get() && !SetVideoFormat())
    return false;

  if (!audioMetaFound && mAudioSource.get() && !SetAudioFormat())
    return false;

  return true;
}

bool OmxDecoder::SetVideoFormat() {
  const char *componentName;

  if (!mVideoSource->getFormat()->findInt32(kKeyWidth, &mVideoWidth) ||
      !mVideoSource->getFormat()->findInt32(kKeyHeight, &mVideoHeight) ||
      !mVideoSource->getFormat()->findCString(kKeyDecoderComponent, &componentName) ||
      !mVideoSource->getFormat()->findInt32(kKeyColorFormat, &mVideoColorFormat) ) {
    return false;
  }

  if (!mVideoSource->getFormat()->findInt32(kKeyStride, &mVideoStride)) {
    mVideoStride = mVideoWidth;
    LOG("stride not available, assuming width");
  }

  if (!mVideoSource->getFormat()->findInt32(kKeySliceHeight, &mVideoSliceHeight)) {
    mVideoSliceHeight = mVideoHeight;
    LOG("slice height not available, assuming height");
  }

  if (!mVideoSource->getFormat()->findInt32(kKeyRotation, &mVideoRotation)) {
    mVideoRotation = 0;
    LOG("rotation not available, assuming 0");
  }

  LOG("width: %d height: %d component: %s format: %d stride: %d sliceHeight: %d rotation: %d",
      mVideoWidth, mVideoHeight, componentName, mVideoColorFormat,
      mVideoStride, mVideoSliceHeight, mVideoRotation);

  return true;
}

bool OmxDecoder::SetAudioFormat() {
  // If the format changed, update our cached info.
  if (!mAudioSource->getFormat()->findInt32(kKeyChannelCount, &mAudioChannels) ||
      !mAudioSource->getFormat()->findInt32(kKeySampleRate, &mAudioSampleRate)) {
    return false;
  }

  LOG("channelCount: %d sampleRate: %d",
      mAudioChannels, mAudioSampleRate);

  return true;
}

void OmxDecoder::ReleaseVideoBuffer() {
  if (mVideoBuffer) {
    mVideoBuffer->release();
    mVideoBuffer = NULL;
  }
}

void OmxDecoder::ReleaseAudioBuffer() {
  if (mAudioBuffer) {
    mAudioBuffer->release();
    mAudioBuffer = NULL;
  }
}

void OmxDecoder::PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *u = static_cast<uint8_t *>(y) + mVideoStride * mVideoSliceHeight;
  void *v = static_cast<uint8_t *>(u) + mVideoStride/2 * mVideoSliceHeight/2;

  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              u, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0,
              v, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0);
}

void OmxDecoder::CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              aData, mVideoStride, mVideoWidth, mVideoHeight, 1, 1,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 3,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 2, 3);
}

void OmxDecoder::SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + (mVideoStride * mVideoSliceHeight);

  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 1,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 1, 1);
}

void OmxDecoder::SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
  aFrame->Cb.mOffset = 1;
  aFrame->Cr.mOffset = 0;
}

bool OmxDecoder::ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  const int OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00;

  switch (mVideoColorFormat) {
  case OMX_COLOR_FormatYUV420Planar:
    PlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatCbYCrY:
    CbYCrYFrame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatYUV420SemiPlanar:
    SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
    SemiPlanarYVU420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  default:
    return false;
  }
  return true;
}

bool OmxDecoder::ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize, int32_t aAudioChannels, int32_t aAudioSampleRate)
{
  aFrame->Set(aTimeUs, reinterpret_cast<char *>(aData) + aDataOffset, aSize, aAudioChannels, aAudioSampleRate);
  return true;
}

bool OmxDecoder::ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs)
{
  if (!mVideoSource.get())
    return false;

  for (;;) {
    ReleaseVideoBuffer();

    status_t err;

    if (aSeekTimeUs != -1) {
      MediaSource::ReadOptions options;
      options.setSeekTo(aSeekTimeUs);
      err = mVideoSource->read(&mVideoBuffer, &options);
    } else {
      err = mVideoSource->read(&mVideoBuffer);
    }

    aSeekTimeUs = -1;

    if (err == OK) {
      if (mVideoBuffer->range_length() == 0) // If we get a spurious empty buffer, keep going
        continue;

      int64_t timeUs;
      int32_t unreadable;
      int32_t keyFrame;

      if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs) ) {
        LOG("no key time");
        return false;
      }

      if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
        keyFrame = 0;
      }

      if (!mVideoBuffer->meta_data()->findInt32(kKeyIsUnreadable, &unreadable)) {
        unreadable = 0;
      }

      LOG("data: %p size: %u offset: %u length: %u unreadable: %d",
          mVideoBuffer->data(), 
          mVideoBuffer->size(),
          mVideoBuffer->range_offset(),
          mVideoBuffer->range_length(),
          unreadable);

      char *data = reinterpret_cast<char *>(mVideoBuffer->data()) + mVideoBuffer->range_offset();
      size_t length = mVideoBuffer->range_length();

      if (unreadable) {
        LOG("video frame is unreadable");
      }

      if (!ToVideoFrame(aFrame, timeUs, data, length, keyFrame)) {
        return false;
      }

      return true;
    }

    if (err == INFO_FORMAT_CHANGED) {
      // If the format changed, update our cached info.
      if (!SetVideoFormat()) {
        return false;
      }

      // Ok, try to read a buffer again.
      continue;
    }

    /* err == ERROR_END_OF_STREAM */
    break;
  }

  return false;
}

bool OmxDecoder::ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  ReleaseAudioBuffer();

  status_t err;

  if (aSeekTimeUs != -1) {
    MediaSource::ReadOptions options;
    options.setSeekTo(aSeekTimeUs);
    err = mAudioSource->read(&mAudioBuffer, &options);
  } else {
    err = mAudioSource->read(&mAudioBuffer);
  }

  aSeekTimeUs = -1;

  if (err == OK && mAudioBuffer->range_length() != 0) {
    int64_t timeUs;
    if (!mAudioBuffer->meta_data()->findInt64(kKeyTime, &timeUs))
      return false;

    return ToAudioFrame(aFrame, timeUs,
                        mAudioBuffer->data(),
                        mAudioBuffer->range_offset(),
                        mAudioBuffer->range_length(),
                        mAudioChannels, mAudioSampleRate);
  }
  else if (err == INFO_FORMAT_CHANGED && !SetAudioFormat()) {
    // If the format changed, update our cached info.
    return false;
  }
  else if (err == ERROR_END_OF_STREAM)
    return false;

  return true;
}

static OmxDecoder *cast(Decoder *decoder) {
  return reinterpret_cast<OmxDecoder *>(decoder->mPrivate);
}

static void GetDuration(Decoder *aDecoder, int64_t *durationUs) {
  cast(aDecoder)->GetDuration(durationUs);
}

static void GetVideoParameters(Decoder *aDecoder, int32_t *width, int32_t *height) {
  cast(aDecoder)->GetVideoParameters(width, height);
}

static void GetAudioParameters(Decoder *aDecoder, int32_t *numChannels, int32_t *sampleRate) {
  cast(aDecoder)->GetAudioParameters(numChannels, sampleRate);
}

static bool HasVideo(Decoder *aDecoder) {
  return cast(aDecoder)->HasVideo();
}

static bool HasAudio(Decoder *aDecoder) {
  return cast(aDecoder)->HasAudio();
}

static bool ReadVideo(Decoder *aDecoder, VideoFrame *aFrame, int64_t aSeekTimeUs)
{
  return cast(aDecoder)->ReadVideo(aFrame, aSeekTimeUs);
}

static bool ReadAudio(Decoder *aDecoder, AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  return cast(aDecoder)->ReadAudio(aFrame, aSeekTimeUs);
}

static void DestroyDecoder(Decoder *aDecoder)
{
  if (aDecoder->mPrivate)
    delete reinterpret_cast<OmxDecoder *>(aDecoder->mPrivate);
}

static bool Match(const char *aMimeChars, size_t aMimeLen, const char *aNeedle)
{
  return !strncmp(aMimeChars, aNeedle, aMimeLen);
}

static const char* const gCodecs[] = {
  "avc",
  "mp3",
  "mp4v",
  "mp4a",
  NULL
};

static bool CanDecode(const char *aMimeChars, size_t aMimeLen, const char* const**aCodecs)
{
  if (!Match(aMimeChars, aMimeLen, "video/mp4") &&
      !Match(aMimeChars, aMimeLen, "audio/mp4") &&
      !Match(aMimeChars, aMimeLen, "audio/mpeg") &&
      !Match(aMimeChars, aMimeLen, "application/octet-stream")) { // file urls
    return false;
  }
  *aCodecs = gCodecs;

  return true;
}

static bool CreateDecoder(PluginHost *aPluginHost, Decoder *aDecoder, const char *aMimeChars, size_t aMimeLen)
{
  OmxDecoder *omx = new OmxDecoder(aPluginHost, aDecoder);
  if (!omx || !omx->Init())
    return false;

  aDecoder->mPrivate = omx;
  aDecoder->GetDuration = GetDuration;
  aDecoder->GetVideoParameters = GetVideoParameters;
  aDecoder->GetAudioParameters = GetAudioParameters;
  aDecoder->HasVideo = HasVideo;
  aDecoder->HasAudio = HasAudio;
  aDecoder->ReadVideo = ReadVideo;
  aDecoder->ReadAudio = ReadAudio;
  aDecoder->DestroyDecoder = DestroyDecoder;

  return true;
}

// Export the manifest so MPAPI can find our entry points.
Manifest MOZ_EXPORT_DATA(MPAPI_MANIFEST) {
  CanDecode,
  CreateDecoder
};
