/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>

#include <algorithm>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/event.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/cpu_info.h"
#include "webrtc/test/layer_filtering_transport.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/statistics.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/video/video_quality_test.h"

namespace webrtc {

static const int kSendStatsPollingIntervalMs = 1000;
static const int kPayloadTypeVP8 = 123;
static const int kPayloadTypeVP9 = 124;

class VideoAnalyzer : public PacketReceiver,
                      public Transport,
                      public VideoRenderer,
                      public VideoCaptureInput,
                      public EncodedFrameObserver,
                      public EncodingTimeObserver {
 public:
  VideoAnalyzer(test::LayerFilteringTransport* transport,
                const std::string& test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames,
                FILE* graph_data_output_file,
                const std::string& graph_title,
                uint32_t ssrc_to_analyze)
      : input_(nullptr),
        transport_(transport),
        receiver_(nullptr),
        send_stream_(nullptr),
        test_label_(test_label),
        graph_data_output_file_(graph_data_output_file),
        graph_title_(graph_title),
        ssrc_to_analyze_(ssrc_to_analyze),
        frames_to_process_(duration_frames),
        frames_recorded_(0),
        frames_processed_(0),
        dropped_frames_(0),
        last_render_time_(0),
        rtp_timestamp_delta_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        stats_polling_thread_(&PollStatsThread, this, "StatsPoller"),
        comparison_available_event_(false, false),
        done_(false, false) {
    // Create thread pool for CPU-expensive PSNR/SSIM calculations.

    // Try to use about as many threads as cores, but leave kMinCoresLeft alone,
    // so that we don't accidentally starve "real" worker threads (codec etc).
    // Also, don't allocate more than kMaxComparisonThreads, even if there are
    // spare cores.

    uint32_t num_cores = CpuInfo::DetectNumberOfCores();
    RTC_DCHECK_GE(num_cores, 1u);
    static const uint32_t kMinCoresLeft = 4;
    static const uint32_t kMaxComparisonThreads = 8;

    if (num_cores <= kMinCoresLeft) {
      num_cores = 1;
    } else {
      num_cores -= kMinCoresLeft;
      num_cores = std::min(num_cores, kMaxComparisonThreads);
    }

    for (uint32_t i = 0; i < num_cores; ++i) {
      rtc::PlatformThread* thread =
          new rtc::PlatformThread(&FrameComparisonThread, this, "Analyzer");
      thread->Start();
      comparison_thread_pool_.push_back(thread);
    }
  }

  ~VideoAnalyzer() {
    for (rtc::PlatformThread* thread : comparison_thread_pool_) {
      thread->Stop();
      delete thread;
    }
  }

  virtual void SetReceiver(PacketReceiver* receiver) { receiver_ = receiver; }

  DeliveryStatus DeliverPacket(MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override {
    RtpUtility::RtpHeaderParser parser(packet, length);
    RTPHeader header;
    parser.Parse(&header);
    {
      rtc::CritScope lock(&crit_);
      recv_times_[header.timestamp - rtp_timestamp_delta_] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(media_type, packet, length, packet_time);
  }

  // EncodingTimeObserver.
  void OnReportEncodedTime(int64_t ntp_time_ms, int encode_time_ms) override {
    rtc::CritScope crit(&comparison_lock_);
    samples_encode_time_ms_[ntp_time_ms] = encode_time_ms;
  }

  void IncomingCapturedFrame(const VideoFrame& video_frame) override {
    VideoFrame copy = video_frame;
    copy.set_timestamp(copy.ntp_time_ms() * 90);

    {
      rtc::CritScope lock(&crit_);
      if (first_send_frame_.IsZeroSize() && rtp_timestamp_delta_ == 0)
        first_send_frame_ = copy;

      frames_.push_back(copy);
    }

    input_->IncomingCapturedFrame(video_frame);
  }

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override {
    RtpUtility::RtpHeaderParser parser(packet, length);
    RTPHeader header;
    parser.Parse(&header);

    int64_t current_time =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    bool result = transport_->SendRtp(packet, length, options);
    {
      rtc::CritScope lock(&crit_);
      if (rtp_timestamp_delta_ == 0) {
        rtp_timestamp_delta_ = header.timestamp - first_send_frame_.timestamp();
        first_send_frame_.Reset();
      }
      uint32_t timestamp = header.timestamp - rtp_timestamp_delta_;
      send_times_[timestamp] = current_time;
      if (!transport_->DiscardedLastPacket() &&
          header.ssrc == ssrc_to_analyze_) {
        encoded_frame_sizes_[timestamp] +=
            length - (header.headerLength + header.paddingLength);
      }
    }
    return result;
  }

  bool SendRtcp(const uint8_t* packet, size_t length) override {
    return transport_->SendRtcp(packet, length);
  }

  void EncodedFrameCallback(const EncodedFrame& frame) override {
    rtc::CritScope lock(&comparison_lock_);
    if (frames_recorded_ < frames_to_process_)
      encoded_frame_size_.AddSample(frame.length_);
  }

  void RenderFrame(const VideoFrame& video_frame,
                   int time_to_render_ms) override {
    int64_t render_time_ms =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    uint32_t send_timestamp = video_frame.timestamp() - rtp_timestamp_delta_;

    rtc::CritScope lock(&crit_);

    while (frames_.front().timestamp() < send_timestamp) {
      AddFrameComparison(frames_.front(), last_rendered_frame_, true,
                         render_time_ms);
      frames_.pop_front();
    }

    VideoFrame reference_frame = frames_.front();
    frames_.pop_front();
    assert(!reference_frame.IsZeroSize());
    if (send_timestamp == reference_frame.timestamp() - 1) {
      // TODO(ivica): Make this work for > 2 streams.
      // Look at rtp_sender.c:RTPSender::BuildRTPHeader.
      ++send_timestamp;
    }
    EXPECT_EQ(reference_frame.timestamp(), send_timestamp);
    assert(reference_frame.timestamp() == send_timestamp);

    AddFrameComparison(reference_frame, video_frame, false, render_time_ms);

    last_rendered_frame_ = video_frame;
  }

  bool IsTextureSupported() const override { return false; }

  void Wait() {
    // Frame comparisons can be very expensive. Wait for test to be done, but
    // at time-out check if frames_processed is going up. If so, give it more
    // time, otherwise fail. Hopefully this will reduce test flakiness.

    stats_polling_thread_.Start();

    int last_frames_processed = -1;
    int iteration = 0;
    while (!done_.Wait(VideoQualityTest::kDefaultTimeoutMs)) {
      int frames_processed;
      {
        rtc::CritScope crit(&comparison_lock_);
        frames_processed = frames_processed_;
      }

      // Print some output so test infrastructure won't think we've crashed.
      const char* kKeepAliveMessages[3] = {
          "Uh, I'm-I'm not quite dead, sir.",
          "Uh, I-I think uh, I could pull through, sir.",
          "Actually, I think I'm all right to come with you--"};
      printf("- %s\n", kKeepAliveMessages[iteration++ % 3]);

      if (last_frames_processed == -1) {
        last_frames_processed = frames_processed;
        continue;
      }
      ASSERT_GT(frames_processed, last_frames_processed)
          << "Analyzer stalled while waiting for test to finish.";
      last_frames_processed = frames_processed;
    }

    if (iteration > 0)
      printf("- Farewell, sweet Concorde!\n");

    // Signal stats polling thread if that is still waiting and stop it now,
    // since it uses the send_stream_ reference that might be reclaimed after
    // returning from this method.
    done_.Set();
    stats_polling_thread_.Stop();
  }

  VideoCaptureInput* input_;
  test::LayerFilteringTransport* const transport_;
  PacketReceiver* receiver_;
  VideoSendStream* send_stream_;

 private:
  struct FrameComparison {
    FrameComparison()
        : dropped(false),
          send_time_ms(0),
          recv_time_ms(0),
          render_time_ms(0),
          encoded_frame_size(0) {}

    FrameComparison(const VideoFrame& reference,
                    const VideoFrame& render,
                    bool dropped,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size)
        : reference(reference),
          render(render),
          dropped(dropped),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size) {}

    VideoFrame reference;
    VideoFrame render;
    bool dropped;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
  };

  struct Sample {
    Sample(int dropped,
           int64_t input_time_ms,
           int64_t send_time_ms,
           int64_t recv_time_ms,
           int64_t render_time_ms,
           size_t encoded_frame_size,
           double psnr,
           double ssim)
        : dropped(dropped),
          input_time_ms(input_time_ms),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size),
          psnr(psnr),
          ssim(ssim) {}

    int dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
    double psnr;
    double ssim;
  };

  void AddFrameComparison(const VideoFrame& reference,
                          const VideoFrame& render,
                          bool dropped,
                          int64_t render_time_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    int64_t send_time_ms = send_times_[reference.timestamp()];
    send_times_.erase(reference.timestamp());
    int64_t recv_time_ms = recv_times_[reference.timestamp()];
    recv_times_.erase(reference.timestamp());

    // TODO(ivica): Make this work for > 2 streams.
    auto it = encoded_frame_sizes_.find(reference.timestamp());
    if (it == encoded_frame_sizes_.end())
      it = encoded_frame_sizes_.find(reference.timestamp() - 1);
    size_t encoded_size = it == encoded_frame_sizes_.end() ? 0 : it->second;
    if (it != encoded_frame_sizes_.end())
      encoded_frame_sizes_.erase(it);

    VideoFrame reference_copy;
    VideoFrame render_copy;
    reference_copy.CopyFrame(reference);
    render_copy.CopyFrame(render);

    rtc::CritScope crit(&comparison_lock_);
    comparisons_.push_back(FrameComparison(reference_copy, render_copy, dropped,
                                           send_time_ms, recv_time_ms,
                                           render_time_ms, encoded_size));
    comparison_available_event_.Set();
  }

  static bool PollStatsThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->PollStats();
  }

  bool PollStats() {
    if (done_.Wait(kSendStatsPollingIntervalMs)) {
      // Set event again to make sure main thread is also signaled, then we're
      // done.
      done_.Set();
      return false;
    }

    VideoSendStream::Stats stats = send_stream_->GetStats();

    rtc::CritScope crit(&comparison_lock_);
    encode_frame_rate_.AddSample(stats.encode_frame_rate);
    encode_time_ms.AddSample(stats.avg_encode_time_ms);
    encode_usage_percent.AddSample(stats.encode_usage_percent);
    media_bitrate_bps.AddSample(stats.media_bitrate_bps);

    return true;
  }

  static bool FrameComparisonThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->CompareFrames();
  }

  bool CompareFrames() {
    if (AllFramesRecorded())
      return false;

    VideoFrame reference;
    VideoFrame render;
    FrameComparison comparison;

    if (!PopComparison(&comparison)) {
      // Wait until new comparison task is available, or test is done.
      // If done, wake up remaining threads waiting.
      comparison_available_event_.Wait(1000);
      if (AllFramesRecorded()) {
        comparison_available_event_.Set();
        return false;
      }
      return true;  // Try again.
    }

    PerformFrameComparison(comparison);

    if (FrameProcessed()) {
      PrintResults();
      if (graph_data_output_file_)
        PrintSamplesToFile();
      done_.Set();
      comparison_available_event_.Set();
      return false;
    }

    return true;
  }

  bool PopComparison(FrameComparison* comparison) {
    rtc::CritScope crit(&comparison_lock_);
    // If AllFramesRecorded() is true, it means we have already popped
    // frames_to_process_ frames from comparisons_, so there is no more work
    // for this thread to be done. frames_processed_ might still be lower if
    // all comparisons are not done, but those frames are currently being
    // worked on by other threads.
    if (comparisons_.empty() || AllFramesRecorded())
      return false;

    *comparison = comparisons_.front();
    comparisons_.pop_front();

    FrameRecorded();
    return true;
  }

  // Increment counter for number of frames received for comparison.
  void FrameRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_recorded_;
  }

  // Returns true if all frames to be compared have been taken from the queue.
  bool AllFramesRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    assert(frames_recorded_ <= frames_to_process_);
    return frames_recorded_ == frames_to_process_;
  }

  // Increase count of number of frames processed. Returns true if this was the
  // last frame to be processed.
  bool FrameProcessed() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_processed_;
    assert(frames_processed_ <= frames_to_process_);
    return frames_processed_ == frames_to_process_;
  }

  void PrintResults() {
    rtc::CritScope crit(&comparison_lock_);
    PrintResult("psnr", psnr_, " dB");
    PrintResult("ssim", ssim_, "");
    PrintResult("sender_time", sender_time_, " ms");
    printf("RESULT dropped_frames: %s = %d frames\n", test_label_.c_str(),
           dropped_frames_);
    PrintResult("receiver_time", receiver_time_, " ms");
    PrintResult("total_delay_incl_network", end_to_end_, " ms");
    PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
    PrintResult("encoded_frame_size", encoded_frame_size_, " bytes");
    PrintResult("encode_frame_rate", encode_frame_rate_, " fps");
    PrintResult("encode_time", encode_time_ms, " ms");
    PrintResult("encode_usage_percent", encode_usage_percent, " percent");
    PrintResult("media_bitrate", media_bitrate_bps, " bps");

    EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
    EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
  }

  void PerformFrameComparison(const FrameComparison& comparison) {
    // Perform expensive psnr and ssim calculations while not holding lock.
    double psnr = I420PSNR(&comparison.reference, &comparison.render);
    double ssim = I420SSIM(&comparison.reference, &comparison.render);

    int64_t input_time_ms = comparison.reference.ntp_time_ms();

    rtc::CritScope crit(&comparison_lock_);
    if (graph_data_output_file_) {
      samples_.push_back(
          Sample(comparison.dropped, input_time_ms, comparison.send_time_ms,
                 comparison.recv_time_ms, comparison.render_time_ms,
                 comparison.encoded_frame_size, psnr, ssim));
    }
    psnr_.AddSample(psnr);
    ssim_.AddSample(ssim);

    if (comparison.dropped) {
      ++dropped_frames_;
      return;
    }
    if (last_render_time_ != 0)
      rendered_delta_.AddSample(comparison.render_time_ms - last_render_time_);
    last_render_time_ = comparison.render_time_ms;

    sender_time_.AddSample(comparison.send_time_ms - input_time_ms);
    receiver_time_.AddSample(comparison.render_time_ms -
                             comparison.recv_time_ms);
    end_to_end_.AddSample(comparison.render_time_ms - input_time_ms);
    encoded_frame_size_.AddSample(comparison.encoded_frame_size);
  }

  void PrintResult(const char* result_type,
                   test::Statistics stats,
                   const char* unit) {
    printf("RESULT %s: %s = {%f, %f}%s\n",
           result_type,
           test_label_.c_str(),
           stats.Mean(),
           stats.StandardDeviation(),
           unit);
  }

  void PrintSamplesToFile(void) {
    FILE* out = graph_data_output_file_;
    rtc::CritScope crit(&comparison_lock_);
    std::sort(samples_.begin(), samples_.end(),
              [](const Sample& A, const Sample& B) -> bool {
                return A.input_time_ms < B.input_time_ms;
              });

    fprintf(out, "%s\n", graph_title_.c_str());
    fprintf(out, "%" PRIuS "\n", samples_.size());
    fprintf(out,
            "dropped "
            "input_time_ms "
            "send_time_ms "
            "recv_time_ms "
            "render_time_ms "
            "encoded_frame_size "
            "psnr "
            "ssim "
            "encode_time_ms\n");
    int missing_encode_time_samples = 0;
    for (const Sample& sample : samples_) {
      auto it = samples_encode_time_ms_.find(sample.input_time_ms);
      int encode_time_ms;
      if (it != samples_encode_time_ms_.end()) {
        encode_time_ms = it->second;
      } else {
        ++missing_encode_time_samples;
        encode_time_ms = -1;
      }
      fprintf(out, "%d %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRIuS
                   " %lf %lf %d\n",
              sample.dropped, sample.input_time_ms, sample.send_time_ms,
              sample.recv_time_ms, sample.render_time_ms,
              sample.encoded_frame_size, sample.psnr, sample.ssim,
              encode_time_ms);
    }
    if (missing_encode_time_samples) {
      fprintf(stderr,
              "Warning: Missing encode_time_ms samples for %d frame(s).\n",
              missing_encode_time_samples);
    }
  }

  const std::string test_label_;
  FILE* const graph_data_output_file_;
  const std::string graph_title_;
  const uint32_t ssrc_to_analyze_;
  std::vector<Sample> samples_ GUARDED_BY(comparison_lock_);
  std::map<int64_t, int> samples_encode_time_ms_ GUARDED_BY(comparison_lock_);
  test::Statistics sender_time_ GUARDED_BY(comparison_lock_);
  test::Statistics receiver_time_ GUARDED_BY(comparison_lock_);
  test::Statistics psnr_ GUARDED_BY(comparison_lock_);
  test::Statistics ssim_ GUARDED_BY(comparison_lock_);
  test::Statistics end_to_end_ GUARDED_BY(comparison_lock_);
  test::Statistics rendered_delta_ GUARDED_BY(comparison_lock_);
  test::Statistics encoded_frame_size_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_frame_rate_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_time_ms GUARDED_BY(comparison_lock_);
  test::Statistics encode_usage_percent GUARDED_BY(comparison_lock_);
  test::Statistics media_bitrate_bps GUARDED_BY(comparison_lock_);

  const int frames_to_process_;
  int frames_recorded_;
  int frames_processed_;
  int dropped_frames_;
  int64_t last_render_time_;
  uint32_t rtp_timestamp_delta_;

  rtc::CriticalSection crit_;
  std::deque<VideoFrame> frames_ GUARDED_BY(crit_);
  VideoFrame last_rendered_frame_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> send_times_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> recv_times_ GUARDED_BY(crit_);
  std::map<uint32_t, size_t> encoded_frame_sizes_ GUARDED_BY(crit_);
  VideoFrame first_send_frame_ GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;

  rtc::CriticalSection comparison_lock_;
  std::vector<rtc::PlatformThread*> comparison_thread_pool_;
  rtc::PlatformThread stats_polling_thread_;
  rtc::Event comparison_available_event_;
  std::deque<FrameComparison> comparisons_ GUARDED_BY(comparison_lock_);
  rtc::Event done_;
};

VideoQualityTest::VideoQualityTest() : clock_(Clock::GetRealTimeClock()) {}

void VideoQualityTest::TestBody() {}

std::string VideoQualityTest::GenerateGraphTitle() const {
  std::stringstream ss;
  ss << params_.common.codec;
  ss << " (" << params_.common.target_bitrate_bps / 1000 << "kbps";
  ss << ", " << params_.common.fps << " FPS";
  if (params_.screenshare.scroll_duration)
    ss << ", " << params_.screenshare.scroll_duration << "s scroll";
  if (params_.ss.streams.size() > 1)
    ss << ", Stream #" << params_.ss.selected_stream;
  if (params_.ss.num_spatial_layers > 1)
    ss << ", Layer #" << params_.ss.selected_sl;
  ss << ")";
  return ss.str();
}

void VideoQualityTest::CheckParams() {
  // Add a default stream in none specified.
  if (params_.ss.streams.empty())
    params_.ss.streams.push_back(VideoQualityTest::DefaultVideoStream(params_));
  if (params_.ss.num_spatial_layers == 0)
    params_.ss.num_spatial_layers = 1;

  if (params_.pipe.loss_percent != 0 ||
      params_.pipe.queue_length_packets != 0) {
    // Since LayerFilteringTransport changes the sequence numbers, we can't
    // use that feature with pack loss, since the NACK request would end up
    // retransmitting the wrong packets.
    RTC_CHECK(params_.ss.selected_sl == -1 ||
              params_.ss.selected_sl == params_.ss.num_spatial_layers - 1);
    RTC_CHECK(params_.common.selected_tl == -1 ||
              params_.common.selected_tl ==
                  params_.common.num_temporal_layers - 1);
  }

  // TODO(ivica): Should max_bitrate_bps == -1 represent inf max bitrate, as it
  // does in some parts of the code?
  RTC_CHECK_GE(params_.common.max_bitrate_bps,
               params_.common.target_bitrate_bps);
  RTC_CHECK_GE(params_.common.target_bitrate_bps,
               params_.common.min_bitrate_bps);
  RTC_CHECK_LT(params_.common.selected_tl, params_.common.num_temporal_layers);
  RTC_CHECK_LT(params_.ss.selected_stream, params_.ss.streams.size());
  for (const VideoStream& stream : params_.ss.streams) {
    RTC_CHECK_GE(stream.min_bitrate_bps, 0);
    RTC_CHECK_GE(stream.target_bitrate_bps, stream.min_bitrate_bps);
    RTC_CHECK_GE(stream.max_bitrate_bps, stream.target_bitrate_bps);
    RTC_CHECK_EQ(static_cast<int>(stream.temporal_layer_thresholds_bps.size()),
                 params_.common.num_temporal_layers - 1);
  }
  // TODO(ivica): Should we check if the sum of all streams/layers is equal to
  // the total bitrate? We anyway have to update them in the case bitrate
  // estimator changes the total bitrates.
  RTC_CHECK_GE(params_.ss.num_spatial_layers, 1);
  RTC_CHECK_LE(params_.ss.selected_sl, params_.ss.num_spatial_layers);
  RTC_CHECK(params_.ss.spatial_layers.empty() ||
            params_.ss.spatial_layers.size() ==
                static_cast<size_t>(params_.ss.num_spatial_layers));
  if (params_.common.codec == "VP8") {
    RTC_CHECK_EQ(params_.ss.num_spatial_layers, 1);
  } else if (params_.common.codec == "VP9") {
    RTC_CHECK_EQ(params_.ss.streams.size(), 1u);
  }
}

// Static.
std::vector<int> VideoQualityTest::ParseCSV(const std::string& str) {
  // Parse comma separated nonnegative integers, where some elements may be
  // empty. The empty values are replaced with -1.
  // E.g. "10,-20,,30,40" --> {10, 20, -1, 30,40}
  // E.g. ",,10,,20," --> {-1, -1, 10, -1, 20, -1}
  std::vector<int> result;
  if (str.empty())
    return result;

  const char* p = str.c_str();
  int value = -1;
  int pos;
  while (*p) {
    if (*p == ',') {
      result.push_back(value);
      value = -1;
      ++p;
      continue;
    }
    RTC_CHECK_EQ(sscanf(p, "%d%n", &value, &pos), 1)
        << "Unexpected non-number value.";
    p += pos;
  }
  result.push_back(value);
  return result;
}

// Static.
VideoStream VideoQualityTest::DefaultVideoStream(const Params& params) {
  VideoStream stream;
  stream.width = params.common.width;
  stream.height = params.common.height;
  stream.max_framerate = params.common.fps;
  stream.min_bitrate_bps = params.common.min_bitrate_bps;
  stream.target_bitrate_bps = params.common.target_bitrate_bps;
  stream.max_bitrate_bps = params.common.max_bitrate_bps;
  stream.max_qp = 52;
  if (params.common.num_temporal_layers == 2)
    stream.temporal_layer_thresholds_bps.push_back(stream.target_bitrate_bps);
  return stream;
}

// Static.
void VideoQualityTest::FillScalabilitySettings(
    Params* params,
    const std::vector<std::string>& stream_descriptors,
    size_t selected_stream,
    int num_spatial_layers,
    int selected_sl,
    const std::vector<std::string>& sl_descriptors) {
  // Read VideoStream and SpatialLayer elements from a list of comma separated
  // lists. To use a default value for an element, use -1 or leave empty.
  // Validity checks performed in CheckParams.

  RTC_CHECK(params->ss.streams.empty());
  for (auto descriptor : stream_descriptors) {
    if (descriptor.empty())
      continue;
    VideoStream stream = VideoQualityTest::DefaultVideoStream(*params);
    std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
    if (v[0] != -1)
      stream.width = static_cast<size_t>(v[0]);
    if (v[1] != -1)
      stream.height = static_cast<size_t>(v[1]);
    if (v[2] != -1)
      stream.max_framerate = v[2];
    if (v[3] != -1)
      stream.min_bitrate_bps = v[3];
    if (v[4] != -1)
      stream.target_bitrate_bps = v[4];
    if (v[5] != -1)
      stream.max_bitrate_bps = v[5];
    if (v.size() > 6 && v[6] != -1)
      stream.max_qp = v[6];
    if (v.size() > 7) {
      stream.temporal_layer_thresholds_bps.clear();
      stream.temporal_layer_thresholds_bps.insert(
          stream.temporal_layer_thresholds_bps.end(), v.begin() + 7, v.end());
    } else {
      // Automatic TL thresholds for more than two layers not supported.
      RTC_CHECK_LE(params->common.num_temporal_layers, 2);
    }
    params->ss.streams.push_back(stream);
  }
  params->ss.selected_stream = selected_stream;

  params->ss.num_spatial_layers = num_spatial_layers ? num_spatial_layers : 1;
  params->ss.selected_sl = selected_sl;
  RTC_CHECK(params->ss.spatial_layers.empty());
  for (auto descriptor : sl_descriptors) {
    if (descriptor.empty())
      continue;
    std::vector<int> v = VideoQualityTest::ParseCSV(descriptor);
    RTC_CHECK_GT(v[2], 0);

    SpatialLayer layer;
    layer.scaling_factor_num = v[0] == -1 ? 1 : v[0];
    layer.scaling_factor_den = v[1] == -1 ? 1 : v[1];
    layer.target_bitrate_bps = v[2];
    params->ss.spatial_layers.push_back(layer);
  }
}

void VideoQualityTest::SetupCommon(Transport* send_transport,
                                   Transport* recv_transport) {
  if (params_.logs)
    trace_to_stderr_.reset(new test::TraceToStderr);

  size_t num_streams = params_.ss.streams.size();
  CreateSendConfig(num_streams, 0, send_transport);

  int payload_type;
  if (params_.common.codec == "VP8") {
    encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp8));
    payload_type = kPayloadTypeVP8;
  } else if (params_.common.codec == "VP9") {
    encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp9));
    payload_type = kPayloadTypeVP9;
  } else {
    RTC_NOTREACHED() << "Codec not supported!";
    return;
  }
  video_send_config_.encoder_settings.encoder = encoder_.get();
  video_send_config_.encoder_settings.payload_name = params_.common.codec;
  video_send_config_.encoder_settings.payload_type = payload_type;
  video_send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  video_send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;
  for (size_t i = 0; i < num_streams; ++i)
    video_send_config_.rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[i]);

  video_send_config_.rtp.extensions.clear();
  if (params_.common.send_side_bwe) {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumber,
                     test::kTransportSequenceNumberExtensionId));
  } else {
    video_send_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kAbsSendTime, test::kAbsSendTimeExtensionId));
  }

  video_encoder_config_.min_transmit_bitrate_bps =
      params_.common.min_transmit_bps;
  video_encoder_config_.streams = params_.ss.streams;
  video_encoder_config_.spatial_layers = params_.ss.spatial_layers;

  CreateMatchingReceiveConfigs(recv_transport);

  for (size_t i = 0; i < num_streams; ++i) {
    video_receive_configs_[i].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
    video_receive_configs_[i].rtp.rtx[kSendRtxPayloadType].ssrc =
        kSendRtxSsrcs[i];
    video_receive_configs_[i].rtp.rtx[kSendRtxPayloadType].payload_type =
        kSendRtxPayloadType;
    video_receive_configs_[i].rtp.transport_cc = params_.common.send_side_bwe;
  }
}

void VideoQualityTest::SetupScreenshare() {
  RTC_CHECK(params_.screenshare.enabled);

  // Fill out codec settings.
  video_encoder_config_.content_type = VideoEncoderConfig::ContentType::kScreen;
  if (params_.common.codec == "VP8") {
    codec_settings_.VP8 = VideoEncoder::GetDefaultVp8Settings();
    codec_settings_.VP8.denoisingOn = false;
    codec_settings_.VP8.frameDroppingOn = false;
    codec_settings_.VP8.numberOfTemporalLayers =
        static_cast<unsigned char>(params_.common.num_temporal_layers);
    video_encoder_config_.encoder_specific_settings = &codec_settings_.VP8;
  } else if (params_.common.codec == "VP9") {
    codec_settings_.VP9 = VideoEncoder::GetDefaultVp9Settings();
    codec_settings_.VP9.denoisingOn = false;
    codec_settings_.VP9.frameDroppingOn = false;
    codec_settings_.VP9.numberOfTemporalLayers =
        static_cast<unsigned char>(params_.common.num_temporal_layers);
    video_encoder_config_.encoder_specific_settings = &codec_settings_.VP9;
    codec_settings_.VP9.numberOfSpatialLayers =
        static_cast<unsigned char>(params_.ss.num_spatial_layers);
  }

  // Setup frame generator.
  const size_t kWidth = 1850;
  const size_t kHeight = 1110;
  std::vector<std::string> slides;
  slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));

  if (params_.screenshare.scroll_duration == 0) {
    // Cycle image every slide_change_interval seconds.
    frame_generator_.reset(test::FrameGenerator::CreateFromYuvFile(
        slides, kWidth, kHeight,
        params_.screenshare.slide_change_interval * params_.common.fps));
  } else {
    RTC_CHECK_LE(params_.common.width, kWidth);
    RTC_CHECK_LE(params_.common.height, kHeight);
    RTC_CHECK_GT(params_.screenshare.slide_change_interval, 0);
    const int kPauseDurationMs = (params_.screenshare.slide_change_interval -
                                  params_.screenshare.scroll_duration) *
                                 1000;
    RTC_CHECK_LE(params_.screenshare.scroll_duration,
                 params_.screenshare.slide_change_interval);

    frame_generator_.reset(
        test::FrameGenerator::CreateScrollingInputFromYuvFiles(
            clock_, slides, kWidth, kHeight, params_.common.width,
            params_.common.height, params_.screenshare.scroll_duration * 1000,
            kPauseDurationMs));
  }
}

void VideoQualityTest::CreateCapturer(VideoCaptureInput* input) {
  if (params_.screenshare.enabled) {
    test::FrameGeneratorCapturer* frame_generator_capturer =
        new test::FrameGeneratorCapturer(
            clock_, input, frame_generator_.release(), params_.common.fps);
    EXPECT_TRUE(frame_generator_capturer->Init());
    capturer_.reset(frame_generator_capturer);
  } else {
    if (params_.video.clip_name.empty()) {
      capturer_.reset(test::VideoCapturer::Create(input, params_.common.width,
                                                  params_.common.height,
                                                  params_.common.fps, clock_));
    } else {
      capturer_.reset(test::FrameGeneratorCapturer::CreateFromYuvFile(
          input, test::ResourcePath(params_.video.clip_name, "yuv"),
          params_.common.width, params_.common.height, params_.common.fps,
          clock_));
      ASSERT_TRUE(capturer_.get() != nullptr)
          << "Could not create capturer for " << params_.video.clip_name
          << ".yuv. Is this resource file present?";
    }
  }
}

void VideoQualityTest::RunWithAnalyzer(const Params& params) {
  params_ = params;

  // TODO(ivica): Merge with RunWithRenderer and use a flag / argument to
  // differentiate between the analyzer and the renderer case.
  CheckParams();

  FILE* graph_data_output_file = nullptr;
  if (!params_.analyzer.graph_data_output_filename.empty()) {
    graph_data_output_file =
        fopen(params_.analyzer.graph_data_output_filename.c_str(), "w");
    RTC_CHECK(graph_data_output_file != nullptr)
        << "Can't open the file " << params_.analyzer.graph_data_output_filename
        << "!";
  }

  Call::Config call_config;
  call_config.bitrate_config = params.common.call_bitrate_config;
  CreateCalls(call_config, call_config);

  test::LayerFilteringTransport send_transport(
      params.pipe, sender_call_.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      params.common.selected_tl, params_.ss.selected_sl);
  test::DirectTransport recv_transport(params.pipe, receiver_call_.get());

  std::string graph_title = params_.analyzer.graph_title;
  if (graph_title.empty())
    graph_title = VideoQualityTest::GenerateGraphTitle();

  // In the case of different resolutions, the functions calculating PSNR and
  // SSIM return -1.0, instead of a positive value as usual. VideoAnalyzer
  // aborts if the average psnr/ssim are below the given threshold, which is
  // 0.0 by default. Setting the thresholds to -1.1 prevents the unnecessary
  // abort.
  VideoStream& selected_stream = params_.ss.streams[params_.ss.selected_stream];
  int selected_sl = params_.ss.selected_sl != -1
                        ? params_.ss.selected_sl
                        : params_.ss.num_spatial_layers - 1;
  bool disable_quality_check =
      selected_stream.width != params_.common.width ||
      selected_stream.height != params_.common.height ||
      (!params_.ss.spatial_layers.empty() &&
       params_.ss.spatial_layers[selected_sl].scaling_factor_num !=
           params_.ss.spatial_layers[selected_sl].scaling_factor_den);
  if (disable_quality_check) {
    fprintf(stderr,
            "Warning: Calculating PSNR and SSIM for downsized resolution "
            "not implemented yet! Skipping PSNR and SSIM calculations!");
  }

  VideoAnalyzer analyzer(
      &send_transport, params_.analyzer.test_label,
      disable_quality_check ? -1.1 : params_.analyzer.avg_psnr_threshold,
      disable_quality_check ? -1.1 : params_.analyzer.avg_ssim_threshold,
      params_.analyzer.test_durations_secs * params_.common.fps,
      graph_data_output_file, graph_title,
      kVideoSendSsrcs[params_.ss.selected_stream]);

  analyzer.SetReceiver(receiver_call_->Receiver());
  send_transport.SetReceiver(&analyzer);
  recv_transport.SetReceiver(sender_call_->Receiver());

  SetupCommon(&analyzer, &recv_transport);
  video_send_config_.encoding_time_observer = &analyzer;
  video_receive_configs_[params_.ss.selected_stream].renderer = &analyzer;
  for (auto& config : video_receive_configs_)
    config.pre_decode_callback = &analyzer;

  if (params_.screenshare.enabled)
    SetupScreenshare();

  CreateVideoStreams();
  analyzer.input_ = video_send_stream_->Input();
  analyzer.send_stream_ = video_send_stream_;

  CreateCapturer(&analyzer);

  video_send_stream_->Start();
  for (VideoReceiveStream* receive_stream : video_receive_streams_)
    receive_stream->Start();
  capturer_->Start();

  analyzer.Wait();

  send_transport.StopSending();
  recv_transport.StopSending();

  capturer_->Stop();
  for (VideoReceiveStream* receive_stream : video_receive_streams_)
    receive_stream->Stop();
  video_send_stream_->Stop();

  DestroyStreams();

  if (graph_data_output_file)
    fclose(graph_data_output_file);
}

void VideoQualityTest::RunWithVideoRenderer(const Params& params) {
  params_ = params;
  CheckParams();

  rtc::scoped_ptr<test::VideoRenderer> local_preview(
      test::VideoRenderer::Create("Local Preview", params_.common.width,
                                  params_.common.height));
  size_t stream_id = params_.ss.selected_stream;
  std::string title = "Loopback Video";
  if (params_.ss.streams.size() > 1) {
    std::ostringstream s;
    s << stream_id;
    title += " - Stream #" + s.str();
  }

  rtc::scoped_ptr<test::VideoRenderer> loopback_video(
      test::VideoRenderer::Create(title.c_str(),
                                  params_.ss.streams[stream_id].width,
                                  params_.ss.streams[stream_id].height));

  // TODO(ivica): Remove bitrate_config and use the default Call::Config(), to
  // match the full stack tests.
  Call::Config call_config;
  call_config.bitrate_config = params_.common.call_bitrate_config;
  rtc::scoped_ptr<Call> call(Call::Create(call_config));

  test::LayerFilteringTransport transport(
      params.pipe, call.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      params.common.selected_tl, params_.ss.selected_sl);
  // TODO(ivica): Use two calls to be able to merge with RunWithAnalyzer or at
  // least share as much code as possible. That way this test would also match
  // the full stack tests better.
  transport.SetReceiver(call->Receiver());

  SetupCommon(&transport, &transport);

  video_send_config_.local_renderer = local_preview.get();
  video_receive_configs_[stream_id].renderer = loopback_video.get();

  if (params_.screenshare.enabled)
    SetupScreenshare();

  video_send_stream_ =
      call->CreateVideoSendStream(video_send_config_, video_encoder_config_);
  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(video_receive_configs_[stream_id]);
  CreateCapturer(video_send_stream_->Input());

  receive_stream->Start();
  video_send_stream_->Start();
  capturer_->Start();

  test::PressEnterToContinue();

  capturer_->Stop();
  video_send_stream_->Stop();
  receive_stream->Stop();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(video_send_stream_);

  transport.StopSending();
}

}  // namespace webrtc
