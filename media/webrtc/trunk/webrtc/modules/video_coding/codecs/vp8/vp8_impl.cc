/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * This file contains the WEBRTC VP8 wrapper implementation
 *
 */

#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"

#include "webrtc/common.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/codecs/vp8/reference_picture_selection.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

enum { kVp8ErrorPropagationTh = 30 };

namespace webrtc {

VP8EncoderImpl::VP8EncoderImpl()
    : encoded_image_(),
      encoded_complete_callback_(NULL),
      inited_(false),
      timestamp_(0),
      picture_id_(0),
      feedback_mode_(false),
      cpu_speed_(-6),  // default value
      rc_max_intra_target_(0),
      token_partitions_(VP8_ONE_TOKENPARTITION),
      rps_(new ReferencePictureSelection),
      temporal_layers_(NULL),
      encoder_(NULL),
      config_(NULL),
      raw_(NULL) {
  memset(&codec_, 0, sizeof(codec_));
  uint32_t seed = static_cast<uint32_t>(TickTime::MillisecondTimestamp());
  srand(seed);
}

VP8EncoderImpl::~VP8EncoderImpl() {
  Release();
  delete rps_;
}

int VP8EncoderImpl::Release() {
  if (encoded_image_._buffer != NULL) {
    delete [] encoded_image_._buffer;
    encoded_image_._buffer = NULL;
  }
  if (encoder_ != NULL) {
    if (vpx_codec_destroy(encoder_)) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    delete encoder_;
    encoder_ = NULL;
  }
  if (config_ != NULL) {
    delete config_;
    config_ = NULL;
  }
  if (raw_ != NULL) {
    vpx_img_free(raw_);
    raw_ = NULL;
  }
  delete temporal_layers_;
  temporal_layers_ = NULL;
  inited_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::SetRates(uint32_t new_bitrate_kbit,
                             uint32_t new_framerate) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoder_->err) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (new_framerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // update bit rate
  if (codec_.maxBitrate > 0 && new_bitrate_kbit > codec_.maxBitrate) {
    new_bitrate_kbit = codec_.maxBitrate;
  }
  config_->rc_target_bitrate = new_bitrate_kbit;  // in kbit/s
  temporal_layers_->ConfigureBitrates(new_bitrate_kbit, codec_.maxBitrate,
                                      new_framerate, config_);
  codec_.maxFramerate = new_framerate;
  quality_scaler_.ReportFramerate(new_framerate);

  // update encoder context
  if (vpx_codec_enc_config_set(encoder_, config_)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::InitEncode(const VideoCodec* inst,
                               int number_of_cores,
                               uint32_t /*max_payload_size*/) {
  if (inst == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width < 1 || inst->height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  feedback_mode_ = inst->codecSpecific.VP8.feedbackModeOn;

  int retVal = Release();
  if (retVal < 0) {
    return retVal;
  }
  if (encoder_ == NULL) {
    encoder_ = new vpx_codec_ctx_t;
  }
  if (config_ == NULL) {
    config_ = new vpx_codec_enc_cfg_t;
  }
  timestamp_ = 0;

  if (&codec_ != inst) {
    codec_ = *inst;
  }

  // TODO(andresp): assert(inst->extra_options) and cleanup.
  Config default_options;
  const Config& options =
      inst->extra_options ? *inst->extra_options : default_options;

  int num_temporal_layers = inst->codecSpecific.VP8.numberOfTemporalLayers > 1 ?
      inst->codecSpecific.VP8.numberOfTemporalLayers : 1;
  assert(temporal_layers_ == NULL);
  temporal_layers_ = options.Get<TemporalLayers::Factory>()
                         .Create(num_temporal_layers, rand());
  // random start 16 bits is enough.
  picture_id_ = static_cast<uint16_t>(rand()) & 0x7FFF;

  // allocate memory for encoded image
  if (encoded_image_._buffer != NULL) {
    delete [] encoded_image_._buffer;
  }
  // Reserve 100 extra bytes for overhead at small resolutions.
  encoded_image_._size = CalcBufferSize(kI420, codec_.width, codec_.height)
                         + 100;
  encoded_image_._buffer = new uint8_t[encoded_image_._size];
  encoded_image_._completeFrame = true;

  // Creating a wrapper to the image - setting image data to NULL. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  raw_ = vpx_img_wrap(NULL, VPX_IMG_FMT_I420, codec_.width, codec_.height,
                      1, NULL);
  // populate encoder configuration with default values
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), config_, 0)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  config_->g_w = codec_.width;
  config_->g_h = codec_.height;
  config_->rc_target_bitrate = inst->startBitrate;  // in kbit/s
  temporal_layers_->ConfigureBitrates(inst->startBitrate, inst->maxBitrate,
                                      inst->maxFramerate, config_);
  // setting the time base of the codec
  config_->g_timebase.num = 1;
  config_->g_timebase.den = 90000;

  // Set the error resilience mode according to user settings.
  switch (inst->codecSpecific.VP8.resilience) {
    case kResilienceOff:
      config_->g_error_resilient = 0;
      if (num_temporal_layers > 1) {
        // Must be on for temporal layers (i.e., |num_temporal_layers| > 1).
        config_->g_error_resilient = 1;
      }
      break;
    case kResilientStream:
      config_->g_error_resilient = 1;  // TODO(holmer): Replace with
      // VPX_ERROR_RESILIENT_DEFAULT when we
      // drop support for libvpx 9.6.0.
      break;
    case kResilientFrames:
#ifdef INDEPENDENT_PARTITIONS
      config_->g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT |
      VPX_ERROR_RESILIENT_PARTITIONS;
      break;
#else
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;  // Not supported
#endif
  }
  config_->g_lag_in_frames = 0;  // 0- no frame lagging

  if (codec_.width * codec_.height >= 1920 * 1080 && number_of_cores > 8) {
    config_->g_threads = 8;  // 8 threads for 1080p on high perf machines.
  } else if (codec_.width * codec_.height > 1280 * 960 &&
      number_of_cores >= 6) {
    config_->g_threads = 3;  // 3 threads for 1080p.
  } else if (codec_.width * codec_.height > 640 * 480 && number_of_cores >= 3) {
    config_->g_threads = 2;  // 2 threads for qHD/HD.
  } else {
    config_->g_threads = 1;  // 1 thread for VGA or less
  }

  // rate control settings
  config_->rc_dropframe_thresh = inst->codecSpecific.VP8.frameDroppingOn ?
      30 : 0;
  config_->rc_end_usage = VPX_CBR;
  config_->g_pass = VPX_RC_ONE_PASS;
  // Handle resizing outside of libvpx.
  config_->rc_resize_allowed = 0;
  config_->rc_min_quantizer = 2;
  config_->rc_max_quantizer = inst->qpMax;
  config_->rc_undershoot_pct = 100;
  config_->rc_overshoot_pct = 15;
  config_->rc_buf_initial_sz = 500;
  config_->rc_buf_optimal_sz = 600;
  config_->rc_buf_sz = 1000;
  // set the maximum target size of any key-frame.
  rc_max_intra_target_ = MaxIntraTarget(config_->rc_buf_optimal_sz);

  if (feedback_mode_) {
    // Disable periodic key frames if we get feedback from the decoder
    // through SLI and RPSI.
    config_->kf_mode = VPX_KF_DISABLED;
  } else if (inst->codecSpecific.VP8.keyFrameInterval  > 0) {
    config_->kf_mode = VPX_KF_AUTO;
    config_->kf_max_dist = inst->codecSpecific.VP8.keyFrameInterval;
  } else {
    config_->kf_mode = VPX_KF_DISABLED;
  }
  switch (inst->codecSpecific.VP8.complexity) {
    case kComplexityHigh:
      cpu_speed_ = -5;
      break;
    case kComplexityHigher:
      cpu_speed_ = -4;
      break;
    case kComplexityMax:
      cpu_speed_ = -3;
      break;
    default:
      cpu_speed_ = -6;
      break;
  }
#if defined(WEBRTC_ARCH_ARM)
  // On mobile platform, always set to -12 to leverage between cpu usage
  // and video quality
  cpu_speed_ = -12;
#endif
  rps_->Init();
  quality_scaler_.Init(codec_.qpMax);
  quality_scaler_.ReportFramerate(codec_.maxFramerate);
  return InitAndSetControlSettings(inst);
}

int VP8EncoderImpl::InitAndSetControlSettings(const VideoCodec* inst) {
  vpx_codec_flags_t flags = 0;
  // TODO(holmer): We should make a smarter decision on the number of
  // partitions. Eight is probably not the optimal number for low resolution
  // video.
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;
  if (vpx_codec_enc_init(encoder_, vpx_codec_vp8_cx(), config_, flags)) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  vpx_codec_control(encoder_, VP8E_SET_STATIC_THRESHOLD, 1);
  vpx_codec_control(encoder_, VP8E_SET_CPUUSED, cpu_speed_);
  vpx_codec_control(encoder_, VP8E_SET_TOKEN_PARTITIONS,
                    static_cast<vp8e_token_partitions>(token_partitions_));
#if !defined(WEBRTC_ARCH_ARM)
  // TODO(fbarchard): Enable Noise reduction for ARM once optimized.
  vpx_codec_control(encoder_, VP8E_SET_NOISE_SENSITIVITY,
                    inst->codecSpecific.VP8.denoisingOn ? 1 : 0);
#endif
  vpx_codec_control(encoder_, VP8E_SET_MAX_INTRA_BITRATE_PCT,
                    rc_max_intra_target_);
  inited_ = true;

  return WEBRTC_VIDEO_CODEC_OK;
}

uint32_t VP8EncoderImpl::MaxIntraTarget(uint32_t optimalBuffersize) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scalePar.
  // Max target size = scalePar * optimalBufferSize * targetBR[Kbps].
  // This values is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / frameRate.
  // The target in % is as follows:

  float scalePar = 0.5;
  uint32_t targetPct = optimalBuffersize * scalePar * codec_.maxFramerate / 10;

  // Don't go below 3 times the per frame bandwidth.
  const uint32_t minIntraTh = 300;
  return (targetPct < minIntraTh) ? minIntraTh: targetPct;
}

int VP8EncoderImpl::Encode(const I420VideoFrame& input_frame,
                           const CodecSpecificInfo* codec_specific_info,
                           const std::vector<VideoFrameType>* frame_types) {
  TRACE_EVENT1("webrtc", "VP8::Encode", "timestamp", input_frame.timestamp());

  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_frame.IsZeroSize()) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (encoded_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  VideoFrameType frame_type = kDeltaFrame;
  // We only support one stream at the moment.
  if (frame_types && frame_types->size() > 0) {
    frame_type = (*frame_types)[0];
  }

  const I420VideoFrame& frame =
      config_->rc_dropframe_thresh > 0 &&
              codec_.codecSpecific.VP8.automaticResizeOn
          ? quality_scaler_.GetScaledFrame(input_frame)
          : input_frame;

  // Check for change in frame size.
  if (frame.width() != codec_.width ||
      frame.height() != codec_.height) {
    int ret = UpdateCodecFrameSize(frame);
    if (ret < 0) {
      return ret;
    }
  }
  // Image in vpx_image_t format.
  // Input frame is const. VP8's raw frame is not defined as const.
  raw_->planes[VPX_PLANE_Y] = const_cast<uint8_t*>(frame.buffer(kYPlane));
  raw_->planes[VPX_PLANE_U] = const_cast<uint8_t*>(frame.buffer(kUPlane));
  raw_->planes[VPX_PLANE_V] = const_cast<uint8_t*>(frame.buffer(kVPlane));
  // TODO(mikhal): Stride should be set in initialization.
  raw_->stride[VPX_PLANE_Y] = frame.stride(kYPlane);
  raw_->stride[VPX_PLANE_U] = frame.stride(kUPlane);
  raw_->stride[VPX_PLANE_V] = frame.stride(kVPlane);

  int flags = temporal_layers_->EncodeFlags(frame.timestamp());

  bool send_keyframe = (frame_type == kKeyFrame);
  if (send_keyframe) {
    // Key frame request from caller.
    // Will update both golden and alt-ref.
    flags = VPX_EFLAG_FORCE_KF;
  } else if (feedback_mode_ && codec_specific_info) {
    // Handle RPSI and SLI messages and set up the appropriate encode flags.
    bool sendRefresh = false;
    if (codec_specific_info->codecType == kVideoCodecVP8) {
      if (codec_specific_info->codecSpecific.VP8.hasReceivedRPSI) {
        rps_->ReceivedRPSI(
            codec_specific_info->codecSpecific.VP8.pictureIdRPSI);
      }
      if (codec_specific_info->codecSpecific.VP8.hasReceivedSLI) {
        sendRefresh = rps_->ReceivedSLI(frame.timestamp());
      }
    }
    flags = rps_->EncodeFlags(picture_id_, sendRefresh,
                              frame.timestamp());
  }

  // TODO(holmer): Ideally the duration should be the timestamp diff of this
  // frame and the next frame to be encoded, which we don't have. Instead we
  // would like to use the duration of the previous frame. Unfortunately the
  // rate control seems to be off with that setup. Using the average input
  // frame rate to calculate an average duration for now.
  assert(codec_.maxFramerate > 0);
  uint32_t duration = 90000 / codec_.maxFramerate;
  if (vpx_codec_encode(encoder_, raw_, timestamp_, duration, flags,
                       VPX_DL_REALTIME)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  timestamp_ += duration;

  return GetEncodedPartitions(frame);
}

int VP8EncoderImpl::UpdateCodecFrameSize(const I420VideoFrame& input_image) {
  codec_.width = input_image.width();
  codec_.height = input_image.height();
  raw_->w = codec_.width;
  raw_->h = codec_.height;
  raw_->d_w = codec_.width;
  raw_->d_h = codec_.height;

  raw_->stride[VPX_PLANE_Y] = input_image.stride(kYPlane);
  raw_->stride[VPX_PLANE_U] = input_image.stride(kUPlane);
  raw_->stride[VPX_PLANE_V] = input_image.stride(kVPlane);
  vpx_img_set_rect(raw_, 0, 0, codec_.width, codec_.height);

  // Update encoder context for new frame size.
  // Change of frame size will automatically trigger a key frame.
  config_->g_w = codec_.width;
  config_->g_h = codec_.height;
  if (vpx_codec_enc_config_set(encoder_, config_)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void VP8EncoderImpl::PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                                       const vpx_codec_cx_pkt& pkt,
                                       uint32_t timestamp) {
  assert(codec_specific != NULL);
  codec_specific->codecType = kVideoCodecVP8;
  CodecSpecificInfoVP8 *vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->pictureId = picture_id_;
  vp8Info->simulcastIdx = 0;
  vp8Info->keyIdx = kNoKeyIdx;  // TODO(hlundin) populate this
  vp8Info->nonReference = (pkt.data.frame.flags & VPX_FRAME_IS_DROPPABLE) != 0;
  temporal_layers_->PopulateCodecSpecific(
      (pkt.data.frame.flags & VPX_FRAME_IS_KEY) ? true : false, vp8Info,
          timestamp);
  picture_id_ = (picture_id_ + 1) & 0x7FFF;  // prepare next
}

int VP8EncoderImpl::GetEncodedPartitions(const I420VideoFrame& input_image) {
  vpx_codec_iter_t iter = NULL;
  int part_idx = 0;
  encoded_image_._length = 0;
  encoded_image_._frameType = kDeltaFrame;
  RTPFragmentationHeader frag_info;
  frag_info.VerifyAndAllocateFragmentationHeader((1 << token_partitions_) + 1);
  CodecSpecificInfo codec_specific;

  const vpx_codec_cx_pkt_t *pkt = NULL;
  while ((pkt = vpx_codec_get_cx_data(encoder_, &iter)) != NULL) {
    switch (pkt->kind) {
      case VPX_CODEC_CX_FRAME_PKT: {
        memcpy(&encoded_image_._buffer[encoded_image_._length],
               pkt->data.frame.buf,
               pkt->data.frame.sz);
        frag_info.fragmentationOffset[part_idx] = encoded_image_._length;
        frag_info.fragmentationLength[part_idx] =  pkt->data.frame.sz;
        frag_info.fragmentationPlType[part_idx] = 0;  // not known here
        frag_info.fragmentationTimeDiff[part_idx] = 0;
        encoded_image_._length += pkt->data.frame.sz;
        assert(encoded_image_._length <= encoded_image_._size);
        ++part_idx;
        break;
      }
      default: {
        break;
      }
    }
    // End of frame
    if ((pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT) == 0) {
      // check if encoded frame is a key frame
      if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
          encoded_image_._frameType = kKeyFrame;
          rps_->EncodedKeyFrame(picture_id_);
      }
      PopulateCodecSpecific(&codec_specific, *pkt, input_image.timestamp());
      break;
    }
  }
  if (encoded_image_._length > 0) {
    TRACE_COUNTER1("webrtc", "EncodedFrameSize", encoded_image_._length);
    encoded_image_._timeStamp = input_image.timestamp();
    encoded_image_.capture_time_ms_ = input_image.render_time_ms();
    encoded_image_._encodedHeight = codec_.height;
    encoded_image_._encodedWidth = codec_.width;
    encoded_complete_callback_->Encoded(encoded_image_, &codec_specific,
                                      &frag_info);
    int qp;
    vpx_codec_control(encoder_, VP8E_GET_LAST_QUANTIZER_64, &qp);
    quality_scaler_.ReportEncodedFrame(qp);
  } else {
    quality_scaler_.ReportDroppedFrame();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::SetChannelParameters(uint32_t /*packet_loss*/, int rtt) {
  rps_->SetRtt(rtt);
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

VP8DecoderImpl::VP8DecoderImpl()
    : decode_complete_callback_(NULL),
      inited_(false),
      feedback_mode_(false),
      decoder_(NULL),
      last_keyframe_(),
      image_format_(VPX_IMG_FMT_NONE),
      ref_frame_(NULL),
      propagation_cnt_(-1),
      mfqe_enabled_(false),
      key_frame_required_(true) {
  memset(&codec_, 0, sizeof(codec_));
}

VP8DecoderImpl::~VP8DecoderImpl() {
  inited_ = true;  // in order to do the actual release
  Release();
}

int VP8DecoderImpl::Reset() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  InitDecode(&codec_, 1);
  propagation_cnt_ = -1;
  mfqe_enabled_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::InitDecode(const VideoCodec* inst, int number_of_cores) {
  if (inst == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  if (decoder_ == NULL) {
    decoder_ = new vpx_dec_ctx_t;
  }
  if (inst->codecType == kVideoCodecVP8) {
    feedback_mode_ = inst->codecSpecific.VP8.feedbackModeOn;
  }
  vpx_codec_dec_cfg_t  cfg;
  // Setting number of threads to a constant value (1)
  cfg.threads = 1;
  cfg.h = cfg.w = 0;  // set after decode

  vpx_codec_flags_t flags = 0;
#ifndef WEBRTC_ARCH_ARM
  flags = VPX_CODEC_USE_POSTPROC;
  if (inst->codecSpecific.VP8.errorConcealmentOn) {
    flags |= VPX_CODEC_USE_ERROR_CONCEALMENT;
  }
#ifdef INDEPENDENT_PARTITIONS
  flags |= VPX_CODEC_USE_INPUT_PARTITION;
#endif
#endif

  if (vpx_codec_dec_init(decoder_, vpx_codec_vp8_dx(), &cfg, flags)) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

#ifndef WEBRTC_ARCH_ARM
  vp8_postproc_cfg_t  ppcfg;
  ppcfg.post_proc_flag = VP8_DEMACROBLOCK | VP8_DEBLOCK;
  // Strength of deblocking filter. Valid range:[0,16]
  ppcfg.deblocking_level = 3;
  vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
#endif

  if (&codec_ != inst) {
    // Save VideoCodec instance for later; mainly for duplicating the decoder.
    codec_ = *inst;
  }

  propagation_cnt_ = -1;

  inited_ = true;

  // Always start with a complete key frame.
  key_frame_required_ = true;

  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::Decode(const EncodedImage& input_image,
                           bool missing_frames,
                           const RTPFragmentationHeader* fragmentation,
                           const CodecSpecificInfo* codec_specific_info,
                           int64_t /*render_time_ms*/) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_image._buffer == NULL && input_image._length > 0) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

#ifdef INDEPENDENT_PARTITIONS
  if (fragmentation == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
#endif

#ifndef WEBRTC_ARCH_ARM
  if (!mfqe_enabled_ && codec_specific_info &&
      codec_specific_info->codecSpecific.VP8.temporalIdx > 0) {
    // Enable MFQE if we are receiving layers.
    // temporalIdx is set in the jitter buffer according to what the RTP
    // header says.
    mfqe_enabled_ = true;
    vp8_postproc_cfg_t  ppcfg;
    ppcfg.post_proc_flag = VP8_MFQE | VP8_DEMACROBLOCK | VP8_DEBLOCK;
    ppcfg.deblocking_level = 3;
    vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
  }
#endif


  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (input_image._frameType != kKeyFrame)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (input_image._completeFrame) {
      key_frame_required_ = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Restrict error propagation using key frame requests. Disabled when
  // the feedback mode is enabled (RPS).
  // Reset on a key frame refresh.
  if (!feedback_mode_) {
    if (input_image._frameType == kKeyFrame && input_image._completeFrame)
      propagation_cnt_ = -1;
    // Start count on first loss.
    else if ((!input_image._completeFrame || missing_frames) &&
        propagation_cnt_ == -1)
      propagation_cnt_ = 0;
    if (propagation_cnt_ >= 0)
      propagation_cnt_++;
  }

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img;
  int ret;

  // Check for missing frames.
  if (missing_frames) {
    // Call decoder with zero data length to signal missing frames.
    if (vpx_codec_decode(decoder_, NULL, 0, 0, VPX_DL_REALTIME)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    // We don't render this frame.
    vpx_codec_get_frame(decoder_, &iter);
    iter = NULL;
  }

#ifdef INDEPENDENT_PARTITIONS
  if (DecodePartitions(inputImage, fragmentation)) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0) {
      propagation_cnt_ = 0;
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#else
  uint8_t* buffer = input_image._buffer;
  if (input_image._length == 0) {
    buffer = NULL;  // Triggers full frame concealment.
  }
  if (vpx_codec_decode(decoder_,
                       buffer,
                       input_image._length,
                       0,
                       VPX_DL_REALTIME)) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#endif

  // Store encoded frame if key frame. (Used in Copy method.)
  if (input_image._frameType == kKeyFrame && input_image._buffer != NULL) {
    const uint32_t bytes_to_copy = input_image._length;
    if (last_keyframe_._size < bytes_to_copy) {
      delete [] last_keyframe_._buffer;
      last_keyframe_._buffer = NULL;
      last_keyframe_._size = 0;
    }

    uint8_t* temp_buffer = last_keyframe_._buffer;  // Save buffer ptr.
    uint32_t temp_size = last_keyframe_._size;  // Save size.
    last_keyframe_ = input_image;  // Shallow copy.
    last_keyframe_._buffer = temp_buffer;  // Restore buffer ptr.
    last_keyframe_._size = temp_size;  // Restore buffer size.
    if (!last_keyframe_._buffer) {
      // Allocate memory.
      last_keyframe_._size = bytes_to_copy;
      last_keyframe_._buffer = new uint8_t[last_keyframe_._size];
    }
    // Copy encoded frame.
    memcpy(last_keyframe_._buffer, input_image._buffer, bytes_to_copy);
    last_keyframe_._length = bytes_to_copy;
  }

  img = vpx_codec_get_frame(decoder_, &iter);
  ret = ReturnFrame(img, input_image._timeStamp, input_image.ntp_time_ms_);
  if (ret != 0) {
    // Reset to avoid requesting key frames too often.
    if (ret < 0 && propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return ret;
  }
  if (feedback_mode_) {
    // Whenever we receive an incomplete key frame all reference buffers will
    // be corrupt. If that happens we must request new key frames until we
    // decode a complete.
    if (input_image._frameType == kKeyFrame && !input_image._completeFrame)
      return WEBRTC_VIDEO_CODEC_ERROR;

    // Check for reference updates and last reference buffer corruption and
    // signal successful reference propagation or frame corruption to the
    // encoder.
    int reference_updates = 0;
    if (vpx_codec_control(decoder_, VP8D_GET_LAST_REF_UPDATES,
                          &reference_updates)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    int corrupted = 0;
    if (vpx_codec_control(decoder_, VP8D_GET_FRAME_CORRUPTED, &corrupted)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    int16_t picture_id = -1;
    if (codec_specific_info) {
      picture_id = codec_specific_info->codecSpecific.VP8.pictureId;
    }
    if (picture_id > -1) {
      if (((reference_updates & VP8_GOLD_FRAME) ||
          (reference_updates & VP8_ALTR_FRAME)) && !corrupted) {
        decode_complete_callback_->ReceivedDecodedReferenceFrame(picture_id);
      }
      decode_complete_callback_->ReceivedDecodedFrame(picture_id);
    }
    if (corrupted) {
      // we can decode but with artifacts
      return WEBRTC_VIDEO_CODEC_REQUEST_SLI;
    }
  }
  // Check Vs. threshold
  if (propagation_cnt_ > kVp8ErrorPropagationTh) {
    // Reset to avoid requesting key frames too often.
    propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::DecodePartitions(
    const EncodedImage& input_image,
    const RTPFragmentationHeader* fragmentation) {
  for (int i = 0; i < fragmentation->fragmentationVectorSize; ++i) {
    const uint8_t* partition = input_image._buffer +
        fragmentation->fragmentationOffset[i];
    const uint32_t partition_length =
        fragmentation->fragmentationLength[i];
    if (vpx_codec_decode(decoder_,
                         partition,
                         partition_length,
                         0,
                         VPX_DL_REALTIME)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Signal end of frame data. If there was no frame data this will trigger
  // a full frame concealment.
  if (vpx_codec_decode(decoder_, NULL, 0, 0, VPX_DL_REALTIME))
    return WEBRTC_VIDEO_CODEC_ERROR;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::ReturnFrame(const vpx_image_t* img,
                                uint32_t timestamp,
                                int64_t ntp_time_ms) {
  if (img == NULL) {
    // Decoder OK and NULL image => No show frame
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }
  int half_height = (img->d_h + 1) / 2;
  int size_y = img->stride[VPX_PLANE_Y] * img->d_h;
  int size_u = img->stride[VPX_PLANE_U] * half_height;
  int size_v = img->stride[VPX_PLANE_V] * half_height;
  // TODO(mikhal): This does  a copy - need to SwapBuffers.
  decoded_image_.CreateFrame(size_y, img->planes[VPX_PLANE_Y],
                             size_u, img->planes[VPX_PLANE_U],
                             size_v, img->planes[VPX_PLANE_V],
                             img->d_w, img->d_h,
                             img->stride[VPX_PLANE_Y],
                             img->stride[VPX_PLANE_U],
                             img->stride[VPX_PLANE_V]);
  decoded_image_.set_timestamp(timestamp);
  decoded_image_.set_ntp_time_ms(ntp_time_ms);
  int ret = decode_complete_callback_->Decoded(decoded_image_);
  if (ret != 0)
    return ret;

  // Remember image format for later
  image_format_ = img->fmt;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::Release() {
  if (last_keyframe_._buffer != NULL) {
    delete [] last_keyframe_._buffer;
    last_keyframe_._buffer = NULL;
  }
  if (decoder_ != NULL) {
    if (vpx_codec_destroy(decoder_)) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    delete decoder_;
    decoder_ = NULL;
  }
  if (ref_frame_ != NULL) {
    vpx_img_free(&ref_frame_->img);
    delete ref_frame_;
    ref_frame_ = NULL;
  }
  inited_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

VideoDecoder* VP8DecoderImpl::Copy() {
  // Sanity checks.
  if (!inited_) {
    // Not initialized.
    assert(false);
    return NULL;
  }
  if (decoded_image_.IsZeroSize()) {
    // Nothing has been decoded before; cannot clone.
    return NULL;
  }
  if (last_keyframe_._buffer == NULL) {
    // Cannot clone if we have no key frame to start with.
    return NULL;
  }
  // Create a new VideoDecoder object
  VP8DecoderImpl *copy = new VP8DecoderImpl;

  // Initialize the new decoder
  if (copy->InitDecode(&codec_, 1) != WEBRTC_VIDEO_CODEC_OK) {
    delete copy;
    return NULL;
  }
  // Inject last key frame into new decoder.
  if (vpx_codec_decode(copy->decoder_, last_keyframe_._buffer,
                       last_keyframe_._length, NULL, VPX_DL_REALTIME)) {
    delete copy;
    return NULL;
  }
  // Allocate memory for reference image copy
  assert(decoded_image_.width() > 0);
  assert(decoded_image_.height() > 0);
  assert(image_format_ > VPX_IMG_FMT_NONE);
  // Check if frame format has changed.
  if (ref_frame_ &&
      (decoded_image_.width() != static_cast<int>(ref_frame_->img.d_w) ||
          decoded_image_.height() != static_cast<int>(ref_frame_->img.d_h) ||
          image_format_ != ref_frame_->img.fmt)) {
    vpx_img_free(&ref_frame_->img);
    delete ref_frame_;
    ref_frame_ = NULL;
  }


  if (!ref_frame_) {
    ref_frame_ = new vpx_ref_frame_t;

    unsigned int align = 16;
    if (!vpx_img_alloc(&ref_frame_->img,
                       static_cast<vpx_img_fmt_t>(image_format_),
                       decoded_image_.width(), decoded_image_.height(),
                       align)) {
      assert(false);
      delete copy;
      return NULL;
    }
  }
  const vpx_ref_frame_type_t type_vec[] = { VP8_LAST_FRAME, VP8_GOLD_FRAME,
      VP8_ALTR_FRAME };
  for (uint32_t ix = 0;
      ix < sizeof(type_vec) / sizeof(vpx_ref_frame_type_t); ++ix) {
    ref_frame_->frame_type = type_vec[ix];
    if (CopyReference(copy) < 0) {
      delete copy;
      return NULL;
    }
  }
  // Copy all member variables (that are not set in initialization).
  copy->feedback_mode_ = feedback_mode_;
  copy->image_format_ = image_format_;
  copy->last_keyframe_ = last_keyframe_;  // Shallow copy.
  // Allocate memory. (Discard copied _buffer pointer.)
  copy->last_keyframe_._buffer = new uint8_t[last_keyframe_._size];
  memcpy(copy->last_keyframe_._buffer, last_keyframe_._buffer,
         last_keyframe_._length);

  return static_cast<VideoDecoder*>(copy);
}

int VP8DecoderImpl::CopyReference(VP8Decoder* copyTo) {
  // The type of frame to copy should be set in ref_frame_->frame_type
  // before the call to this function.
  if (vpx_codec_control(decoder_, VP8_COPY_REFERENCE, ref_frame_)
      != VPX_CODEC_OK) {
    return -1;
  }
  if (vpx_codec_control(static_cast<VP8DecoderImpl*>(copyTo)->decoder_,
                        VP8_SET_REFERENCE, ref_frame_) != VPX_CODEC_OK) {
    return -1;
  }
  return 0;
}

}  // namespace webrtc
