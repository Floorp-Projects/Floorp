/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor.h"

#include <cassert>
#include <cstring>
#include <limits>

#include "system_wrappers/interface/cpu_info.h"

namespace webrtc {
namespace test {

VideoProcessorImpl::VideoProcessorImpl(webrtc::VideoEncoder* encoder,
                                       webrtc::VideoDecoder* decoder,
                                       FrameReader* frame_reader,
                                       FrameWriter* frame_writer,
                                       PacketManipulator* packet_manipulator,
                                       const TestConfig& config,
                                       Stats* stats)
    : encoder_(encoder),
      decoder_(decoder),
      frame_reader_(frame_reader),
      frame_writer_(frame_writer),
      packet_manipulator_(packet_manipulator),
      config_(config),
      stats_(stats),
      encode_callback_(NULL),
      decode_callback_(NULL),
      source_buffer_(NULL),
      first_key_frame_has_been_excluded_(false),
      last_frame_missing_(false),
      initialized_(false),
      encoded_frame_size_(0),
      prev_time_stamp_(0),
      num_dropped_frames_(0),
      num_spatial_resizes_(0),
      last_encoder_frame_width_(0),
      last_encoder_frame_height_(0),
      scaler_() {
  assert(encoder);
  assert(decoder);
  assert(frame_reader);
  assert(frame_writer);
  assert(packet_manipulator);
  assert(stats);
}

bool VideoProcessorImpl::Init() {
  // Calculate a factor used for bit rate calculations:
  bit_rate_factor_ = config_.codec_settings->maxFramerate * 0.001 * 8;  // bits

  int frame_length_in_bytes = frame_reader_->FrameLength();

  // Initialize data structures used by the encoder/decoder APIs
  source_buffer_ = new WebRtc_UWord8[frame_length_in_bytes];
  last_successful_frame_buffer_ = new WebRtc_UWord8[frame_length_in_bytes];

  // Set fixed properties common for all frames:
  source_frame_.SetWidth(config_.codec_settings->width);
  source_frame_.SetHeight(config_.codec_settings->height);
  source_frame_.VerifyAndAllocate(frame_length_in_bytes);
  source_frame_.SetLength(frame_length_in_bytes);

  // To keep track of spatial resize actions by encoder.
  last_encoder_frame_width_ = config_.codec_settings->width;
  last_encoder_frame_height_ = config_.codec_settings->height;

  // Setup required callbacks for the encoder/decoder:
  encode_callback_ = new VideoProcessorEncodeCompleteCallback(this);
  decode_callback_ = new VideoProcessorDecodeCompleteCallback(this);
  WebRtc_Word32 register_result =
      encoder_->RegisterEncodeCompleteCallback(encode_callback_);
  if (register_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to register encode complete callback, return code: "
        "%d\n", register_result);
    return false;
  }
  register_result = decoder_->RegisterDecodeCompleteCallback(decode_callback_);
  if (register_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to register decode complete callback, return code: "
            "%d\n", register_result);
    return false;
  }
  // Init the encoder and decoder
  WebRtc_UWord32 nbr_of_cores = 1;
  if (!config_.use_single_core) {
    nbr_of_cores = CpuInfo::DetectNumberOfCores();
  }
  WebRtc_Word32 init_result =
      encoder_->InitEncode(config_.codec_settings, nbr_of_cores,
                           config_.networking_config.max_payload_size_in_bytes);
  if (init_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to initialize VideoEncoder, return code: %d\n",
            init_result);
    return false;
  }
  init_result = decoder_->InitDecode(config_.codec_settings, nbr_of_cores);
  if (init_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to initialize VideoDecoder, return code: %d\n",
            init_result);
    return false;
  }

  if (config_.verbose) {
    printf("Video Processor:\n");
    printf("  #CPU cores used  : %d\n", nbr_of_cores);
    printf("  Total # of frames: %d\n", frame_reader_->NumberOfFrames());
    printf("  Codec settings:\n");
    printf("    Start bitrate  : %d kbps\n",
           config_.codec_settings->startBitrate);
    printf("    Width          : %d\n", config_.codec_settings->width);
    printf("    Height         : %d\n", config_.codec_settings->height);
  }
  initialized_ = true;
  return true;
}

VideoProcessorImpl::~VideoProcessorImpl() {
  delete[] source_buffer_;
  delete[] last_successful_frame_buffer_;
  encoder_->RegisterEncodeCompleteCallback(NULL);
  delete encode_callback_;
  decoder_->RegisterDecodeCompleteCallback(NULL);
  delete decode_callback_;
}


void VideoProcessorImpl::SetRates(int bit_rate, int frame_rate) {
  int set_rates_result = encoder_->SetRates(bit_rate, frame_rate);
  assert(set_rates_result >= 0);
  if (set_rates_result < 0) {
    fprintf(stderr, "Failed to update encoder with new rate %d, "
            "return code: %d\n", bit_rate, set_rates_result);
  }
  num_dropped_frames_ = 0;
  num_spatial_resizes_ = 0;
}

int VideoProcessorImpl::EncodedFrameSize() {
  return encoded_frame_size_;
}

int VideoProcessorImpl::NumberDroppedFrames() {
  return num_dropped_frames_;
}

int VideoProcessorImpl::NumberSpatialResizes() {
  return num_spatial_resizes_;
}

bool VideoProcessorImpl::ProcessFrame(int frame_number) {
  assert(frame_number >=0);
  if (!initialized_) {
    fprintf(stderr, "Attempting to use uninitialized VideoProcessor!\n");
    return false;
  }
  // |prev_time_stamp_| is used for getting number of dropped frames.
  if (frame_number == 0) {
    prev_time_stamp_ = -1;
  }
  if (frame_reader_->ReadFrame(source_buffer_)) {
    // Copy the source frame to the newly read frame data.
    // Length is common for all frames.
    source_frame_.CopyFrame(source_frame_.Length(), source_buffer_);

    // Ensure we have a new statistics data object we can fill:
    FrameStatistic& stat = stats_->NewFrame(frame_number);

    encode_start_ = TickTime::Now();
    // Use the frame number as "timestamp" to identify frames
    source_frame_.SetTimeStamp(frame_number);

    // Decide if we're going to force a keyframe:
    VideoFrameType frame_type = kDeltaFrame;
    if (config_.keyframe_interval > 0 &&
        frame_number % config_.keyframe_interval == 0) {
      frame_type = kKeyFrame;
    }

    // For dropped frames, we regard them as zero size encoded frames.
    encoded_frame_size_ = 0;

    WebRtc_Word32 encode_result = encoder_->Encode(source_frame_, NULL,
                                                   frame_type);

    if (encode_result != WEBRTC_VIDEO_CODEC_OK) {
      fprintf(stderr, "Failed to encode frame %d, return code: %d\n",
              frame_number, encode_result);
    }
    stat.encode_return_code = encode_result;
    return true;
  } else {
    return false;  // we've reached the last frame
  }
}

void VideoProcessorImpl::FrameEncoded(EncodedImage* encoded_image) {
  // Timestamp is frame number, so this gives us #dropped frames.
  int num_dropped_from_prev_encode =  encoded_image->_timeStamp -
      prev_time_stamp_ - 1;
  num_dropped_frames_ +=  num_dropped_from_prev_encode;
  prev_time_stamp_ =  encoded_image->_timeStamp;
  if (num_dropped_from_prev_encode > 0) {
    // For dropped frames, we write out the last decoded frame to avoid getting
    // out of sync for the computation of PSNR and SSIM.
    for (int i = 0; i < num_dropped_from_prev_encode; i++) {
      frame_writer_->WriteFrame(last_successful_frame_buffer_);
    }
  }
  // Frame is not dropped, so update the encoded frame size
  // (encoder callback is only called for non-zero length frames).
  encoded_frame_size_ = encoded_image->_length;

  TickTime encode_stop = TickTime::Now();
  int frame_number = encoded_image->_timeStamp;
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.encode_time_in_us = GetElapsedTimeMicroseconds(encode_start_,
                                                      encode_stop);
  stat.encoding_successful = true;
  stat.encoded_frame_length_in_bytes = encoded_image->_length;
  stat.frame_number = encoded_image->_timeStamp;
  stat.frame_type = encoded_image->_frameType;
  stat.bit_rate_in_kbps = encoded_image->_length * bit_rate_factor_;
  stat.total_packets = encoded_image->_length /
      config_.networking_config.packet_size_in_bytes + 1;

  // Perform packet loss if criteria is fullfilled:
  bool exclude_this_frame = false;
  // Only keyframes can be excluded
  if (encoded_image->_frameType == kKeyFrame) {
    switch (config_.exclude_frame_types) {
      case kExcludeOnlyFirstKeyFrame:
        if (!first_key_frame_has_been_excluded_) {
          first_key_frame_has_been_excluded_ = true;
          exclude_this_frame = true;
        }
        break;
      case kExcludeAllKeyFrames:
        exclude_this_frame = true;
        break;
      default:
        assert(false);
    }
  }
  if (!exclude_this_frame) {
    stat.packets_dropped =
          packet_manipulator_->ManipulatePackets(encoded_image);
  }

  // Keep track of if frames are lost due to packet loss so we can tell
  // this to the encoder (this is handled by the RTP logic in the full stack)
  decode_start_ = TickTime::Now();
  // TODO(kjellander): Pass fragmentation header to the decoder when
  // CL 172001 has been submitted and PacketManipulator supports this.
  WebRtc_Word32 decode_result = decoder_->Decode(*encoded_image,
                                                 last_frame_missing_, NULL);
  stat.decode_return_code = decode_result;
  if (decode_result != WEBRTC_VIDEO_CODEC_OK) {
    // Write the last successful frame the output file to avoid getting it out
    // of sync with the source file for SSIM and PSNR comparisons:
    frame_writer_->WriteFrame(last_successful_frame_buffer_);
  }
  // save status for losses so we can inform the decoder for the next frame:
  last_frame_missing_ = encoded_image->_length == 0;
}

void VideoProcessorImpl::FrameDecoded(const VideoFrame& image) {
  TickTime decode_stop = TickTime::Now();
  int frame_number = image.TimeStamp();
  // Report stats
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.decode_time_in_us = GetElapsedTimeMicroseconds(decode_start_,
                                                      decode_stop);
  stat.decoding_successful = true;

  // Check for resize action (either down or up):
  if (static_cast<int>(image.Width()) != last_encoder_frame_width_ ||
      static_cast<int>(image.Height()) != last_encoder_frame_height_ ) {
    ++num_spatial_resizes_;
    last_encoder_frame_width_ = image.Width();
    last_encoder_frame_height_ = image.Height();
  }
  // Check if codec size is different from native/original size, and if so,
  // upsample back to original size: needed for PSNR and SSIM computations.
  if (image.Width() !=  config_.codec_settings->width ||
      image.Height() != config_.codec_settings->height) {
    int required_size = CalcBufferSize(kI420,
                                       config_.codec_settings->width,
                                       config_.codec_settings->height);
    VideoFrame up_image;
    up_image.VerifyAndAllocate(required_size);
    up_image.SetLength(required_size);
    up_image.SetWidth(config_.codec_settings->width);
    up_image.SetHeight(config_.codec_settings->height);

    int ret_val = scaler_.Set(image.Width(), image.Height(),
                              config_.codec_settings->width,
                              config_.codec_settings->height,
                              kI420, kI420, kScaleBilinear);
    assert(ret_val >= 0);
    if (ret_val < 0) {
      fprintf(stderr, "Failed to set scalar for frame: %d, return code: %d\n",
              frame_number, ret_val);
    }
    ret_val = scaler_.Scale(image.Buffer(), up_image.Buffer(),
                            required_size);
    assert(ret_val >= 0);
    if (ret_val < 0) {
      fprintf(stderr, "Failed to scale frame: %d, return code: %d\n",
              frame_number, ret_val);
    }
    // Update our copy of the last successful frame:
    memcpy(last_successful_frame_buffer_, up_image.Buffer(), up_image.Length());

    bool write_success = frame_writer_->WriteFrame(up_image.Buffer());
    assert(write_success);
    if (!write_success) {
      fprintf(stderr, "Failed to write frame %d to disk!", frame_number);
    }
    up_image.Free();
  } else {  // No resize.
    // Update our copy of the last successful frame:
    memcpy(last_successful_frame_buffer_, image.Buffer(), image.Length());

    bool write_success = frame_writer_->WriteFrame(image.Buffer());
    assert(write_success);
    if (!write_success) {
      fprintf(stderr, "Failed to write frame %d to disk!", frame_number);
    }
  }
}

int VideoProcessorImpl::GetElapsedTimeMicroseconds(
    const webrtc::TickTime& start, const webrtc::TickTime& stop) {
  WebRtc_UWord64 encode_time = (stop - start).Microseconds();
  assert(encode_time <
         static_cast<unsigned int>(std::numeric_limits<int>::max()));
  return static_cast<int>(encode_time);
}

const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e) {
  switch (e) {
    case kExcludeOnlyFirstKeyFrame:
      return "ExcludeOnlyFirstKeyFrame";
    case kExcludeAllKeyFrames:
      return "ExcludeAllKeyFrames";
    default:
      assert(false);
      return "Unknown";
  }
}

const char* VideoCodecTypeToStr(webrtc::VideoCodecType e) {
  switch (e) {
    case kVideoCodecVP8:
      return "VP8";
    case kVideoCodecI420:
      return "I420";
    case kVideoCodecRED:
      return "RED";
    case kVideoCodecULPFEC:
      return "ULPFEC";
    case kVideoCodecUnknown:
      return "Unknown";
    default:
      assert(false);
      return "Unknown";
  }
}

// Callbacks
WebRtc_Word32
VideoProcessorImpl::VideoProcessorEncodeCompleteCallback::Encoded(
    EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  video_processor_->FrameEncoded(&encoded_image);  // Forward to parent class.
  return 0;
}
WebRtc_Word32
VideoProcessorImpl::VideoProcessorDecodeCompleteCallback::Decoded(
    VideoFrame& image) {
  video_processor_->FrameDecoded(image);  // forward to parent class
  return 0;
}

}  // namespace test
}  // namespace webrtc
