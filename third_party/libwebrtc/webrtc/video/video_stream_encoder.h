/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_ENCODER_H_
#define VIDEO_VIDEO_STREAM_ENCODER_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/video/video_rotation.h"
#include "api/video_codecs/video_encoder.h"
#include "call/call.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/video_bitrate_allocator.h"
#include "media/base/videosinkinterface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "video/overuse_frame_detector.h"
#include "call/video_send_stream.h"

namespace webrtc {

class SendStatisticsProxy;
class VideoBitrateAllocationObserver;

// VideoStreamEncoder represent a video encoder that accepts raw video frames as
// input and produces an encoded bit stream.
// Usage:
//  Instantiate.
//  Call SetSink.
//  Call SetSource.
//  Call ConfigureEncoder with the codec settings.
//  Call Stop() when done.
class VideoStreamEncoder : public rtc::VideoSinkInterface<VideoFrame>,
                           public EncodedImageCallback,
                           public AdaptationObserverInterface {
 public:
  // Interface for receiving encoded video frames and notifications about
  // configuration changes.
  class EncoderSink : public EncodedImageCallback {
   public:
    virtual void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        int min_transmit_bitrate_bps) = 0;
  };

  // Number of resolution and framerate reductions (-1: disabled).
  struct AdaptCounts {
    int resolution = 0;
    int fps = 0;
  };

  // Downscale resolution at most 2 times for CPU reasons.
  static const int kMaxCpuResolutionDowngrades = 2;
  // Downscale framerate at most 4 times.
  static const int kMaxCpuFramerateDowngrades = 4;

  VideoStreamEncoder(uint32_t number_of_cores,
                     SendStatisticsProxy* stats_proxy,
                     const VideoSendStream::Config::EncoderSettings& settings,
                     rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
                     EncodedFrameObserver* encoder_timing,
                     std::unique_ptr<OveruseFrameDetector> overuse_detector);
  ~VideoStreamEncoder();

  // Sets the source that will provide I420 video frames.
  // |degradation_preference| control whether or not resolution or frame rate
  // may be reduced.
  void SetSource(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const VideoSendStream::DegradationPreference& degradation_preference);

  // Sets the |sink| that gets the encoded frames. |rotation_applied| means
  // that the source must support rotation. Only set |rotation_applied| if the
  // remote side does not support the rotation extension.
  void SetSink(EncoderSink* sink, bool rotation_applied);

  // TODO(perkj): Can we remove VideoCodec.startBitrate ?
  void SetStartBitrate(int start_bitrate_bps);

  void SetBitrateObserver(VideoBitrateAllocationObserver* bitrate_observer);

  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length,
                        bool nack_enabled);

  // Permanently stop encoding. After this method has returned, it is
  // guaranteed that no encoded frames will be delivered to the sink.
  void Stop();

  void SendKeyFrame();

  // virtual to test EncoderStateFeedback with mocks.
  virtual void OnReceivedIntraFrameRequest(size_t stream_index);

  void OnBitrateUpdated(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms);

 protected:
  // Used for testing. For example the |ScalingObserverInterface| methods must
  // be called on |encoder_queue_|.
  rtc::TaskQueue* encoder_queue() { return &encoder_queue_; }

  // AdaptationObserverInterface implementation.
  // These methods are protected for easier testing.
  void AdaptUp(AdaptReason reason) override;
  void AdaptDown(AdaptReason reason) override;
  static CpuOveruseOptions GetCpuOveruseOptions(bool full_overuse_time);

 private:
  class ConfigureEncoderTask;
  class EncodeTask;
  class VideoSourceProxy;

  class VideoFrameInfo {
   public:
    VideoFrameInfo(int width,
                   int height,
                   bool is_texture)
        : width(width),
          height(height),
          is_texture(is_texture) {}
    int width;
    int height;
    bool is_texture;
    int pixel_count() const { return width * height; }
  };

  void ConfigureEncoderOnTaskQueue(VideoEncoderConfig config,
                                   size_t max_data_payload_length,
                                   bool nack_enabled);
  void ReconfigureEncoder();

  void ConfigureQualityScaler();

  // Implements VideoSinkInterface.
  void OnFrame(const VideoFrame& video_frame) override;
  void OnDiscardedFrame() override;

  void EncodeVideoFrame(const VideoFrame& frame,
                        int64_t time_when_posted_in_ms);

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void OnDroppedFrame(EncodedImageCallback::DropReason reason) override;

  bool EncoderPaused() const;
  void TraceFrameDropStart();
  void TraceFrameDropEnd();

  // Class holding adaptation information.
  class AdaptCounter final {
   public:
    AdaptCounter();
    ~AdaptCounter();

    // Get number of adaptation downscales for |reason|.
    AdaptCounts Counts(int reason) const;

    std::string ToString() const;

    void IncrementFramerate(int reason);
    void IncrementResolution(int reason);
    void DecrementFramerate(int reason);
    void DecrementResolution(int reason);
    void DecrementFramerate(int reason, int cur_fps);

    // Gets the total number of downgrades (for all adapt reasons).
    int FramerateCount() const;
    int ResolutionCount() const;

    // Gets the total number of downgrades for |reason|.
    int FramerateCount(int reason) const;
    int ResolutionCount(int reason) const;
    int TotalCount(int reason) const;

   private:
    std::string ToString(const std::vector<int>& counters) const;
    int Count(const std::vector<int>& counters) const;
    void MoveCount(std::vector<int>* counters, int from_reason);

    // Degradation counters holding number of framerate/resolution reductions
    // per adapt reason.
    std::vector<int> fps_counters_;
    std::vector<int> resolution_counters_;
  };

  AdaptCounter& GetAdaptCounter() RTC_RUN_ON(&encoder_queue_);
  const AdaptCounter& GetConstAdaptCounter() RTC_RUN_ON(&encoder_queue_);
  void UpdateAdaptationStats(AdaptReason reason) RTC_RUN_ON(&encoder_queue_);
  AdaptCounts GetActiveCounts(AdaptReason reason) RTC_RUN_ON(&encoder_queue_);

  rtc::Event shutdown_event_;

  const uint32_t number_of_cores_;
  // Counts how many frames we've dropped in the initial rampup phase.
  int initial_rampup_;

  const std::unique_ptr<VideoSourceProxy> source_proxy_;
  EncoderSink* sink_;
  const VideoSendStream::Config::EncoderSettings settings_;
  const VideoCodecType codec_type_;

  vcm::VideoSender video_sender_ RTC_ACCESS_ON(&encoder_queue_);
  std::unique_ptr<OveruseFrameDetector> overuse_detector_
      RTC_ACCESS_ON(&encoder_queue_);
  std::unique_ptr<QualityScaler> quality_scaler_ RTC_ACCESS_ON(&encoder_queue_);

  SendStatisticsProxy* const stats_proxy_;
  rtc::VideoSinkInterface<VideoFrame>* const pre_encode_callback_;
  // |thread_checker_| checks that public methods that are related to lifetime
  // of VideoStreamEncoder are called on the same thread.
  rtc::ThreadChecker thread_checker_;

  VideoEncoderConfig encoder_config_ RTC_ACCESS_ON(&encoder_queue_);
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_
      RTC_ACCESS_ON(&encoder_queue_);
  // The maximum frame rate of the current codec configuration, as determined
  // at the last ReconfigureEncoder() call.
  int max_framerate_ RTC_ACCESS_ON(&encoder_queue_);

  // Set when ConfigureEncoder has been called in order to lazy reconfigure the
  // encoder on the next frame.
  bool pending_encoder_reconfiguration_ RTC_ACCESS_ON(&encoder_queue_);
  rtc::Optional<VideoFrameInfo> last_frame_info_ RTC_ACCESS_ON(&encoder_queue_);
  int crop_width_ RTC_ACCESS_ON(&encoder_queue_);
  int crop_height_ RTC_ACCESS_ON(&encoder_queue_);
  uint32_t encoder_start_bitrate_bps_ RTC_ACCESS_ON(&encoder_queue_);
  size_t max_data_payload_length_ RTC_ACCESS_ON(&encoder_queue_);
  bool nack_enabled_ RTC_ACCESS_ON(&encoder_queue_);
  uint32_t last_observed_bitrate_bps_ RTC_ACCESS_ON(&encoder_queue_);
  bool encoder_paused_and_dropped_frame_ RTC_ACCESS_ON(&encoder_queue_);
  Clock* const clock_;
  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const VideoSendStream::DegradationPreference, AdaptCounter>
      adapt_counters_ RTC_ACCESS_ON(&encoder_queue_);
  // Set depending on degradation preferences.
  VideoSendStream::DegradationPreference degradation_preference_
      RTC_ACCESS_ON(&encoder_queue_);

  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;
  };
  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  rtc::Optional<AdaptationRequest> last_adaptation_request_
      RTC_ACCESS_ON(&encoder_queue_);

  rtc::RaceChecker incoming_frame_race_checker_
      RTC_GUARDED_BY(incoming_frame_race_checker_);
  std::atomic<int> posted_frames_waiting_for_encode_;
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_ RTC_GUARDED_BY(incoming_frame_race_checker_);
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_
      RTC_GUARDED_BY(incoming_frame_race_checker_);

  int64_t last_frame_log_ms_ RTC_GUARDED_BY(incoming_frame_race_checker_);
  int captured_frame_count_ RTC_ACCESS_ON(&encoder_queue_);
  int dropped_frame_count_ RTC_ACCESS_ON(&encoder_queue_);

  VideoBitrateAllocationObserver* bitrate_observer_
      RTC_ACCESS_ON(&encoder_queue_);
  rtc::Optional<int64_t> last_parameters_update_ms_
      RTC_ACCESS_ON(&encoder_queue_);

  // All public methods are proxied to |encoder_queue_|. It must must be
  // destroyed first to make sure no tasks are run that use other members.
  rtc::TaskQueue encoder_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoStreamEncoder);
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_ENCODER_H_
