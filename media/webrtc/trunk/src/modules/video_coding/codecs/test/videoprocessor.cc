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
      initialized_(false) {
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
  source_frame_._width = config_.codec_settings->width;
  source_frame_._height = config_.codec_settings->height;
  source_frame_._length = frame_length_in_bytes;
  source_frame_._size = frame_length_in_bytes;

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

bool VideoProcessorImpl::ProcessFrame(int frame_number) {
  assert(frame_number >=0);
  if (!initialized_) {
    fprintf(stderr, "Attempting to use uninitialized VideoProcessor!\n");
    return false;
  }
  if (frame_reader_->ReadFrame(source_buffer_)) {
    // point the source frame buffer to the newly read frame data:
    source_frame_._buffer = source_buffer_;

    // Ensure we have a new statistics data object we can fill:
    FrameStatistic& stat = stats_->NewFrame(frame_number);

    encode_start_ = TickTime::Now();
    // Use the frame number as "timestamp" to identify frames
    source_frame_._timeStamp = frame_number;

    // Decide if we're going to force a keyframe:
    VideoFrameType frame_type = kDeltaFrame;
    if (config_.keyframe_interval > 0 &&
        frame_number % config_.keyframe_interval == 0) {
      frame_type = kKeyFrame;
    }
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

void VideoProcessorImpl::FrameDecoded(const RawImage& image) {
  TickTime decode_stop = TickTime::Now();
  int frame_number = image._timeStamp;
  // Report stats
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.decode_time_in_us = GetElapsedTimeMicroseconds(decode_start_,
                                                      decode_stop);
  stat.decoding_successful = true;
  // Update our copy of the last successful frame:
  memcpy(last_successful_frame_buffer_, image._buffer, image._length);

  bool write_success = frame_writer_->WriteFrame(image._buffer);
  if (!write_success) {
    fprintf(stderr, "Failed to write frame %d to disk!", frame_number);
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
  video_processor_->FrameEncoded(&encoded_image);  // forward to parent class
  return 0;
}
WebRtc_Word32
VideoProcessorImpl::VideoProcessorDecodeCompleteCallback::Decoded(
    RawImage& image) {
  video_processor_->FrameDecoded(image);  // forward to parent class
  return 0;
}

}  // namespace test
}  // namespace webrtc
