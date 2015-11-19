/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_
#define WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Error codes
#define VCM_FRAME_NOT_READY      3
#define VCM_REQUEST_SLI          2
#define VCM_MISSING_CALLBACK     1
#define VCM_OK                   0
#define VCM_GENERAL_ERROR       -1
#define VCM_LEVEL_EXCEEDED      -2
#define VCM_MEMORY              -3
#define VCM_PARAMETER_ERROR     -4
#define VCM_UNKNOWN_PAYLOAD     -5
#define VCM_CODEC_ERROR         -6
#define VCM_UNINITIALIZED       -7
#define VCM_NO_CODEC_REGISTERED -8
#define VCM_JITTER_BUFFER_ERROR -9
#define VCM_OLD_PACKET_ERROR    -10
#define VCM_NO_FRAME_DECODED    -11
#define VCM_ERROR_REQUEST_SLI   -12
#define VCM_NOT_IMPLEMENTED     -20

#define VCM_RED_PAYLOAD_TYPE        96
#define VCM_ULPFEC_PAYLOAD_TYPE     97
#define VCM_VP8_PAYLOAD_TYPE       100
#define VCM_VP9_PAYLOAD_TYPE       101
#define VCM_I420_PAYLOAD_TYPE      124
#define VCM_H264_PAYLOAD_TYPE      127

enum { kDefaultStartBitrateKbps = 300 };

enum VCMVideoProtection {
  kProtectionNone,
  kProtectionNack,                // Both send-side and receive-side
  kProtectionNackSender,          // Send-side only
  kProtectionNackReceiver,        // Receive-side only
  kProtectionFEC,
  kProtectionNackFEC,
  kProtectionKeyOnLoss,
  kProtectionKeyOnKeyLoss,
};

enum VCMTemporalDecimation {
  kBitrateOverUseDecimation,
};

struct VCMFrameCount {
  uint32_t numKeyFrames;
  uint32_t numDeltaFrames;
};

// Callback class used for sending data ready to be packetized
class VCMPacketizationCallback {
 public:
  virtual int32_t SendData(uint8_t payloadType,
                           const EncodedImage& encoded_image,
                           const RTPFragmentationHeader& fragmentationHeader,
                           const RTPVideoHeader* rtpVideoHdr) = 0;

 protected:
  virtual ~VCMPacketizationCallback() {
  }
};

// Callback class used for passing decoded frames which are ready to be rendered.
class VCMReceiveCallback {
 public:
  virtual int32_t FrameToRender(I420VideoFrame& videoFrame) = 0;
  virtual int32_t ReceivedDecodedReferenceFrame(
      const uint64_t pictureId) {
    return -1;
  }
  // Called when the current receive codec changes.
  virtual void IncomingCodecChanged(const VideoCodec& codec) {}

 protected:
  virtual ~VCMReceiveCallback() {
  }
};

// Callback class used for informing the user of the bit rate and frame rate produced by the
// encoder.
class VCMSendStatisticsCallback {
 public:
  virtual int32_t SendStatistics(const uint32_t bitRate,
                                       const uint32_t frameRate) = 0;

 protected:
  virtual ~VCMSendStatisticsCallback() {
  }
};

// Callback class used for informing the user of the incoming bit rate and frame rate.
class VCMReceiveStatisticsCallback {
 public:
  virtual void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) = 0;
  virtual void OnDiscardedPacketsUpdated(int discarded_packets) = 0;
  virtual void OnFrameCountsUpdated(const FrameCounts& frame_counts) = 0;

 protected:
  virtual ~VCMReceiveStatisticsCallback() {
  }
};

// Callback class used for informing the user of decode timing info.
class VCMDecoderTimingCallback {
 public:
  virtual void OnDecoderTiming(int decode_ms,
                               int max_decode_ms,
                               int current_delay_ms,
                               int target_delay_ms,
                               int jitter_buffer_ms,
                               int min_playout_delay_ms,
                               int render_delay_ms) = 0;

 protected:
  virtual ~VCMDecoderTimingCallback() {}
};

// Callback class used for telling the user about how to configure the FEC,
// and the rates sent the last second is returned to the VCM.
class VCMProtectionCallback {
 public:
  virtual int ProtectionRequest(const FecProtectionParams* delta_params,
                                const FecProtectionParams* key_params,
                                uint32_t* sent_video_rate_bps,
                                uint32_t* sent_nack_rate_bps,
                                uint32_t* sent_fec_rate_bps) = 0;

 protected:
  virtual ~VCMProtectionCallback() {
  }
};

class VideoEncoderRateObserver {
 public:
  virtual ~VideoEncoderRateObserver() {}
  virtual void OnSetRates(uint32_t bitrate_bps, int framerate) = 0;
};

// Callback class used for telling the user about what frame type needed to continue decoding.
// Typically a key frame when the stream has been corrupted in some way.
class VCMFrameTypeCallback {
 public:
  virtual int32_t RequestKeyFrame() = 0;
  virtual int32_t SliceLossIndicationRequest(
      const uint64_t pictureId) {
    return -1;
  }

 protected:
  virtual ~VCMFrameTypeCallback() {
  }
};

// Callback class used for telling the user about which packet sequence numbers are currently
// missing and need to be resent.
class VCMPacketRequestCallback {
 public:
  virtual int32_t ResendPackets(const uint16_t* sequenceNumbers,
                                      uint16_t length) = 0;

 protected:
  virtual ~VCMPacketRequestCallback() {
  }
};

// Callback class used for telling the user about the state of the decoder & jitter buffer.
//
class VCMReceiveStateCallback {
 public:
  virtual void ReceiveStateChange(VideoReceiveState state) = 0;

 protected:
  virtual ~VCMReceiveStateCallback() {
  }
};

// Callback used to inform the user of the the desired resolution
// as subscribed by Media Optimization (Quality Modes)
class VCMQMSettingsCallback {
 public:
  virtual int32_t SetVideoQMSettings(const uint32_t frameRate,
                                           const uint32_t width,
                                           const uint32_t height) = 0;

 protected:
  virtual ~VCMQMSettingsCallback() {
  }
};

// Callback class used for telling the user about the size (in time) of the
// render buffer, that is the size in time of the complete continuous frames.
class VCMRenderBufferSizeCallback {
 public:
  virtual void RenderBufferSizeMs(int buffer_size_ms) = 0;

 protected:
  virtual ~VCMRenderBufferSizeCallback() {
  }
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_
