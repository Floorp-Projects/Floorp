/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"
#include "test/pc/e2e/analyzer/video/multi_head_queue.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {

class DefaultVideoQualityAnalyzer : public VideoQualityAnalyzerInterface {
 public:
  explicit DefaultVideoQualityAnalyzer(
      webrtc::Clock* clock,
      DefaultVideoQualityAnalyzerOptions options = {});
  ~DefaultVideoQualityAnalyzer() override;

  void Start(std::string test_case_name,
             rtc::ArrayView<const std::string> peer_names,
             int max_threads_count) override;
  uint16_t OnFrameCaptured(absl::string_view peer_name,
                           const std::string& stream_label,
                           const VideoFrame& frame) override;
  void OnFramePreEncode(absl::string_view peer_name,
                        const VideoFrame& frame) override;
  void OnFrameEncoded(absl::string_view peer_name,
                      uint16_t frame_id,
                      const EncodedImage& encoded_image,
                      const EncoderStats& stats) override;
  void OnFrameDropped(absl::string_view peer_name,
                      EncodedImageCallback::DropReason reason) override;
  void OnFramePreDecode(absl::string_view peer_name,
                        uint16_t frame_id,
                        const EncodedImage& input_image) override;
  void OnFrameDecoded(absl::string_view peer_name,
                      const VideoFrame& frame,
                      const DecoderStats& stats) override;
  void OnFrameRendered(absl::string_view peer_name,
                       const VideoFrame& frame) override;
  void OnEncoderError(absl::string_view peer_name,
                      const VideoFrame& frame,
                      int32_t error_code) override;
  void OnDecoderError(absl::string_view peer_name,
                      uint16_t frame_id,
                      int32_t error_code) override;
  void RegisterParticipantInCall(absl::string_view peer_name) override;
  void Stop() override;
  std::string GetStreamLabel(uint16_t frame_id) override;
  void OnStatsReports(
      absl::string_view pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {}

  // Returns set of stream labels, that were met during test call.
  std::set<StatsKey> GetKnownVideoStreams() const;
  VideoStreamsInfo GetKnownStreams() const;
  FrameCounters GetGlobalCounters() const;
  // Returns frame counter per stream label. Valid stream labels can be obtained
  // by calling GetKnownVideoStreams()
  std::map<StatsKey, FrameCounters> GetPerStreamCounters() const;
  // Returns video quality stats per stream label. Valid stream labels can be
  // obtained by calling GetKnownVideoStreams()
  std::map<StatsKey, StreamStats> GetStats() const;
  AnalyzerStats GetAnalyzerStats() const;
  double GetCpuUsagePercent();

  // Returns mapping from the stream label to the history of frames that were
  // met in this stream in the order as they were captured.
  std::map<std::string, std::vector<uint16_t>> GetStreamFrames() const;

 private:
  // Represents a current state of video stream.
  class StreamState {
   public:
    StreamState(size_t owner,
                size_t peers_count,
                bool enable_receive_own_stream,
                Timestamp stream_started_time)
        : owner_(owner),
          enable_receive_own_stream_(enable_receive_own_stream),
          stream_started_time_(stream_started_time),
          frame_ids_(enable_receive_own_stream ? peers_count + 1
                                               : peers_count) {}

    size_t owner() const { return owner_; }
    Timestamp stream_started_time() const { return stream_started_time_; }

    void PushBack(uint16_t frame_id) { frame_ids_.PushBack(frame_id); }
    // Crash if state is empty. Guarantees that there can be no alive frames
    // that are not in the owner queue
    uint16_t PopFront(size_t peer);
    bool IsEmpty(size_t peer) const {
      return frame_ids_.IsEmpty(GetPeerQueueIndex(peer));
    }
    // Crash if state is empty.
    uint16_t Front(size_t peer) const {
      return frame_ids_.Front(GetPeerQueueIndex(peer)).value();
    }

    // When new peer is added - all current alive frames will be sent to it as
    // well. So we need to register them as expected by copying owner_ head to
    // the new head.
    void AddPeer() { frame_ids_.AddHead(GetAliveFramesQueueIndex()); }

    size_t GetAliveFramesCount() const {
      return frame_ids_.size(GetAliveFramesQueueIndex());
    }
    uint16_t MarkNextAliveFrameAsDead();

    void SetLastRenderedFrameTime(size_t peer, Timestamp time);
    absl::optional<Timestamp> last_rendered_frame_time(size_t peer) const;

   private:
    // Returns index of the `frame_ids_` queue which is used for specified
    // `peer_index`.
    size_t GetPeerQueueIndex(size_t peer_index) const;

    // Returns index of the `frame_ids_` queue which is used to track alive
    // frames for this stream. The frame is alive if it contains VideoFrame
    // payload in `captured_frames_in_flight_`.
    size_t GetAliveFramesQueueIndex() const;

    // Index of the owner. Owner's queue in `frame_ids_` will keep alive frames.
    const size_t owner_;
    const bool enable_receive_own_stream_;
    const Timestamp stream_started_time_;
    // To correctly determine dropped frames we have to know sequence of frames
    // in each stream so we will keep a list of frame ids inside the stream.
    // This list is represented by multi head queue of frame ids with separate
    // head for each receiver. When the frame is rendered, we will pop ids from
    // the corresponding head until id will match with rendered one. All ids
    // before matched one can be considered as dropped:
    //
    // | frame_id1 |->| frame_id2 |->| frame_id3 |->| frame_id4 |
    //
    // If we received frame with id frame_id3, then we will pop frame_id1 and
    // frame_id2 and consider that frames as dropped and then compare received
    // frame with the one from `captured_frames_in_flight_` with id frame_id3.
    MultiHeadQueue<uint16_t> frame_ids_;
    std::map<size_t, Timestamp> last_rendered_frame_time_;
  };

  enum State { kNew, kActive, kStopped };

  struct ReceiverFrameStats {
    // Time when last packet of a frame was received.
    Timestamp received_time = Timestamp::MinusInfinity();
    Timestamp decode_start_time = Timestamp::MinusInfinity();
    Timestamp decode_end_time = Timestamp::MinusInfinity();
    Timestamp rendered_time = Timestamp::MinusInfinity();
    Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

    absl::optional<int> rendered_frame_width = absl::nullopt;
    absl::optional<int> rendered_frame_height = absl::nullopt;

    // Can be not set if frame was dropped in the network.
    absl::optional<StreamCodecInfo> used_decoder = absl::nullopt;

    bool dropped = false;
  };

  class FrameInFlight {
   public:
    FrameInFlight(size_t stream,
                  VideoFrame frame,
                  Timestamp captured_time,
                  size_t owner,
                  size_t peers_count,
                  bool enable_receive_own_stream)
        : stream_(stream),
          owner_(owner),
          peers_count_(peers_count),
          enable_receive_own_stream_(enable_receive_own_stream),
          frame_(std::move(frame)),
          captured_time_(captured_time) {}

    size_t stream() const { return stream_; }
    const absl::optional<VideoFrame>& frame() const { return frame_; }
    // Returns was frame removed or not.
    bool RemoveFrame();
    void SetFrameId(uint16_t id);

    void AddPeer() { ++peers_count_; }

    std::vector<size_t> GetPeersWhichDidntReceive() const;
    bool HaveAllPeersReceived() const;

    void SetPreEncodeTime(webrtc::Timestamp time) { pre_encode_time_ = time; }

    void OnFrameEncoded(webrtc::Timestamp time,
                        int64_t encoded_image_size,
                        uint32_t target_encode_bitrate,
                        StreamCodecInfo used_encoder);

    bool HasEncodedTime() const { return encoded_time_.IsFinite(); }

    void OnFramePreDecode(size_t peer,
                          webrtc::Timestamp received_time,
                          webrtc::Timestamp decode_start_time);

    bool HasReceivedTime(size_t peer) const;

    void OnFrameDecoded(size_t peer,
                        webrtc::Timestamp time,
                        StreamCodecInfo used_decoder);

    bool HasDecodeEndTime(size_t peer) const;

    void OnFrameRendered(size_t peer,
                         webrtc::Timestamp time,
                         int width,
                         int height);

    bool HasRenderedTime(size_t peer) const;

    // Crash if rendered time is not set for specified `peer`.
    webrtc::Timestamp rendered_time(size_t peer) const {
      return receiver_stats_.at(peer).rendered_time;
    }

    void MarkDropped(size_t peer) { receiver_stats_[peer].dropped = true; }
    bool IsDropped(size_t peer) const;

    void SetPrevFrameRenderedTime(size_t peer, webrtc::Timestamp time) {
      receiver_stats_[peer].prev_frame_rendered_time = time;
    }

    FrameStats GetStatsForPeer(size_t peer) const;

   private:
    const size_t stream_;
    const size_t owner_;
    size_t peers_count_;
    const bool enable_receive_own_stream_;
    absl::optional<VideoFrame> frame_;

    // Frame events timestamp.
    Timestamp captured_time_;
    Timestamp pre_encode_time_ = Timestamp::MinusInfinity();
    Timestamp encoded_time_ = Timestamp::MinusInfinity();
    int64_t encoded_image_size_ = 0;
    uint32_t target_encode_bitrate_ = 0;
    // Can be not set if frame was dropped by encoder.
    absl::optional<StreamCodecInfo> used_encoder_ = absl::nullopt;
    std::map<size_t, ReceiverFrameStats> receiver_stats_;
  };

  class NamesCollection {
   public:
    NamesCollection() = default;
    explicit NamesCollection(rtc::ArrayView<const std::string> names) {
      names_ = std::vector<std::string>(names.begin(), names.end());
      for (size_t i = 0; i < names_.size(); ++i) {
        index_.emplace(names_[i], i);
      }
    }

    size_t size() const { return names_.size(); }

    size_t index(absl::string_view name) const { return index_.at(name); }

    const std::string& name(size_t index) const { return names_[index]; }

    bool HasName(absl::string_view name) const {
      return index_.find(name) != index_.end();
    }

    // Add specified `name` to the collection if it isn't presented.
    // Returns index which corresponds to specified `name`.
    size_t AddIfAbsent(absl::string_view name);

   private:
    std::vector<std::string> names_;
    std::map<absl::string_view, size_t> index_;
  };

  // Report results for all metrics for all streams.
  void ReportResults();
  void ReportResults(const std::string& test_case_name,
                     const StreamStats& stats,
                     const FrameCounters& frame_counters)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Report result for single metric for specified stream.
  static void ReportResult(const std::string& metric_name,
                           const std::string& test_case_name,
                           const SamplesStatsCounter& counter,
                           const std::string& unit,
                           webrtc::test::ImproveDirection improve_direction =
                               webrtc::test::ImproveDirection::kNone);
  // Returns name of current test case for reporting.
  std::string GetTestCaseName(const std::string& stream_label) const;
  Timestamp Now();
  StatsKey ToStatsKey(const InternalStatsKey& key) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Returns string representation of stats key for metrics naming. Used for
  // backward compatibility by metrics naming for 2 peers cases.
  std::string ToMetricName(const InternalStatsKey& key) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const DefaultVideoQualityAnalyzerOptions options_;
  webrtc::Clock* const clock_;
  std::atomic<uint16_t> next_frame_id_{0};

  std::string test_label_;

  mutable Mutex mutex_;
  std::unique_ptr<NamesCollection> peers_ RTC_GUARDED_BY(mutex_);
  State state_ RTC_GUARDED_BY(mutex_) = State::kNew;
  Timestamp start_time_ RTC_GUARDED_BY(mutex_) = Timestamp::MinusInfinity();
  // Mapping from stream label to unique size_t value to use in stats and avoid
  // extra string copying.
  NamesCollection streams_ RTC_GUARDED_BY(mutex_);
  // Frames that were captured by all streams and still aren't rendered on
  // receivers or deemed dropped. Frame with id X can be removed from this map
  // if:
  // 1. The frame with id X was received in OnFrameRendered by all expected
  //    receivers.
  // 2. The frame with id Y > X was received in OnFrameRendered by all expected
  //    receivers.
  // 3. Next available frame id for newly captured frame is X
  // 4. There too many frames in flight for current video stream and X is the
  //    oldest frame id in this stream. In such case only the frame content
  //    will be removed, but the map entry will be preserved.
  std::map<uint16_t, FrameInFlight> captured_frames_in_flight_
      RTC_GUARDED_BY(mutex_);
  // Global frames count for all video streams.
  FrameCounters frame_counters_ RTC_GUARDED_BY(mutex_);
  // Frame counters per each stream per each receiver.
  std::map<InternalStatsKey, FrameCounters> stream_frame_counters_
      RTC_GUARDED_BY(mutex_);
  // Map from stream index in `streams_` to its StreamState.
  std::map<size_t, StreamState> stream_states_ RTC_GUARDED_BY(mutex_);
  // Map from stream index in `streams_` to sender peer index in `peers_`.
  std::map<size_t, size_t> stream_to_sender_ RTC_GUARDED_BY(mutex_);

  // Stores history mapping between stream index in `streams_` and frame ids.
  // Updated when frame id overlap. It required to properly return stream label
  // after 1st frame from simulcast streams was already rendered and last is
  // still encoding.
  std::map<size_t, std::set<uint16_t>> stream_to_frame_id_history_
      RTC_GUARDED_BY(mutex_);
  // Map from stream index to the list of frames as they were met in the stream.
  std::map<size_t, std::vector<uint16_t>> stream_to_frame_id_full_history_
      RTC_GUARDED_BY(mutex_);
  AnalyzerStats analyzer_stats_ RTC_GUARDED_BY(mutex_);

  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer_;
  DefaultVideoQualityAnalyzerFramesComparator frames_comparator_;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
