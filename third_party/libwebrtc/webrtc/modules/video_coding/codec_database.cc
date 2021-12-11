/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codec_database.h"

#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/i420/include/i420.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/internal_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace {
const size_t kDefaultPayloadSize = 1440;
}

namespace webrtc {

// Create an internal Decoder given a codec type
static std::unique_ptr<VCMGenericDecoder> CreateDecoder(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return std::unique_ptr<VCMGenericDecoder>(
          new VCMGenericDecoder(VP8Decoder::Create()));
    case kVideoCodecVP9:
      return std::unique_ptr<VCMGenericDecoder>(
          new VCMGenericDecoder(VP9Decoder::Create()));
    case kVideoCodecI420:
      return std::unique_ptr<VCMGenericDecoder>(
          new VCMGenericDecoder(new I420Decoder()));
    case kVideoCodecH264:
      if (H264Decoder::IsSupported()) {
        return std::unique_ptr<VCMGenericDecoder>(
            new VCMGenericDecoder(H264Decoder::Create()));
      }
      break;
    default:
      break;
  }
  RTC_LOG(LS_WARNING) << "No internal decoder of this type exists.";
  return std::unique_ptr<VCMGenericDecoder>();
}

VCMDecoderMapItem::VCMDecoderMapItem(VideoCodec* settings,
                                     int number_of_cores,
                                     bool require_key_frame)
    : settings(settings),
      number_of_cores(number_of_cores),
      require_key_frame(require_key_frame) {
  RTC_DCHECK_GE(number_of_cores, 0);
}

VCMExtDecoderMapItem::VCMExtDecoderMapItem(
    VideoDecoder* external_decoder_instance,
    uint8_t payload_type)
    : payload_type(payload_type),
      external_decoder_instance(external_decoder_instance) {}

VCMCodecDataBase::VCMCodecDataBase(
    VCMEncodedFrameCallback* encoded_frame_callback)
    : number_of_cores_(0),
      max_payload_size_(kDefaultPayloadSize),
      periodic_key_frames_(false),
      pending_encoder_reset_(true),
      send_codec_(),
      receive_codec_(),
      encoder_payload_type_(0),
      external_encoder_(nullptr),
      internal_source_(false),
      encoded_frame_callback_(encoded_frame_callback),
      dec_map_(),
      dec_external_map_() {}

VCMCodecDataBase::~VCMCodecDataBase() {
  DeleteEncoder();
  ptr_decoder_.reset();
  for (auto& kv : dec_map_)
    delete kv.second;
  for (auto& kv : dec_external_map_)
    delete kv.second;
}

// Assuming only one registered encoder - since only one used, no need for more.
bool VCMCodecDataBase::SetSendCodec(const VideoCodec* send_codec,
                                    int number_of_cores,
                                    size_t max_payload_size) {
  RTC_DCHECK(send_codec);
  if (max_payload_size == 0) {
    max_payload_size = kDefaultPayloadSize;
  }
  RTC_DCHECK_GE(number_of_cores, 1);
  RTC_DCHECK_GE(send_codec->plType, 1);
  // Make sure the start bit rate is sane...
  RTC_DCHECK_LE(send_codec->startBitrate, 1000000);
  RTC_DCHECK(send_codec->codecType != kVideoCodecUnknown);
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
                                 static_cast<int>(send_codec->maxFramerate)) /
                                1000;
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
    return true;
  }

  // If encoder exists, will destroy it and create new one.
  DeleteEncoder();
  RTC_DCHECK_EQ(encoder_payload_type_, send_codec_.plType)
      << "Encoder not registered for payload type " << send_codec_.plType;
  ptr_encoder_.reset(new VCMGenericEncoder(
      external_encoder_, encoded_frame_callback_, internal_source_));
  encoded_frame_callback_->SetInternalSource(internal_source_);
  if (ptr_encoder_->InitEncode(&send_codec_, number_of_cores_,
                               max_payload_size_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to initialize video encoder.";
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

bool VCMCodecDataBase::DeregisterExternalEncoder(uint8_t payload_type,
                                                 bool* was_send_codec) {
  RTC_DCHECK(was_send_codec);
  *was_send_codec = false;
  if (encoder_payload_type_ != payload_type) {
    return false;
  }
  if (send_codec_.plType == payload_type) {
    // De-register as send codec if needed.
    DeleteEncoder();
    memset(&send_codec_, 0, sizeof(VideoCodec));
    *was_send_codec = true;
  }
  encoder_payload_type_ = 0;
  external_encoder_ = nullptr;
  internal_source_ = false;
  return true;
}

void VCMCodecDataBase::RegisterExternalEncoder(VideoEncoder* external_encoder,
                                               uint8_t payload_type,
                                               bool internal_source) {
  // Since only one encoder can be used at a given time, only one external
  // encoder can be registered/used.
  external_encoder_ = external_encoder;
  encoder_payload_type_ = payload_type;
  internal_source_ = internal_source;
  pending_encoder_reset_ = true;
}

bool VCMCodecDataBase::RequiresEncoderReset(const VideoCodec& new_send_codec) {
  if (!ptr_encoder_)
    return true;

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
      new_send_codec.mode != send_codec_.mode) {
    return true;
  }

  switch (new_send_codec.codecType) {
    case kVideoCodecVP8:
      if (memcmp(&new_send_codec.VP8(), send_codec_.VP8(),
                 sizeof(new_send_codec.VP8())) != 0) {
        return true;
      }
      break;
    case kVideoCodecVP9:
      if (memcmp(&new_send_codec.VP9(), send_codec_.VP9(),
                 sizeof(new_send_codec.VP9())) != 0) {
        return true;
      }
      break;
    case kVideoCodecH264:
      if (memcmp(&new_send_codec.H264(), send_codec_.H264(),
                 sizeof(new_send_codec.H264())) != 0) {
        return true;
      }
      break;
    case kVideoCodecGeneric:
      break;
    // Known codecs without payload-specifics
    case kVideoCodecI420:
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
    case kVideoCodecFlexfec:
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
                 sizeof(new_send_codec.simulcastStream[i])) != 0) {
        return true;
      }
    }
  }
  return false;
}

VCMGenericEncoder* VCMCodecDataBase::GetEncoder() {
  return ptr_encoder_.get();
}

bool VCMCodecDataBase::SetPeriodicKeyFrames(bool enable) {
  periodic_key_frames_ = enable;
  if (ptr_encoder_) {
    return (ptr_encoder_->SetPeriodicKeyFrames(periodic_key_frames_) == 0);
  }
  return true;
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
  if (ptr_decoder_ &&
      ptr_decoder_->IsSameDecoder((*it).second->external_decoder_instance)) {
    // Release it if it was registered and in use.
    ptr_decoder_.reset();
  }
  DeregisterReceiveCodec(payload_type);
  delete it->second;
  dec_external_map_.erase(it);
  return true;
}

// Add the external encoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
void VCMCodecDataBase::RegisterExternalDecoder(VideoDecoder* external_decoder,
                                               uint8_t payload_type) {
  // Check if payload value already exists, if so  - erase old and insert new.
  VCMExtDecoderMapItem* ext_decoder =
      new VCMExtDecoderMapItem(external_decoder, payload_type);
  DeregisterExternalDecoder(payload_type);
  dec_external_map_[payload_type] = ext_decoder;
}

bool VCMCodecDataBase::DecoderRegistered() const {
  return !dec_map_.empty();
}

bool VCMCodecDataBase::RegisterReceiveCodec(const VideoCodec* receive_codec,
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
  dec_map_[receive_codec->plType] = new VCMDecoderMapItem(
      new_receive_codec, number_of_cores, require_key_frame);
  return true;
}

bool VCMCodecDataBase::DeregisterReceiveCodec(uint8_t payload_type) {
  DecoderMap::iterator it = dec_map_.find(payload_type);
  if (it == dec_map_.end()) {
    return false;
  }
  delete it->second;
  dec_map_.erase(it);
  if (receive_codec_.plType == payload_type) {
    // This codec is currently in use.
    memset(&receive_codec_, 0, sizeof(VideoCodec));
  }
  return true;
}

VCMGenericDecoder* VCMCodecDataBase::GetDecoder(
    const VCMEncodedFrame& frame,
    VCMDecodedFrameCallback* decoded_frame_callback) {
  RTC_DCHECK(decoded_frame_callback->UserReceiveCallback());
  uint8_t payload_type = frame.PayloadType();
  if (payload_type == receive_codec_.plType || payload_type == 0) {
    return ptr_decoder_.get();
  }
  // Check for exisitng decoder, if exists - delete.
  if (ptr_decoder_) {
    ptr_decoder_.reset();
    memset(&receive_codec_, 0, sizeof(VideoCodec));
  }
  ptr_decoder_ = CreateAndInitDecoder(frame, &receive_codec_);
  if (!ptr_decoder_) {
    return nullptr;
  }
  VCMReceiveCallback* callback = decoded_frame_callback->UserReceiveCallback();
  callback->OnIncomingPayloadType(receive_codec_.plType);
  if (ptr_decoder_->RegisterDecodeCompleteCallback(decoded_frame_callback) <
      0) {
    ptr_decoder_.reset();
    memset(&receive_codec_, 0, sizeof(VideoCodec));
    return nullptr;
  }
  return ptr_decoder_.get();
}

VCMGenericDecoder* VCMCodecDataBase::GetCurrentDecoder() {
  return ptr_decoder_.get();
}

bool VCMCodecDataBase::PrefersLateDecoding() const {
  return ptr_decoder_ ? ptr_decoder_->PrefersLateDecoding() : true;
}

bool VCMCodecDataBase::MatchesCurrentResolution(int width, int height) const {
  return send_codec_.width == width && send_codec_.height == height;
}

std::unique_ptr<VCMGenericDecoder> VCMCodecDataBase::CreateAndInitDecoder(
    const VCMEncodedFrame& frame,
    VideoCodec* new_codec) const {
  uint8_t payload_type = frame.PayloadType();
  RTC_LOG(LS_INFO) << "Initializing decoder with payload type '"
                   << static_cast<int>(payload_type) << "'.";
  RTC_DCHECK(new_codec);
  const VCMDecoderMapItem* decoder_item = FindDecoderItem(payload_type);
  if (!decoder_item) {
    RTC_LOG(LS_ERROR) << "Can't find a decoder associated with payload type: "
                      << static_cast<int>(payload_type);
    return nullptr;
  }
  std::unique_ptr<VCMGenericDecoder> ptr_decoder;
  const VCMExtDecoderMapItem* external_dec_item =
      FindExternalDecoderItem(payload_type);
  if (external_dec_item) {
    // External codec.
    ptr_decoder.reset(new VCMGenericDecoder(
        external_dec_item->external_decoder_instance, true));
  } else {
    // Create decoder.
    ptr_decoder = CreateDecoder(decoder_item->settings->codecType);
  }
  if (!ptr_decoder)
    return nullptr;

  // Copy over input resolutions to prevent codec reinitialization due to
  // the first frame being of a different resolution than the database values.
  // This is best effort, since there's no guarantee that width/height have been
  // parsed yet (and may be zero).
  if (frame.EncodedImage()._encodedWidth > 0 &&
      frame.EncodedImage()._encodedHeight > 0) {
    decoder_item->settings->width = frame.EncodedImage()._encodedWidth;
    decoder_item->settings->height = frame.EncodedImage()._encodedHeight;
  }
  if (ptr_decoder->InitDecode(decoder_item->settings.get(),
                              decoder_item->number_of_cores) < 0) {
    return nullptr;
  }
  memcpy(new_codec, decoder_item->settings.get(), sizeof(VideoCodec));
  return ptr_decoder;
}

void VCMCodecDataBase::DeleteEncoder() {
  if (!ptr_encoder_)
    return;
  ptr_encoder_->Release();
  ptr_encoder_.reset();
}

const VCMDecoderMapItem* VCMCodecDataBase::FindDecoderItem(
    uint8_t payload_type) const {
  DecoderMap::const_iterator it = dec_map_.find(payload_type);
  if (it != dec_map_.end()) {
    return (*it).second;
  }
  return nullptr;
}

const VCMExtDecoderMapItem* VCMCodecDataBase::FindExternalDecoderItem(
    uint8_t payload_type) const {
  ExternalDecoderMap::const_iterator it = dec_external_map_.find(payload_type);
  if (it != dec_external_map_.end()) {
    return (*it).second;
  }
  return nullptr;
}
}  // namespace webrtc
