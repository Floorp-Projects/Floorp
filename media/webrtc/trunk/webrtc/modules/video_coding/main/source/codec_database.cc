/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/codec_database.h"

#include <assert.h>

#include "webrtc/base/checks.h"
#include "webrtc/engine_configurations.h"
#ifdef VIDEOCODEC_I420
#include "webrtc/modules/video_coding/codecs/i420/main/interface/i420.h"
#endif
#ifdef VIDEOCODEC_VP8
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#endif
#ifdef VIDEOCODEC_VP9
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#endif
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace {
const size_t kDefaultPayloadSize = 1440;
}

namespace webrtc {

VideoCodecVP8 VideoEncoder::GetDefaultVp8Settings() {
  VideoCodecVP8 vp8_settings;
  memset(&vp8_settings, 0, sizeof(vp8_settings));

  vp8_settings.resilience = kResilientStream;
  vp8_settings.numberOfTemporalLayers = 1;
  vp8_settings.denoisingOn = true;
  vp8_settings.errorConcealmentOn = false;
  vp8_settings.automaticResizeOn = false;
  vp8_settings.frameDroppingOn = true;
  vp8_settings.keyFrameInterval = 3000;

  return vp8_settings;
}

VideoCodecVP9 VideoEncoder::GetDefaultVp9Settings() {
  VideoCodecVP9 vp9_settings;
  memset(&vp9_settings, 0, sizeof(vp9_settings));

  vp9_settings.resilience = 1;
  vp9_settings.numberOfTemporalLayers = 1;
  vp9_settings.denoisingOn = false;
  vp9_settings.frameDroppingOn = true;
  vp9_settings.keyFrameInterval = 3000;
  vp9_settings.adaptiveQpMode = true;

  return vp9_settings;
}

VideoCodecH264 VideoEncoder::GetDefaultH264Settings() {
  VideoCodecH264 h264_settings;
  memset(&h264_settings, 0, sizeof(h264_settings));

  h264_settings.profile = kProfileBase;
  h264_settings.frameDroppingOn = true;
  h264_settings.keyFrameInterval = 3000;
  h264_settings.spsData = NULL;
  h264_settings.spsLen = 0;
  h264_settings.ppsData = NULL;
  h264_settings.ppsLen = 0;

  return h264_settings;
}

VCMDecoderMapItem::VCMDecoderMapItem(VideoCodec* settings,
                                     int number_of_cores,
                                     bool require_key_frame)
    : settings(settings),
      number_of_cores(number_of_cores),
      require_key_frame(require_key_frame) {
  assert(number_of_cores >= 0);
}

VCMExtDecoderMapItem::VCMExtDecoderMapItem(
    VideoDecoder* external_decoder_instance,
    uint8_t payload_type,
    bool internal_render_timing)
    : payload_type(payload_type),
      external_decoder_instance(external_decoder_instance),
      internal_render_timing(internal_render_timing) {
}

VCMCodecDataBase::VCMCodecDataBase(
    VideoEncoderRateObserver* encoder_rate_observer)
    : number_of_cores_(0),
      max_payload_size_(kDefaultPayloadSize),
      periodic_key_frames_(false),
      pending_encoder_reset_(true),
      current_enc_is_external_(false),
      send_codec_(),
      receive_codec_(),
      external_payload_type_(0),
      external_encoder_(NULL),
      internal_source_(false),
      encoder_rate_observer_(encoder_rate_observer),
      ptr_encoder_(NULL),
      ptr_decoder_(NULL),
      dec_map_(),
      dec_external_map_() {
}

VCMCodecDataBase::~VCMCodecDataBase() {
  ResetSender();
  ResetReceiver();
}

int VCMCodecDataBase::NumberOfCodecs() {
  return VCM_NUM_VIDEO_CODECS_AVAILABLE;
}

bool VCMCodecDataBase::Codec(int list_id,
                             VideoCodec* settings) {
  if (!settings) {
    return false;
  }
  if (list_id >= VCM_NUM_VIDEO_CODECS_AVAILABLE) {
    return false;
  }
  memset(settings, 0, sizeof(VideoCodec));
  switch (list_id) {
#ifdef VIDEOCODEC_VP8
    case VCM_VP8_IDX: {
      strncpy(settings->plName, "VP8", 4);
      settings->codecType = kVideoCodecVP8;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = VCM_VP8_PAYLOAD_TYPE;
      settings->startBitrate = kDefaultStartBitrateKbps;
      settings->minBitrate = VCM_MIN_BITRATE;
      settings->maxBitrate = 0;
      settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
      settings->width = VCM_DEFAULT_CODEC_WIDTH;
      settings->height = VCM_DEFAULT_CODEC_HEIGHT;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
      return true;
    }
#endif
#ifdef VIDEOCODEC_VP9
    case VCM_VP9_IDX: {
      strncpy(settings->plName, "VP9", 4);
      settings->codecType = kVideoCodecVP9;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = VCM_VP9_PAYLOAD_TYPE;
      settings->startBitrate = 100;
      settings->minBitrate = VCM_MIN_BITRATE;
      settings->maxBitrate = 0;
      settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
      settings->width = VCM_DEFAULT_CODEC_WIDTH;
      settings->height = VCM_DEFAULT_CODEC_HEIGHT;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
      return true;
    }
#endif
#ifdef VIDEOCODEC_H264
    case VCM_H264_IDX: {
      strncpy(settings->plName, "H264", 5);
      settings->codecType = kVideoCodecH264;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = VCM_H264_PAYLOAD_TYPE;
      settings->startBitrate = kDefaultStartBitrateKbps;
      settings->minBitrate = VCM_MIN_BITRATE;
      settings->maxBitrate = 0;
      settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
      settings->width = VCM_DEFAULT_CODEC_WIDTH;
      settings->height = VCM_DEFAULT_CODEC_HEIGHT;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
      return true;
    }
#endif
#ifdef VIDEOCODEC_I420
    case VCM_I420_IDX: {
      strncpy(settings->plName, "I420", 5);
      settings->codecType = kVideoCodecI420;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = VCM_I420_PAYLOAD_TYPE;
      // Bitrate needed for this size and framerate.
      settings->startBitrate = 3 * VCM_DEFAULT_CODEC_WIDTH *
                               VCM_DEFAULT_CODEC_HEIGHT * 8 *
                               VCM_DEFAULT_FRAME_RATE / 1000 / 2;
      settings->maxBitrate = settings->startBitrate;
      settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
      settings->width = VCM_DEFAULT_CODEC_WIDTH;
      settings->height = VCM_DEFAULT_CODEC_HEIGHT;
      settings->minBitrate = VCM_MIN_BITRATE;
      settings->numberOfSimulcastStreams = 0;
      return true;
    }
#endif
    default: {
      return false;
    }
  }
}

bool VCMCodecDataBase::Codec(VideoCodecType codec_type,
                             VideoCodec* settings) {
  for (int i = 0; i < VCMCodecDataBase::NumberOfCodecs(); i++) {
    const bool ret = VCMCodecDataBase::Codec(i, settings);
    if (!ret) {
      return false;
    }
    if (codec_type == settings->codecType) {
      return true;
    }
  }
  return false;
}

void VCMCodecDataBase::ResetSender() {
  DeleteEncoder();
  periodic_key_frames_ = false;
}

// Assuming only one registered encoder - since only one used, no need for more.
bool VCMCodecDataBase::SetSendCodec(
    const VideoCodec* send_codec,
    int number_of_cores,
    size_t max_payload_size,
    VCMEncodedFrameCallback* encoded_frame_callback) {
  DCHECK(send_codec);
  if (max_payload_size == 0) {
    max_payload_size = kDefaultPayloadSize;
  }
  DCHECK_GE(number_of_cores, 1);
  DCHECK_GE(send_codec->plType, 1);
  // Make sure the start bit rate is sane...
  DCHECK_LE(send_codec->startBitrate, 1000000u);
  DCHECK(send_codec->codecType != kVideoCodecUnknown);
  bool reset_required = pending_encoder_reset_;
  if (number_of_cores_ != number_of_cores) {
    number_of_cores_ = number_of_cores;
    reset_required = true;
  }
  if (max_payload_size_ != max_payload_size) {
    max_payload_size_ = max_payload_size;
    reset_required = true;
  }

  VideoCodec new_send_codec;
  memcpy(&new_send_codec, send_codec, sizeof(new_send_codec));

  if (new_send_codec.maxBitrate == 0) {
    // max is one bit per pixel
    new_send_codec.maxBitrate = (static_cast<int>(send_codec->height) *
        static_cast<int>(send_codec->width) *
        static_cast<int>(send_codec->maxFramerate)) / 1000;
    if (send_codec->startBitrate > new_send_codec.maxBitrate) {
      // But if the user tries to set a higher start bit rate we will
      // increase the max accordingly.
      new_send_codec.maxBitrate = send_codec->startBitrate;
    }
  }

  if (new_send_codec.startBitrate > new_send_codec.maxBitrate)
    new_send_codec.startBitrate = new_send_codec.maxBitrate;

  if (!reset_required) {
    reset_required = RequiresEncoderReset(new_send_codec);
  }

  memcpy(&send_codec_, &new_send_codec, sizeof(send_codec_));

  if (!reset_required) {
    encoded_frame_callback->SetPayloadType(send_codec_.plType);
    if (ptr_encoder_->RegisterEncodeCallback(encoded_frame_callback) < 0) {
      LOG(LS_ERROR) << "Failed to register encoded-frame callback.";
      return false;
    }
    return true;
  }

  // If encoder exists, will destroy it and create new one.
  DeleteEncoder();
  if (send_codec_.plType == external_payload_type_) {
    // External encoder.
    ptr_encoder_ = new VCMGenericEncoder(
        external_encoder_, encoder_rate_observer_, internal_source_);
    current_enc_is_external_ = true;
  } else {
    ptr_encoder_ = CreateEncoder(send_codec_.codecType);
    current_enc_is_external_ = false;
    if (!ptr_encoder_)
      return false;
  }
  encoded_frame_callback->SetPayloadType(send_codec_.plType);
  if (ptr_encoder_->InitEncode(&send_codec_, number_of_cores_,
                               max_payload_size_) < 0) {
    LOG(LS_ERROR) << "Failed to initialize video encoder.";
    DeleteEncoder();
    return false;
  } else if (ptr_encoder_->RegisterEncodeCallback(encoded_frame_callback) < 0) {
    LOG(LS_ERROR) << "Failed to register encoded-frame callback.";
    DeleteEncoder();
    return false;
  }

  // Intentionally don't check return value since the encoder registration
  // shouldn't fail because the codec doesn't support changing the periodic key
  // frame setting.
  ptr_encoder_->SetPeriodicKeyFrames(periodic_key_frames_);

  pending_encoder_reset_ = false;

  return true;
}

bool VCMCodecDataBase::SendCodec(VideoCodec* current_send_codec) const {
  if (!ptr_encoder_) {
    return false;
  }
  memcpy(current_send_codec, &send_codec_, sizeof(VideoCodec));
  return true;
}

VideoCodecType VCMCodecDataBase::SendCodec() const {
  if (!ptr_encoder_) {
    return kVideoCodecUnknown;
  }
  return send_codec_.codecType;
}

bool VCMCodecDataBase::DeregisterExternalEncoder(
    uint8_t payload_type, bool* was_send_codec) {
  assert(was_send_codec);
  *was_send_codec = false;
  if (external_payload_type_ != payload_type) {
    return false;
  }
  if (send_codec_.plType == payload_type) {
    // De-register as send codec if needed.
    DeleteEncoder();
    memset(&send_codec_, 0, sizeof(VideoCodec));
    current_enc_is_external_ = false;
    *was_send_codec = true;
  }
  external_payload_type_ = 0;
  external_encoder_ = NULL;
  internal_source_ = false;
  return true;
}

void VCMCodecDataBase::RegisterExternalEncoder(
    VideoEncoder* external_encoder,
    uint8_t payload_type,
    bool internal_source) {
  // Since only one encoder can be used at a given time, only one external
  // encoder can be registered/used.
  external_encoder_ = external_encoder;
  external_payload_type_ = payload_type;
  internal_source_ = internal_source;
  pending_encoder_reset_ = true;
}

bool VCMCodecDataBase::RequiresEncoderReset(const VideoCodec& new_send_codec) {
  if (ptr_encoder_ == NULL) {
    return true;
  }

  // Does not check startBitrate or maxFramerate
  if (new_send_codec.codecType != send_codec_.codecType ||
      strcmp(new_send_codec.plName, send_codec_.plName) != 0 ||
      new_send_codec.plType != send_codec_.plType ||
      new_send_codec.width != send_codec_.width ||
      new_send_codec.height != send_codec_.height ||
      new_send_codec.maxBitrate != send_codec_.maxBitrate ||
      new_send_codec.minBitrate != send_codec_.minBitrate ||
      new_send_codec.qpMax != send_codec_.qpMax ||
      new_send_codec.numberOfSimulcastStreams !=
          send_codec_.numberOfSimulcastStreams ||
      new_send_codec.mode != send_codec_.mode ||
      new_send_codec.extra_options != send_codec_.extra_options) {
    return true;
  }

  switch (new_send_codec.codecType) {
    case kVideoCodecVP8:
      if (memcmp(&new_send_codec.codecSpecific.VP8,
                 &send_codec_.codecSpecific.VP8,
                 sizeof(new_send_codec.codecSpecific.VP8)) != 0) {
        return true;
      }
      break;
    case kVideoCodecVP9:
      if (memcmp(&new_send_codec.codecSpecific.VP9,
                 &send_codec_.codecSpecific.VP9,
                 sizeof(new_send_codec.codecSpecific.VP9)) != 0) {
        return true;
      }
      break;
    case kVideoCodecH264:
      if (memcmp(&new_send_codec.codecSpecific.H264,
                 &send_codec_.codecSpecific.H264,
                 sizeof(new_send_codec.codecSpecific.H264)) != 0) {
        return true;
      }
      break;
    case kVideoCodecGeneric:
      break;
    // Known codecs without payload-specifics
    case kVideoCodecI420:
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
      break;
    // Unknown codec type, reset just to be sure.
    case kVideoCodecUnknown:
      return true;
  }

  if (new_send_codec.numberOfSimulcastStreams > 0) {
    for (unsigned char i = 0; i < new_send_codec.numberOfSimulcastStreams;
         ++i) {
      if (memcmp(&new_send_codec.simulcastStream[i],
                 &send_codec_.simulcastStream[i],
                 sizeof(new_send_codec.simulcastStream[i])) !=
          0) {
        return true;
      }
    }
  }
  return false;
}

VCMGenericEncoder* VCMCodecDataBase::GetEncoder() {
  return ptr_encoder_;
}

bool VCMCodecDataBase::SetPeriodicKeyFrames(bool enable) {
  periodic_key_frames_ = enable;
  if (ptr_encoder_) {
    return (ptr_encoder_->SetPeriodicKeyFrames(periodic_key_frames_) == 0);
  }
  return true;
}

void VCMCodecDataBase::ResetReceiver() {
  ReleaseDecoder(ptr_decoder_);
  ptr_decoder_ = NULL;
  memset(&receive_codec_, 0, sizeof(VideoCodec));
  while (!dec_map_.empty()) {
    DecoderMap::iterator it = dec_map_.begin();
    delete (*it).second;
    dec_map_.erase(it);
  }
  while (!dec_external_map_.empty()) {
    ExternalDecoderMap::iterator external_it = dec_external_map_.begin();
    delete (*external_it).second;
    dec_external_map_.erase(external_it);
  }
}

bool VCMCodecDataBase::DeregisterExternalDecoder(uint8_t payload_type) {
  ExternalDecoderMap::iterator it = dec_external_map_.find(payload_type);
  if (it == dec_external_map_.end()) {
    // Not found
    return false;
  }
  // We can't use payload_type to check if the decoder is currently in use,
  // because payload type may be out of date (e.g. before we decode the first
  // frame after RegisterReceiveCodec)
  if (ptr_decoder_ != NULL &&
      &ptr_decoder_->_decoder == (*it).second->external_decoder_instance) {
    // Release it if it was registered and in use.
    ReleaseDecoder(ptr_decoder_);
    ptr_decoder_ = NULL;
  }
  DeregisterReceiveCodec(payload_type);
  delete (*it).second;
  dec_external_map_.erase(it);
  return true;
}

// Add the external encoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
bool VCMCodecDataBase::RegisterExternalDecoder(
    VideoDecoder* external_decoder,
    uint8_t payload_type,
    bool internal_render_timing) {
  // Check if payload value already exists, if so  - erase old and insert new.
  VCMExtDecoderMapItem* ext_decoder = new VCMExtDecoderMapItem(
      external_decoder, payload_type, internal_render_timing);
  if (!ext_decoder) {
    return false;
  }
  DeregisterExternalDecoder(payload_type);
  dec_external_map_[payload_type] = ext_decoder;
  return true;
}

bool VCMCodecDataBase::DecoderRegistered() const {
  return !dec_map_.empty();
}

bool VCMCodecDataBase::RegisterReceiveCodec(
    const VideoCodec* receive_codec,
    int number_of_cores,
    bool require_key_frame) {
  if (number_of_cores < 0) {
    return false;
  }
  // Check if payload value already exists, if so  - erase old and insert new.
  DeregisterReceiveCodec(receive_codec->plType);
  if (receive_codec->codecType == kVideoCodecUnknown) {
    return false;
  }
  VideoCodec* new_receive_codec = new VideoCodec(*receive_codec);
  dec_map_[receive_codec->plType] = new VCMDecoderMapItem(new_receive_codec,
                                                          number_of_cores,
                                                          require_key_frame);
  return true;
}

bool VCMCodecDataBase::DeregisterReceiveCodec(
    uint8_t payload_type) {
  DecoderMap::iterator it = dec_map_.find(payload_type);
  if (it == dec_map_.end()) {
    return false;
  }
  VCMDecoderMapItem* dec_item = (*it).second;
  delete dec_item;
  dec_map_.erase(it);
  if (receive_codec_.plType == payload_type) {
    // This codec is currently in use.
    memset(&receive_codec_, 0, sizeof(VideoCodec));
  }
  return true;
}

bool VCMCodecDataBase::ReceiveCodec(VideoCodec* current_receive_codec) const {
  assert(current_receive_codec);
  if (!ptr_decoder_) {
    return false;
  }
  memcpy(current_receive_codec, &receive_codec_, sizeof(VideoCodec));
  return true;
}

VideoCodecType VCMCodecDataBase::ReceiveCodec() const {
  if (!ptr_decoder_) {
    return kVideoCodecUnknown;
  }
  return receive_codec_.codecType;
}

VCMGenericDecoder* VCMCodecDataBase::GetDecoder(
    uint8_t payload_type, VCMDecodedFrameCallback* decoded_frame_callback) {
  if (payload_type == receive_codec_.plType || payload_type == 0) {
    return ptr_decoder_;
  }
  // Check for exisitng decoder, if exists - delete.
  if (ptr_decoder_) {
    ReleaseDecoder(ptr_decoder_);
    ptr_decoder_ = NULL;
    memset(&receive_codec_, 0, sizeof(VideoCodec));
  }
  ptr_decoder_ = CreateAndInitDecoder(payload_type, &receive_codec_);
  if (!ptr_decoder_) {
    return NULL;
  }
  VCMReceiveCallback* callback = decoded_frame_callback->UserReceiveCallback();
  if (callback) callback->IncomingCodecChanged(receive_codec_);
  if (ptr_decoder_->RegisterDecodeCompleteCallback(decoded_frame_callback)
      < 0) {
    ReleaseDecoder(ptr_decoder_);
    ptr_decoder_ = NULL;
    memset(&receive_codec_, 0, sizeof(VideoCodec));
    return NULL;
  }
  return ptr_decoder_;
}

void VCMCodecDataBase::ReleaseDecoder(VCMGenericDecoder* decoder) const {
  if (decoder) {
    assert(&decoder->_decoder);
    decoder->Release();
    if (!decoder->External()) {
      delete &decoder->_decoder;
    }
    delete decoder;
  }
}

bool VCMCodecDataBase::SupportsRenderScheduling() const {
  const VCMExtDecoderMapItem* ext_item = FindExternalDecoderItem(
      receive_codec_.plType);
  if (ext_item == nullptr)
    return true;
  return ext_item->internal_render_timing;
}

bool VCMCodecDataBase::MatchesCurrentResolution(int width, int height) const {
  return send_codec_.width == width && send_codec_.height == height;
}

VCMGenericDecoder* VCMCodecDataBase::CreateAndInitDecoder(
    uint8_t payload_type,
    VideoCodec* new_codec) const {
  assert(new_codec);
  const VCMDecoderMapItem* decoder_item = FindDecoderItem(payload_type);
  if (!decoder_item) {
    LOG(LS_ERROR) << "Can't find a decoder associated with payload type: "
                  << static_cast<int>(payload_type);
    return NULL;
  }
  VCMGenericDecoder* ptr_decoder = NULL;
  const VCMExtDecoderMapItem* external_dec_item =
      FindExternalDecoderItem(payload_type);
  if (external_dec_item) {
    // External codec.
    ptr_decoder = new VCMGenericDecoder(
        *external_dec_item->external_decoder_instance, true);
  } else {
    // Create decoder.
    ptr_decoder = CreateDecoder(decoder_item->settings->codecType);
  }
  if (!ptr_decoder)
    return NULL;

  if (ptr_decoder->InitDecode(decoder_item->settings.get(),
                              decoder_item->number_of_cores) < 0) {
    ReleaseDecoder(ptr_decoder);
    return NULL;
  }
  memcpy(new_codec, decoder_item->settings.get(), sizeof(VideoCodec));
  return ptr_decoder;
}

VCMGenericEncoder* VCMCodecDataBase::CreateEncoder(
  const VideoCodecType type) const {
  switch (type) {
#ifdef VIDEOCODEC_VP8
    case kVideoCodecVP8:
      return new VCMGenericEncoder(VP8Encoder::Create(), encoder_rate_observer_,
                                   false);
#endif
#ifdef VIDEOCODEC_VP9
    case kVideoCodecVP9:
      return new VCMGenericEncoder(VP9Encoder::Create(), encoder_rate_observer_,
                                   false);
#endif
#ifdef VIDEOCODEC_I420
    case kVideoCodecI420:
      return new VCMGenericEncoder(new I420Encoder(), encoder_rate_observer_,
                                   false);
#endif
    default:
      LOG(LS_WARNING) << "No internal encoder of this type exists.";
      return NULL;
  }
}

void VCMCodecDataBase::DeleteEncoder() {
  if (ptr_encoder_) {
    ptr_encoder_->Release();
    if (!current_enc_is_external_)
      delete ptr_encoder_->encoder_;
    delete ptr_encoder_;
    ptr_encoder_ = NULL;
  }
}

VCMGenericDecoder* VCMCodecDataBase::CreateDecoder(VideoCodecType type) const {
  switch (type) {
#ifdef VIDEOCODEC_VP8
    case kVideoCodecVP8:
      return new VCMGenericDecoder(*(VP8Decoder::Create()));
#endif
#ifdef VIDEOCODEC_VP9
    case kVideoCodecVP9:
      return new VCMGenericDecoder(*(VP9Decoder::Create()));
#endif
#ifdef VIDEOCODEC_I420
    case kVideoCodecI420:
      return new VCMGenericDecoder(*(new I420Decoder));
#endif
    default:
      LOG(LS_WARNING) << "No internal decoder of this type exists.";
      return NULL;
  }
}

const VCMDecoderMapItem* VCMCodecDataBase::FindDecoderItem(
    uint8_t payload_type) const {
  DecoderMap::const_iterator it = dec_map_.find(payload_type);
  if (it != dec_map_.end()) {
    return (*it).second;
  }
  return NULL;
}

const VCMExtDecoderMapItem* VCMCodecDataBase::FindExternalDecoderItem(
    uint8_t payload_type) const {
  ExternalDecoderMap::const_iterator it = dec_external_map_.find(payload_type);
  if (it != dec_external_map_.end()) {
    return (*it).second;
  }
  return NULL;
}
}  // namespace webrtc
