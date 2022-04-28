/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_
#define MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_

#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/fec_controller_override.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/framerate_controller.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// SimulcastEncoderAdapter implements simulcast support by creating multiple
// webrtc::VideoEncoder instances with the given VideoEncoderFactory.
// The object is created and destroyed on the worker thread, but all public
// interfaces should be called from the encoder task queue.
class RTC_EXPORT SimulcastEncoderAdapter : public VideoEncoder {
 public:
  // TODO(bugs.webrtc.org/11000): Remove when downstream usage is gone.
  SimulcastEncoderAdapter(VideoEncoderFactory* primarty_factory,
                          const SdpVideoFormat& format);
  // |primary_factory| produces the first-choice encoders to use.
  // |fallback_factory|, if non-null, is used to create fallback encoder that
  // will be used if InitEncode() fails for the primary encoder.
  SimulcastEncoderAdapter(VideoEncoderFactory* primary_factory,
                          VideoEncoderFactory* fallback_factory,
                          const SdpVideoFormat& format);
  ~SimulcastEncoderAdapter() override;

  // Implements VideoEncoder.
  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;
  int Release() override;
  int InitEncode(const VideoCodec* codec_settings,
                 const VideoEncoder::Settings& settings) override;
  int Encode(const VideoFrame& input_image,
             const std::vector<VideoFrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  void SetRates(const RateControlParameters& parameters) override;
  void OnPacketLossRateUpdate(float packet_loss_rate) override;
  void OnRttUpdate(int64_t rtt_ms) override;
  void OnLossNotification(const LossNotification& loss_notification) override;

  EncoderInfo GetEncoderInfo() const override;

 private:
  class EncoderContext : public EncodedImageCallback {
   public:
    EncoderContext(SimulcastEncoderAdapter* parent,
                   std::unique_ptr<VideoEncoder> encoder,
                   std::unique_ptr<FramerateController> framerate_controller,
                   int stream_idx,
                   uint16_t width,
                   uint16_t height,
                   bool send_stream);
    EncoderContext(EncoderContext&& rhs);
    EncoderContext& operator=(EncoderContext&&) = delete;
    ~EncoderContext() override;

    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info) override;
    void OnDroppedFrame(DropReason reason) override;

    VideoEncoder& encoder() { return *encoder_; }
    const VideoEncoder& encoder() const { return *encoder_; }
    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }
    bool needs_keyframe() const { return send_stream_ && needs_keyframe_; }
    void set_keyframe_needed() { needs_keyframe_ = true; }
    bool send_stream() const { return send_stream_; }
    void set_send_stream(bool send_stream) { send_stream_ = send_stream; }
    absl::optional<float> target_fps() const {
      return framerate_controller_ == nullptr
                 ? absl::nullopt
                 : absl::optional<float>(
                       framerate_controller_->GetTargetRate());
    }

    std::unique_ptr<VideoEncoder> Release() &&;
    void OnKeyframe(Timestamp timestamp);
    bool ShouldDropFrame(Timestamp timestamp);

   private:
    SimulcastEncoderAdapter* const parent_;
    std::unique_ptr<VideoEncoder> encoder_;
    std::unique_ptr<FramerateController> framerate_controller_;
    const int stream_idx_;
    const uint16_t width_;
    const uint16_t height_;
    bool needs_keyframe_;
    bool send_stream_;
  };

  enum class StreamResolution {
    OTHER,
    HIGHEST,
    LOWEST,
  };

  // Populate the codec settings for each simulcast stream.
  void PopulateStreamCodec(const webrtc::VideoCodec& inst,
                           int stream_index,
                           uint32_t start_bitrate_kbps,
                           StreamResolution stream_resolution,
                           webrtc::VideoCodec* stream_codec);

  bool Initialized() const;

  void DestroyStoredEncoders();

  EncodedImageCallback::Result OnEncodedImage(
      size_t stream_idx,
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info);

  void OnDroppedFrame(size_t stream_idx);

  volatile int inited_;  // Accessed atomically.
  VideoEncoderFactory* const primary_encoder_factory_;
  VideoEncoderFactory* const fallback_encoder_factory_;
  const SdpVideoFormat video_format_;
  VideoCodec codec_;
  std::vector<EncoderContext> encoder_contexts_;
  EncodedImageCallback* encoded_complete_callback_;
  size_t first_active_stream_idx_;
  size_t num_active_streams_;

  // Used for checking the single-threaded access of the encoder interface.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_queue_;

  // Store encoders in between calls to Release and InitEncode, so they don't
  // have to be recreated. Remaining encoders are destroyed by the destructor.
  std::stack<std::unique_ptr<VideoEncoder>> stored_encoders_;

  const absl::optional<unsigned int> experimental_boosted_screenshare_qp_;
  const bool boost_base_layer_quality_;
  const bool prefer_temporal_support_on_base_layer_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_
