/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stagefright/ColorConverter.h>
#include <stagefright/DataSource.h>
#include <stagefright/MediaExtractor.h>
#include <stagefright/MetaData.h>
#include <stagefright/OMXCodec.h>
#include <media/stagefright/MediaErrors.h>
#ifdef MOZ_WIDGET_GONK
#include <OMX.h>
#else
#include <stagefright/OMXClient.h>
#endif
#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"
#include "MPAPI.h"

#include "android/log.h"

#define MAX_DECODER_NAME_LEN 256
#define AVC_MIME_TYPE "video/avc"

#if !defined(MOZ_ANDROID_FROYO)
#define DEFAULT_STAGEFRIGHT_FLAGS OMXCodec::kClientNeedsFramebuffer
#else
#define DEFAULT_STAGEFRIGHT_FLAGS 0
#endif

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "OmxPlugin" , ## args)

#if defined(MOZ_ANDROID_FROYO) || defined(MOZ_ANDROID_GB)
// Android versions 2.x.x have common API differences
#define MOZ_ANDROID_V2_X_X
#endif

#if !defined(MOZ_ANDROID_V2_X_X) && !defined(MOZ_ANDROID_HC)
#define MOZ_ANDROID_V4_OR_ABOVE
#endif

#if defined(MOZ_ANDROID_V4_OR_ABOVE)
#include <I420ColorConverter.h>
#endif

using namespace MPAPI;

#if !defined(MOZ_STAGEFRIGHT_OFF_T)
#define MOZ_STAGEFRIGHT_OFF_T off64_t
#endif

using namespace android;

namespace OmxPlugin {

const int OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka = 0x7FA30C01;
const int OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00;
const int OMX_TI_COLOR_FormatYUV420PackedSemiPlanar = 0x7F000100;

class OmxDecoder {
  PluginHost *mPluginHost;
  Decoder *mDecoder;
#ifndef MOZ_WIDGET_GONK
  OMXClient mClient;
#endif
  sp<MediaSource> mVideoTrack;
  sp<MediaSource> mVideoSource;
  sp<MediaSource> mAudioTrack;
  sp<MediaSource> mAudioSource;
  int32_t mVideoWidth;
  int32_t mVideoHeight;
  int32_t mVideoColorFormat;
  int32_t mVideoStride;
  int32_t mVideoSliceHeight;
  int32_t mVideoCropLeft;
  int32_t mVideoCropTop;
  int32_t mVideoCropRight;
  int32_t mVideoCropBottom;
  int32_t mVideoRotation;
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;
  int64_t mDurationUs;
  MediaBuffer *mVideoBuffer;
  VideoFrame mVideoFrame;
  MediaBuffer *mAudioBuffer;
  AudioFrame mAudioFrame;
  ColorConverter *mColorConverter;

  // 'true' if a read from the audio stream was done while reading the metadata
  bool mAudioMetadataRead;

  void ReleaseVideoBuffer();
  void ReleaseAudioBuffer();

  void ToVideoFrame_YUV420Planar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void ToVideoFrame_CbYCrY(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void ToVideoFrame_YUV420SemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void ToVideoFrame_YVU420SemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void ToVideoFrame_YUV420PackedSemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void ToVideoFrame_YVU420PackedSemiPlanar32m4ka(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  bool ToVideoFrame_RGB565(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback);
  bool ToVideoFrame_ColorConverter(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback);
  bool ToVideoFrame_I420ColorConverter(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback);
  bool ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback);
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
    return mVideoSource != nullptr;
  }

  bool HasAudio() {
    return mAudioSource != nullptr;
  }

  bool ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs, BufferCallback *aBufferCallback);
  bool ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs);
};

OmxDecoder::OmxDecoder(PluginHost *aPluginHost, Decoder *aDecoder) :
  mPluginHost(aPluginHost),
  mDecoder(aDecoder),
  mVideoWidth(0),
  mVideoHeight(0),
  mVideoColorFormat(0),
  mVideoStride(0),
  mVideoSliceHeight(0),
  mVideoCropLeft(0),
  mVideoCropTop(0),
  mVideoCropRight(0),
  mVideoCropBottom(0),
  mVideoRotation(0),
  mAudioChannels(-1),
  mAudioSampleRate(-1),
  mDurationUs(-1),
  mVideoBuffer(nullptr),
  mAudioBuffer(nullptr),
  mColorConverter(nullptr),
  mAudioMetadataRead(false)
{
}

OmxDecoder::~OmxDecoder()
{
  ReleaseVideoBuffer();
  ReleaseAudioBuffer();

  if (mVideoSource.get()) {
    mVideoSource->stop();
  }

  if (mAudioSource.get()) {
    mAudioSource->stop();
  }

#ifndef MOZ_ANDROID_HC
  if (mColorConverter) {
    delete mColorConverter;
  }
#endif
#ifndef MOZ_WIDGET_GONK
  mClient.disconnect();
#endif
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

#ifdef MOZ_WIDGET_GONK
static sp<IOMX> sOMX = nullptr;
static sp<IOMX> GetOMX() {
  if(sOMX.get() == nullptr) {
    sOMX = reinterpret_cast<IOMX*>(new OMX);
  }
  return sOMX;
}
#endif

static uint32_t
GetDefaultStagefrightFlags(PluginHost *aPluginHost)
{
  uint32_t flags = DEFAULT_STAGEFRIGHT_FLAGS;

#if !defined(MOZ_ANDROID_FROYO)

  char hardware[256] = "";
  aPluginHost->GetSystemInfoString("hardware", hardware, sizeof(hardware));

  if (!strcmp("qcom", hardware)) {
    // Qualcomm's OMXCodec implementation interprets this flag to mean that we
    // only want a thumbnail and therefore only need one frame. After the first
    // frame it returns EOS.
    // All other OMXCodec implementations seen so far interpret this flag
    // sanely; some do not return full framebuffers unless this flag is passed.
    flags &= ~OMXCodec::kClientNeedsFramebuffer;
  }

  LOG("Hardware %s; using default flags %#x\n", hardware, flags);

#endif

  return flags;
}

static uint32_t GetVideoCreationFlags(PluginHost* aPluginHost)
{
#ifdef MOZ_WIDGET_GONK
  // Flag value of zero means return a hardware or software decoder
  // depending on what the device supports.
  return 0;
#else
  // Check whether the user has set a pref to override our default OMXCodec
  // CreationFlags flags. This is useful for A/B testing hardware and software
  // decoders for performance and bugs. The interesting flag values are:
  //  0 = Let Stagefright choose hardware or software decoding (default)
  //  8 = Force software decoding
  // 16 = Force hardware decoding
  int32_t flags = 0;
  aPluginHost->GetIntPref("media.stagefright.omxcodec.flags", &flags);
  if (flags != 0) {
#if !defined(MOZ_ANDROID_V2_X_X)
    LOG("media.stagefright.omxcodec.flags=%d", flags);
    if ((flags & OMXCodec::kHardwareCodecsOnly) != 0) {
      LOG("FORCE HARDWARE DECODING");
    } else if ((flags & OMXCodec::kSoftwareCodecsOnly) != 0) {
      LOG("FORCE SOFTWARE DECODING");
    }
#endif
  }

  flags |= GetDefaultStagefrightFlags(aPluginHost);

  return static_cast<uint32_t>(flags);
#endif
}

enum ColorFormatSupport {
  ColorFormatNotSupported = 0,
  ColorFormatSupportOK,
  ColorFormatSupportPreferred,
};

static ColorFormatSupport
IsColorFormatSupported(OMX_COLOR_FORMATTYPE aColorFormat)
{
  switch (aColorFormat) {
    case OMX_COLOR_FormatCbYCrY:
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka:
    case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
    case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
      LOG("Colour format %#x supported natively.", aColorFormat);
      // Prefer natively supported colour formats over formats that need another
      // slow software conversion.
      return ColorFormatSupportPreferred;
    default:
      break;
  }

  // These formats are okay if we can't find a better one; Android provides a
  // software conversion to a sane colour format.
#if !defined(MOZ_ANDROID_HC)
  if (ColorConverter(aColorFormat, OMX_COLOR_Format16bitRGB565).isValid()) {
    LOG("Colour format %#x supported by Android ColorConverter.", aColorFormat);
    return ColorFormatSupportOK;
  }
#endif

#if defined(MOZ_ANDROID_V4_OR_ABOVE)
  I420ColorConverter yuvConverter;

  if (yuvConverter.isLoaded() &&
      yuvConverter.getDecoderOutputFormat() == aColorFormat) {
    LOG("Colour format %#x supported by Android I420ColorConverter.", aColorFormat);
    return ColorFormatSupportOK;
  }
#endif

  return ColorFormatNotSupported;
}

#if defined(MOZ_ANDROID_KK)
/**
 * Look for a decoder that supports a colour format that we support.
 */
static bool
FindPreferredDecoderAndColorFormat(const sp<IOMX>& aOmx,
                                   char *aDecoderName,
                                   size_t aDecoderLen,
                                   OMX_COLOR_FORMATTYPE *aColorFormat)
{
  Vector<CodecCapabilities> codecs;

  // Get all AVC decoder/colour format pairs that this device supports.
  QueryCodecs(aOmx, AVC_MIME_TYPE, true /* queryDecoders */, &codecs);

  // We assume that faster (hardware accelerated) decoders come first in the
  // list, so we choose the first decoder with a colour format we can use.
  for (uint32_t i = 0; i < codecs.size(); i++) {
    const CodecCapabilities &caps = codecs[i];
    const Vector<OMX_U32> &colors = caps.mColorFormats;

    bool found = false;
    for (uint32_t j = 0; j < colors.size(); j++) {
      OMX_COLOR_FORMATTYPE color = (OMX_COLOR_FORMATTYPE)colors[j];

      LOG("Decoder %s can output colour format %#x.\n",
          caps.mComponentName.string(), color);

      ColorFormatSupport supported = IsColorFormatSupported(color);

      if (supported) {
        strncpy(aDecoderName, caps.mComponentName.string(), aDecoderLen);
        *aColorFormat = (OMX_COLOR_FORMATTYPE)color;
        found = true;
      }

      if (supported == ColorFormatSupportPreferred) {
        // The colour format is natively supported -- that's as good as we're
        // going to get.
        break;
      }
    }

    if (found) {
      return true;
    }
  }

  return false;
}
#endif

static sp<MediaSource> CreateVideoSource(PluginHost* aPluginHost,
                                         const sp<IOMX>& aOmx,
                                         const sp<MediaSource>& aVideoTrack)
{
  uint32_t flags = GetVideoCreationFlags(aPluginHost);

  char decoderName[MAX_DECODER_NAME_LEN] = "";
  sp<MetaData> videoFormat = aVideoTrack->getFormat();

#if defined(MOZ_ANDROID_KK)
  OMX_COLOR_FORMATTYPE colorFormat = (OMX_COLOR_FORMATTYPE)0;
  if (FindPreferredDecoderAndColorFormat(aOmx,
                                         decoderName, sizeof(decoderName),
                                         &colorFormat)) {
    // We found a colour format that we can handle. Tell OMXCodec to use it in
    // case it isn't the default.
    videoFormat->setInt32(kKeyColorFormat, colorFormat);

    LOG("Found compatible decoder %s with colour format %#x.\n",
        decoderName, colorFormat);
  }
#endif

  if (flags == DEFAULT_STAGEFRIGHT_FLAGS) {
    // Let Stagefright choose hardware or software decoder.
    sp<MediaSource> videoSource = OMXCodec::Create(aOmx, videoFormat,
                                                   false, aVideoTrack,
                                                   decoderName[0] ? decoderName : nullptr,
                                                   flags);
    if (videoSource == nullptr)
      return nullptr;

    // Now that OMXCodec has parsed the video's AVCDecoderConfigurationRecord,
    // check whether we know how to decode this video.
    int32_t videoColorFormat;
    if (videoSource->getFormat()->findInt32(kKeyColorFormat, &videoColorFormat)) {

      if (IsColorFormatSupported((OMX_COLOR_FORMATTYPE)videoColorFormat)) {
        return videoSource;
      }

      // We need to implement a ToVideoFrame_*() color conversion
      // function for this video color format.
      LOG("Unknown video color format: %#x", videoColorFormat);
    } else {
      LOG("Video color format not found");
    }

    // Throw away the videoSource and try again with new flags.
    LOG("Falling back to software decoder");
    videoSource.clear();
#if defined(MOZ_ANDROID_V2_X_X)
    flags = DEFAULT_STAGEFRIGHT_FLAGS | OMXCodec::kPreferSoftwareCodecs;
#else
    flags = DEFAULT_STAGEFRIGHT_FLAGS | OMXCodec::kSoftwareCodecsOnly;
#endif
  }

  MOZ_ASSERT(flags != DEFAULT_STAGEFRIGHT_FLAGS);
  return OMXCodec::Create(aOmx, aVideoTrack->getFormat(), false, aVideoTrack,
                          nullptr, flags);
}

bool OmxDecoder::Init() {
  //register sniffers, if they are not registered in this process.
  DataSource::RegisterDefaultSniffers();

  sp<DataSource> dataSource =
    DataSource::CreateFromURI(static_cast<char*>(mDecoder->mResource));
  if (!dataSource.get() || dataSource->initCheck()) {
    return false;
  }

  sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
  if (extractor == nullptr) {
    return false;
  }

  ssize_t audioTrackIndex = -1;
  ssize_t videoTrackIndex = -1;
  const char *audioMime = nullptr;
  const char *videoMime = nullptr;

  for (size_t i = 0; i < extractor->countTracks(); ++i) {
    sp<MetaData> meta = extractor->getTrackMetaData(i);

    const char *mime;
    if (!meta->findCString(kKeyMIMEType, &mime)) {
      continue;
    }

    if (videoTrackIndex == -1 && !strncasecmp(mime, "video/", 6)) {
      videoTrackIndex = i;
      videoMime = mime;
    } else if (audioTrackIndex == -1 && !strncasecmp(mime, "audio/", 6)) {
      audioTrackIndex = i;
      audioMime = mime;
    }
  }

  if (videoTrackIndex == -1 && audioTrackIndex == -1) {
    return false;
  }

  int64_t totalDurationUs = 0;

#ifdef MOZ_WIDGET_GONK
  sp<IOMX> omx = GetOMX();
#else
  // OMXClient::connect() always returns OK and abort's fatally if
  // it can't connect. We may need to implement the connect functionality
  // ourselves if this proves to be an issue.
  if (mClient.connect() != OK) {
    LOG("OMXClient failed to connect");
  }
  sp<IOMX> omx = mClient.interface();
#endif

  sp<MediaSource> videoTrack;
  sp<MediaSource> videoSource;
  if (videoTrackIndex != -1 && (videoTrack = extractor->getTrack(videoTrackIndex)) != nullptr) {
#if defined(MOZ_ANDROID_FROYO)
    // Allow up to 720P video.
    sp<MetaData> meta = extractor->getTrackMetaData(videoTrackIndex);
    meta->setInt32(kKeyMaxInputSize, (1280 * 720 * 3) / 2);
#endif
    videoSource = CreateVideoSource(mPluginHost, omx, videoTrack);
    if (videoSource == nullptr) {
      LOG("OMXCodec failed to initialize video decoder for \"%s\"", videoMime);
      return false;
    }
    status_t status = videoSource->start();
    if (status != OK) {
      LOG("videoSource->start() failed with status %#x", status);
      return false;
    }
    int64_t durationUs;
    if (videoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs < 0)
        LOG("video duration %lld should be nonnegative", durationUs);
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  sp<MediaSource> audioTrack;
  sp<MediaSource> audioSource;
  if (audioTrackIndex != -1 && (audioTrack = extractor->getTrack(audioTrackIndex)) != nullptr)
  {
    if (!strcasecmp(audioMime, "audio/raw")) {
      audioSource = audioTrack;
    } else {
      audioSource = OMXCodec::Create(omx,
                                     audioTrack->getFormat(),
                                     false, // decoder
                                     audioTrack);
    }

    if (audioSource == nullptr) {
      LOG("OMXCodec failed to initialize audio decoder for \"%s\"", audioMime);
      return false;
    }

    status_t status = audioSource->start();
    if (status != OK) {
      LOG("audioSource->start() failed with status %#x", status);
      return false;
    }

    int64_t durationUs;
    if (audioTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs < 0)
        LOG("audio duration %lld should be nonnegative", durationUs);
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

  // To reliably get the channel and sample rate data we need to read from the
  // audio source until we get a INFO_FORMAT_CHANGE status
  if (mAudioSource.get()) {
    if (mAudioSource->read(&mAudioBuffer) != INFO_FORMAT_CHANGED) {
      sp<MetaData> meta = mAudioSource->getFormat();
      if (!meta->findInt32(kKeyChannelCount, &mAudioChannels) ||
          !meta->findInt32(kKeySampleRate, &mAudioSampleRate)) {
        return false;
      }
      mAudioMetadataRead = true;

      if (mAudioChannels < 0) {
        LOG("audio channel count %d must be nonnegative", mAudioChannels);
        return false;
      }

      if (mAudioSampleRate < 0) {
        LOG("audio sample rate %d must be nonnegative", mAudioSampleRate);
        return false;
      }
    }
    else if (!SetAudioFormat()) {
        return false;
    }
  }
  return true;
}

bool OmxDecoder::SetVideoFormat() {
  sp<MetaData> format = mVideoSource->getFormat();

  // Stagefright's kKeyWidth and kKeyHeight are what MPAPI calls stride and
  // slice height. Stagefright only seems to use its kKeyStride and
  // kKeySliceHeight to initialize camera video formats.

#if defined(DEBUG) && !defined(MOZ_ANDROID_FROYO)
  int32_t unexpected;
  if (format->findInt32(kKeyStride, &unexpected))
    LOG("Expected kKeyWidth, but found kKeyStride %d", unexpected);
  if (format->findInt32(kKeySliceHeight, &unexpected))
    LOG("Expected kKeyHeight, but found kKeySliceHeight %d", unexpected);
#endif // DEBUG

  const char *componentName;

  if (!format->findInt32(kKeyWidth, &mVideoStride) ||
      !format->findInt32(kKeyHeight, &mVideoSliceHeight) ||
      !format->findCString(kKeyDecoderComponent, &componentName) ||
      !format->findInt32(kKeyColorFormat, &mVideoColorFormat) ) {
    return false;
  }

  if (mVideoStride <= 0) {
    LOG("stride %d must be positive", mVideoStride);
    return false;
  }

  if (mVideoSliceHeight <= 0) {
    LOG("slice height %d must be positive", mVideoSliceHeight);
    return false;
  }

  // Gingerbread does not support the kKeyCropRect key
#if !defined(MOZ_ANDROID_V2_X_X)
  if (!format->findRect(kKeyCropRect, &mVideoCropLeft, &mVideoCropTop,
                                      &mVideoCropRight, &mVideoCropBottom)) {
#endif
    mVideoCropLeft = 0;
    mVideoCropTop = 0;
    mVideoCropRight = mVideoStride - 1;
    mVideoCropBottom = mVideoSliceHeight - 1;
    LOG("crop rect not available, assuming no cropping");
#if !defined(MOZ_ANDROID_V2_X_X)
  }
#endif

  if (mVideoCropLeft < 0 || mVideoCropLeft >= mVideoCropRight || mVideoCropRight >= mVideoStride ||
      mVideoCropTop < 0 || mVideoCropTop >= mVideoCropBottom || mVideoCropBottom >= mVideoSliceHeight) {
    LOG("invalid crop rect %d,%d-%d,%d", mVideoCropLeft, mVideoCropTop, mVideoCropRight, mVideoCropBottom);
    return false;
  }

  mVideoWidth = mVideoCropRight - mVideoCropLeft + 1;
  mVideoHeight = mVideoCropBottom - mVideoCropTop + 1;
  MOZ_ASSERT(mVideoWidth > 0 && mVideoWidth <= mVideoStride);
  MOZ_ASSERT(mVideoHeight > 0 && mVideoHeight <= mVideoSliceHeight);

#if !defined(MOZ_ANDROID_FROYO)
  if (!format->findInt32(kKeyRotation, &mVideoRotation)) {
#endif
    mVideoRotation = 0;
#if !defined(MOZ_ANDROID_FROYO)
    LOG("rotation not available, assuming 0");
  }
#endif

  if (mVideoRotation != 0 && mVideoRotation != 90 &&
      mVideoRotation != 180 && mVideoRotation != 270) {
    LOG("invalid rotation %d, assuming 0", mVideoRotation);
  }

  LOG("width: %d height: %d component: %s format: %#x stride: %d sliceHeight: %d rotation: %d crop: %d,%d-%d,%d",
      mVideoWidth, mVideoHeight, componentName, mVideoColorFormat,
      mVideoStride, mVideoSliceHeight, mVideoRotation,
      mVideoCropLeft, mVideoCropTop, mVideoCropRight, mVideoCropBottom);

  return true;
}

bool OmxDecoder::SetAudioFormat() {
  // If the format changed, update our cached info.
  if (!mAudioSource->getFormat()->findInt32(kKeyChannelCount, &mAudioChannels) ||
      !mAudioSource->getFormat()->findInt32(kKeySampleRate, &mAudioSampleRate)) {
    return false;
  }

  LOG("channelCount: %d sampleRate: %d", mAudioChannels, mAudioSampleRate);

  if (mAudioChannels < 0) {
    LOG("audio channel count %d must be nonnegative", mAudioChannels);
    return false;
  }

  if (mAudioSampleRate < 0) {
    LOG("audio sample rate %d must be nonnegative", mAudioSampleRate);
    return false;
  }

  return true;
}

void OmxDecoder::ReleaseVideoBuffer() {
  if (mVideoBuffer) {
    mVideoBuffer->release();
    mVideoBuffer = nullptr;
  }
}

void OmxDecoder::ReleaseAudioBuffer() {
  if (mAudioBuffer) {
    mAudioBuffer->release();
    mAudioBuffer = nullptr;
  }
}

void OmxDecoder::ToVideoFrame_YUV420Planar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *u = static_cast<uint8_t *>(y) + mVideoStride * mVideoSliceHeight;
  void *v = static_cast<uint8_t *>(u) + mVideoStride/2 * mVideoSliceHeight/2;
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              u, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0,
              v, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0);
}

void OmxDecoder::ToVideoFrame_CbYCrY(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              aData, mVideoStride, mVideoWidth, mVideoHeight, 1, 1,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 3,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 2, 3);
}

void OmxDecoder::ToVideoFrame_YUV420SemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  int32_t videoStride = mVideoStride;
  int32_t videoSliceHeight = mVideoSliceHeight;

  // OMX.SEC.avcdec rounds mVideoStride and mVideoSliceHeight up to the nearest
  // multiple of 16 but the data itself is too small to fit. What we do is check
  // to see if the video size patches the raw width and height. If so we can
  // use those figures instead.

  if (aSize == mVideoWidth * mVideoHeight * 3 / 2) {
    videoStride = mVideoWidth;
    videoSliceHeight = mVideoHeight;
  }

  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + (videoStride * videoSliceHeight);
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, videoStride, videoSliceHeight, mVideoRotation,
              y, videoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, videoStride, mVideoWidth/2, mVideoHeight/2, 0, 1,
              uv, videoStride, mVideoWidth/2, mVideoHeight/2, 1, 1);
}

void OmxDecoder::ToVideoFrame_YVU420SemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  ToVideoFrame_YUV420SemiPlanar(aFrame, aTimeUs, aData, aSize, aKeyFrame);
  aFrame->Cb.mOffset = 1;
  aFrame->Cr.mOffset = 0;
}

void OmxDecoder::ToVideoFrame_YUV420PackedSemiPlanar(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + mVideoStride * (mVideoSliceHeight - mVideoCropTop/2);
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 1,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 1, 1);
}

void OmxDecoder::ToVideoFrame_YVU420PackedSemiPlanar32m4ka(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame) {
  size_t roundedSliceHeight = (mVideoSliceHeight + 31) & ~31;
  size_t roundedStride = (mVideoStride + 31) & ~31;
  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + (roundedStride * roundedSliceHeight);
  aFrame->Set(aTimeUs, aKeyFrame,
              aData, aSize, mVideoStride, mVideoSliceHeight, mVideoRotation,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 1, 1,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 1);
}

bool OmxDecoder::ToVideoFrame_RGB565(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback) {
  void *buffer = (*aBufferCallback)(mVideoWidth, mVideoHeight, MPAPI::RGB565);

  if (!buffer) {
    return false;
  }

  aFrame->mTimeUs = aTimeUs;

  memcpy(buffer, aData, mVideoWidth * mVideoHeight * 2);

  aFrame->mSize = mVideoWidth * mVideoHeight * 2;

  return true;
}

bool OmxDecoder::ToVideoFrame_ColorConverter(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback) {
#ifdef MOZ_ANDROID_HC
  return false;
#else
  if (!mColorConverter) {
    mColorConverter = new ColorConverter((OMX_COLOR_FORMATTYPE)mVideoColorFormat,
                                         OMX_COLOR_Format16bitRGB565);
  }

  if (!mColorConverter->isValid()) {
    return false;
  }

  aFrame->mTimeUs = aTimeUs;

  void *buffer = (*aBufferCallback)(mVideoWidth, mVideoHeight, MPAPI::RGB565);

  if (!buffer) {
    return false;
  }

  aFrame->mSize = mVideoWidth * mVideoHeight * 2;

#if defined(MOZ_ANDROID_V2_X_X)
  mColorConverter->convert(mVideoWidth, mVideoHeight,
                           aData, 0 /* srcSkip */,
                           buffer, mVideoWidth * 2);
#else
  mColorConverter->convert(aData, mVideoStride, mVideoSliceHeight,
                           mVideoCropLeft, mVideoCropTop,
                           mVideoCropLeft + mVideoWidth - 1,
                           mVideoCropTop + mVideoHeight - 1,
                           buffer, mVideoWidth, mVideoHeight,
                           0, 0, mVideoWidth - 1, mVideoHeight - 1);
#endif

  return true;
#endif
}

bool OmxDecoder::ToVideoFrame_I420ColorConverter(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback)
{
#if defined(MOZ_ANDROID_V4_OR_ABOVE)
  I420ColorConverter yuvConverter;

  if (!yuvConverter.isLoaded()) {
    return false;
  }

  if (yuvConverter.getDecoderOutputFormat() != mVideoColorFormat) {
    return false;
  }

  void *buffer = (*aBufferCallback)(mVideoWidth, mVideoHeight, MPAPI::I420);

  ARect crop = { mVideoCropLeft, mVideoCropTop, mVideoCropRight, mVideoCropBottom };
  int result = yuvConverter.convertDecoderOutputToI420(aData,
                                                       mVideoWidth,
                                                       mVideoHeight,
                                                       crop,
                                                       buffer);

  // result is 0 on success, -1 otherwise.
  if (result == OK) {
    aFrame->mTimeUs = aTimeUs;
    aFrame->mSize = mVideoWidth * mVideoHeight * 3 / 2;
  }

  return result == OK;
#else
  return false;
#endif
}

bool OmxDecoder::ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, BufferCallback *aBufferCallback) {
  switch (mVideoColorFormat) {
// Froyo support is best handled with the android color conversion code. I
// get corrupted videos when using our own routines below.
#if !defined(MOZ_ANDROID_FROYO)
  case OMX_COLOR_FormatYUV420Planar: // e.g. Asus Transformer, Stagefright's software decoder
    ToVideoFrame_YUV420Planar(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatCbYCrY: // e.g. Droid 1
    ToVideoFrame_CbYCrY(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_FormatYUV420SemiPlanar: // e.g. Galaxy S III
    ToVideoFrame_YUV420SemiPlanar(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_QCOM_COLOR_FormatYVU420SemiPlanar: // e.g. Nexus One
    ToVideoFrame_YVU420SemiPlanar(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka: // e.g. Otoro
    ToVideoFrame_YVU420PackedSemiPlanar32m4ka(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar: // e.g. Galaxy Nexus
    ToVideoFrame_YUV420PackedSemiPlanar(aFrame, aTimeUs, aData, aSize, aKeyFrame);
    break;
  case OMX_COLOR_Format16bitRGB565:
    return ToVideoFrame_RGB565(aFrame, aTimeUs, aData, aSize, aKeyFrame, aBufferCallback);
    break;
#endif
  default:
    if (!ToVideoFrame_ColorConverter(aFrame, aTimeUs, aData, aSize, aKeyFrame, aBufferCallback) &&
        !ToVideoFrame_I420ColorConverter(aFrame, aTimeUs, aData, aSize, aKeyFrame, aBufferCallback)) {
      LOG("Unknown video color format: %#x", mVideoColorFormat);
      return false;
    }
  }
  return true;
}

bool OmxDecoder::ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize, int32_t aAudioChannels, int32_t aAudioSampleRate)
{
  aFrame->Set(aTimeUs, reinterpret_cast<char *>(aData) + aDataOffset, aSize, aAudioChannels, aAudioSampleRate);
  return true;
}

class ReadOptions : public MediaSource::ReadOptions
{
  // HTC have their own version of ReadOptions with extra fields. If we don't
  // have this here, HTCOMXCodec will corrupt our stack.
  uint32_t sadface[4];
};

bool OmxDecoder::ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs,
                           BufferCallback *aBufferCallback)
{
  MOZ_ASSERT(aSeekTimeUs >= -1);

  if (!mVideoSource.get())
    return false;

  ReleaseVideoBuffer();

  status_t err;

  if (aSeekTimeUs != -1) {
    ReadOptions options;
    options.setSeekTo(aSeekTimeUs);
    err = mVideoSource->read(&mVideoBuffer, &options);
  } else {
    err = mVideoSource->read(&mVideoBuffer);
  }

  aFrame->mSize = 0;

  if (err == OK && mVideoBuffer->range_length() > 0) {
    int64_t timeUs;
    int32_t keyFrame;

    if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs) ) {
      LOG("no frame time");
      return false;
    }

    if (timeUs < 0) {
      LOG("frame time %lld must be nonnegative", timeUs);
      return false;
    }

    if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
       keyFrame = 0;
    }

    char *data = reinterpret_cast<char *>(mVideoBuffer->data()) + mVideoBuffer->range_offset();
    size_t length = mVideoBuffer->range_length();

    if (!ToVideoFrame(aFrame, timeUs, data, length, keyFrame, aBufferCallback)) {
      return false;
    }
  }
  else if (err == INFO_FORMAT_CHANGED) {
    // If the format changed, update our cached info.
    LOG("mVideoSource INFO_FORMAT_CHANGED");
    if (!SetVideoFormat())
      return false;
    else
      return ReadVideo(aFrame, aSeekTimeUs, aBufferCallback);
  }
  else if (err == ERROR_END_OF_STREAM) {
    LOG("mVideoSource END_OF_STREAM");
  }
  else if (err != OK) {
    LOG("mVideoSource ERROR %#x", err);
  }

  return err == OK;
}

bool OmxDecoder::ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  MOZ_ASSERT(aSeekTimeUs >= -1);

  status_t err;
  if (mAudioMetadataRead && aSeekTimeUs == -1) {
    // Use the data read into the buffer during metadata time
    err = OK;
  }
  else {
    ReleaseAudioBuffer();
    if (aSeekTimeUs != -1) {
      ReadOptions options;
      options.setSeekTo(aSeekTimeUs);
      err = mAudioSource->read(&mAudioBuffer, &options);
    } else {
      err = mAudioSource->read(&mAudioBuffer);
    }
  }
  mAudioMetadataRead = false;

  aSeekTimeUs = -1;

  if (err == OK && mAudioBuffer->range_length() != 0) {
    int64_t timeUs;
    if (!mAudioBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
      LOG("no frame time");
      return false;
    }

    if (timeUs < 0) {
      LOG("frame time %lld must be nonnegative", timeUs);
      return false;
    }

    return ToAudioFrame(aFrame, timeUs,
                        mAudioBuffer->data(),
                        mAudioBuffer->range_offset(),
                        mAudioBuffer->range_length(),
                        mAudioChannels, mAudioSampleRate);
  }
  else if (err == INFO_FORMAT_CHANGED) {
    // If the format changed, update our cached info.
    LOG("mAudioSource INFO_FORMAT_CHANGED");
    if (!SetAudioFormat())
      return false;
    else
      return ReadAudio(aFrame, aSeekTimeUs);
  }
  else if (err == ERROR_END_OF_STREAM) {
    LOG("mAudioSource END_OF_STREAM");
  }
  else if (err != OK) {
    LOG("mAudioSource ERROR %#x", err);
  }

  return err == OK;
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

static bool ReadVideo(Decoder *aDecoder, VideoFrame *aFrame, int64_t aSeekTimeUs, BufferCallback *aBufferCallback)
{
  return cast(aDecoder)->ReadVideo(aFrame, aSeekTimeUs, aBufferCallback);
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
  "avc1.42E01E",  // H.264 Constrained Baseline Profile Level 3.0
  "avc1.42001E",  // H.264 Baseline Profile Level 3.0
  "avc1.42001F",  // H.264 Baseline Profile Level 3.1
  "avc1.4D401E",  // H.264 Main Profile Level 3.0
  "avc1.4D401F",  // H.264 Main Profile Level 3.1
  "mp4a.40.2",    // AAC-LC
  nullptr
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
  if (!omx || !omx->Init()) {
    if (omx)
      delete omx;
    return false;
  }

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

} // namespace OmxPlugin

// Export the manifest so MPAPI can find our entry points.
Manifest MOZ_EXPORT MPAPI_MANIFEST = {
  OmxPlugin::CanDecode,
  OmxPlugin::CreateDecoder
};
