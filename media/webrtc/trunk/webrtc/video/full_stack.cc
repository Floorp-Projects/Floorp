/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>

#include <deque>
#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/statistics.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"

namespace webrtc {

static const int kFullStackTestDurationSecs = 60;

struct FullStackTestParams {
  const char* test_label;
  struct {
    const char* name;
    size_t width, height;
    int fps;
  } clip;
  int min_bitrate_bps;
  int target_bitrate_bps;
  int max_bitrate_bps;
  double avg_psnr_threshold;
  double avg_ssim_threshold;
  FakeNetworkPipe::Config link;
};

class FullStackTest : public test::CallTest {
 protected:
  void RunTest(const FullStackTestParams& params);
};

class VideoAnalyzer : public PacketReceiver,
                      public newapi::Transport,
                      public VideoRenderer,
                      public VideoSendStreamInput {
 public:
  VideoAnalyzer(VideoSendStreamInput* input,
                Transport* transport,
                const char* test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames)
      : input_(input),
        transport_(transport),
        receiver_(NULL),
        test_label_(test_label),
        frames_left_(duration_frames),
        dropped_frames_(0),
        last_render_time_(0),
        rtp_timestamp_delta_(0),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        first_send_frame_(NULL),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        comparison_lock_(CriticalSectionWrapper::CreateCriticalSection()),
        comparison_thread_(ThreadWrapper::CreateThread(&FrameComparisonThread,
                                                       this)),
        done_(EventWrapper::Create()) {
    unsigned int id;
    EXPECT_TRUE(comparison_thread_->Start(id));
  }

  ~VideoAnalyzer() {
    EXPECT_TRUE(comparison_thread_->Stop());

    while (!frames_.empty()) {
      delete frames_.back();
      frames_.pop_back();
    }
    while (!frame_pool_.empty()) {
      delete frame_pool_.back();
      frame_pool_.pop_back();
    }
  }

  virtual void SetReceiver(PacketReceiver* receiver) { receiver_ = receiver; }

  virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                       size_t length) OVERRIDE {
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, length, &header);
    {
      CriticalSectionScoped lock(crit_.get());
      recv_times_[header.timestamp - rtp_timestamp_delta_] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(packet, length);
  }

  virtual void SwapFrame(I420VideoFrame* video_frame) OVERRIDE {
    I420VideoFrame* copy = NULL;
    {
      CriticalSectionScoped lock(crit_.get());
      if (frame_pool_.size() > 0) {
        copy = frame_pool_.front();
        frame_pool_.pop_front();
      }
    }
    if (copy == NULL)
      copy = new I420VideoFrame();

    copy->CopyFrame(*video_frame);
    copy->set_timestamp(copy->render_time_ms() * 90);

    {
      CriticalSectionScoped lock(crit_.get());
      if (first_send_frame_ == NULL && rtp_timestamp_delta_ == 0)
        first_send_frame_ = copy;

      frames_.push_back(copy);
    }

    input_->SwapFrame(video_frame);
  }

  virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, length, &header);

    {
      CriticalSectionScoped lock(crit_.get());
      if (rtp_timestamp_delta_ == 0) {
        rtp_timestamp_delta_ =
            header.timestamp - first_send_frame_->timestamp();
        first_send_frame_ = NULL;
      }
      send_times_[header.timestamp - rtp_timestamp_delta_] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return transport_->SendRtp(packet, length);
  }

  virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    return transport_->SendRtcp(packet, length);
  }

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    int64_t render_time_ms =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    uint32_t send_timestamp = video_frame.timestamp() - rtp_timestamp_delta_;

    CriticalSectionScoped lock(crit_.get());
    while (frames_.front()->timestamp() < send_timestamp) {
      AddFrameComparison(
          frames_.front(), &last_rendered_frame_, true, render_time_ms);
      frame_pool_.push_back(frames_.front());
      frames_.pop_front();
    }

    I420VideoFrame* reference_frame = frames_.front();
    frames_.pop_front();
    assert(reference_frame != NULL);
    EXPECT_EQ(reference_frame->timestamp(), send_timestamp);
    assert(reference_frame->timestamp() == send_timestamp);

    AddFrameComparison(reference_frame, &video_frame, false, render_time_ms);
    frame_pool_.push_back(reference_frame);

    last_rendered_frame_.CopyFrame(video_frame);
  }

  void Wait() {
    EXPECT_EQ(kEventSignaled, done_->Wait(FullStackTest::kLongTimeoutMs));
  }

  VideoSendStreamInput* input_;
  Transport* transport_;
  PacketReceiver* receiver_;

 private:
  struct FrameComparison {
    FrameComparison(const I420VideoFrame* reference,
                    const I420VideoFrame* render,
                    bool dropped,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms)
        : dropped(dropped),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms) {
      this->reference.CopyFrame(*reference);
      this->render.CopyFrame(*render);
    }

    FrameComparison(const FrameComparison& compare)
        : dropped(compare.dropped),
          send_time_ms(compare.send_time_ms),
          recv_time_ms(compare.recv_time_ms),
          render_time_ms(compare.render_time_ms) {
      this->reference.CopyFrame(compare.reference);
      this->render.CopyFrame(compare.render);
    }

    ~FrameComparison() {}

    I420VideoFrame reference;
    I420VideoFrame render;
    bool dropped;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
  };

  void AddFrameComparison(const I420VideoFrame* reference,
                          const I420VideoFrame* render,
                          bool dropped,
                          int64_t render_time_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    int64_t send_time_ms = send_times_[reference->timestamp()];
    send_times_.erase(reference->timestamp());
    int64_t recv_time_ms = recv_times_[reference->timestamp()];
    recv_times_.erase(reference->timestamp());

    CriticalSectionScoped crit(comparison_lock_.get());
    comparisons_.push_back(FrameComparison(reference,
                                           render,
                                           dropped,
                                           send_time_ms,
                                           recv_time_ms,
                                           render_time_ms));
  }

  static bool FrameComparisonThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->CompareFrames();
  }

  bool CompareFrames() {
    assert(frames_left_ > 0);

    I420VideoFrame reference;
    I420VideoFrame render;
    bool dropped;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;

    SleepMs(10);

    while (true) {
      {
        CriticalSectionScoped crit(comparison_lock_.get());
        if (comparisons_.empty())
          return true;
        reference.SwapFrame(&comparisons_.front().reference);
        render.SwapFrame(&comparisons_.front().render);
        dropped = comparisons_.front().dropped;
        send_time_ms = comparisons_.front().send_time_ms;
        recv_time_ms = comparisons_.front().recv_time_ms;
        render_time_ms = comparisons_.front().render_time_ms;
        comparisons_.pop_front();
      }

      PerformFrameComparison(&reference,
                             &render,
                             dropped,
                             send_time_ms,
                             recv_time_ms,
                             render_time_ms);

      if (--frames_left_ == 0) {
        PrintResult("psnr", psnr_, " dB");
        PrintResult("ssim", ssim_, "");
        PrintResult("sender_time", sender_time_, " ms");
        printf(
            "RESULT dropped_frames: %s = %d\n", test_label_, dropped_frames_);
        PrintResult("receiver_time", receiver_time_, " ms");
        PrintResult("total_delay_incl_network", end_to_end_, " ms");
        PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
        EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
        EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
        done_->Set();

        return false;
      }
    }
  }

  void PerformFrameComparison(const I420VideoFrame* reference,
                              const I420VideoFrame* render,
                              bool dropped,
                              int64_t send_time_ms,
                              int64_t recv_time_ms,
                              int64_t render_time_ms) {
    psnr_.AddSample(I420PSNR(reference, render));
    ssim_.AddSample(I420SSIM(reference, render));
    if (dropped) {
      ++dropped_frames_;
      return;
    }
    if (last_render_time_ != 0)
      rendered_delta_.AddSample(render_time_ms - last_render_time_);
    last_render_time_ = render_time_ms;

    int64_t input_time_ms = reference->render_time_ms();
    sender_time_.AddSample(send_time_ms - input_time_ms);
    receiver_time_.AddSample(render_time_ms - recv_time_ms);
    end_to_end_.AddSample(render_time_ms - input_time_ms);
  }

  void PrintResult(const char* result_type,
                   test::Statistics stats,
                   const char* unit) {
    printf("RESULT %s: %s = {%f, %f}%s\n",
           result_type,
           test_label_,
           stats.Mean(),
           stats.StandardDeviation(),
           unit);
  }

  const char* const test_label_;
  test::Statistics sender_time_;
  test::Statistics receiver_time_;
  test::Statistics psnr_;
  test::Statistics ssim_;
  test::Statistics end_to_end_;
  test::Statistics rendered_delta_;
  int frames_left_;
  int dropped_frames_;
  int64_t last_render_time_;
  uint32_t rtp_timestamp_delta_;

  const scoped_ptr<CriticalSectionWrapper> crit_;
  std::deque<I420VideoFrame*> frames_ GUARDED_BY(crit_);
  std::deque<I420VideoFrame*> frame_pool_ GUARDED_BY(crit_);
  I420VideoFrame last_rendered_frame_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> send_times_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> recv_times_ GUARDED_BY(crit_);
  I420VideoFrame* first_send_frame_ GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;

  const scoped_ptr<CriticalSectionWrapper> comparison_lock_;
  const scoped_ptr<ThreadWrapper> comparison_thread_;
  std::deque<FrameComparison> comparisons_ GUARDED_BY(comparison_lock_);
  const scoped_ptr<EventWrapper> done_;
};

void FullStackTest::RunTest(const FullStackTestParams& params) {
  test::DirectTransport send_transport(params.link);
  test::DirectTransport recv_transport(params.link);
  VideoAnalyzer analyzer(NULL,
                         &send_transport,
                         params.test_label,
                         params.avg_psnr_threshold,
                         params.avg_ssim_threshold,
                         kFullStackTestDurationSecs * params.clip.fps);

  CreateCalls(Call::Config(&analyzer), Call::Config(&recv_transport));

  analyzer.SetReceiver(receiver_call_->Receiver());
  send_transport.SetReceiver(&analyzer);
  recv_transport.SetReceiver(sender_call_->Receiver());

  CreateSendConfig(1);

  scoped_ptr<VideoEncoder> encoder(
      VideoEncoder::Create(VideoEncoder::kVp8));
  send_config_.encoder_settings.encoder = encoder.get();
  send_config_.encoder_settings.payload_name = "VP8";
  send_config_.encoder_settings.payload_type = 124;
  send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;

  VideoStream* stream = &encoder_config_.streams[0];
  stream->width = params.clip.width;
  stream->height = params.clip.height;
  stream->min_bitrate_bps = params.min_bitrate_bps;
  stream->target_bitrate_bps = params.target_bitrate_bps;
  stream->max_bitrate_bps = params.max_bitrate_bps;
  stream->max_framerate = params.clip.fps;

  CreateMatchingReceiveConfigs();
  receive_configs_[0].renderer = &analyzer;
  receive_configs_[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;

  CreateStreams();
  analyzer.input_ = send_stream_->Input();

  frame_generator_capturer_.reset(
      test::FrameGeneratorCapturer::CreateFromYuvFile(
          &analyzer,
          test::ResourcePath(params.clip.name, "yuv").c_str(),
          params.clip.width,
          params.clip.height,
          params.clip.fps,
          Clock::GetRealTimeClock()));

  ASSERT_TRUE(frame_generator_capturer_.get() != NULL)
      << "Could not create capturer for " << params.clip.name
      << ".yuv. Is this resource file present?";

  Start();

  analyzer.Wait();

  send_transport.StopSending();
  recv_transport.StopSending();

  Stop();

  DestroyStreams();
}

TEST_F(FullStackTest, ParisQcifWithoutPacketLoss) {
  FullStackTestParams paris_qcif = {"net_delay_0_0_plr_0",
                                    {"paris_qcif", 176, 144, 30},
                                    300000,
                                    300000,
                                    300000,
                                    36.0,
                                    0.96
                                   };
  RunTest(paris_qcif);
}

TEST_F(FullStackTest, ForemanCifWithoutPacketLoss) {
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  FullStackTestParams foreman_cif = {"foreman_cif_net_delay_0_0_plr_0",
                                     {"foreman_cif", 352, 288, 30},
                                     700000,
                                     700000,
                                     700000,
                                     0.0,
                                     0.0
                                    };
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5) {
  FullStackTestParams foreman_cif = {"foreman_cif_delay_50_0_plr_5",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     500000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.loss_percent = 5;
  foreman_cif.link.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps) {
  FullStackTestParams foreman_cif = {"foreman_cif_500kbps",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     500000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.queue_length_packets = 0;
  foreman_cif.link.queue_delay_ms = 0;
  foreman_cif.link.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbpsLimitedQueue) {
  FullStackTestParams foreman_cif = {"foreman_cif_500kbps_32pkts_queue",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     500000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.queue_length_packets = 32;
  foreman_cif.link.queue_delay_ms = 0;
  foreman_cif.link.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100ms) {
  FullStackTestParams foreman_cif = {"foreman_cif_500kbps_100ms",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     500000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.queue_length_packets = 0;
  foreman_cif.link.queue_delay_ms = 100;
  foreman_cif.link.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100msLimitedQueue) {
  FullStackTestParams foreman_cif = {"foreman_cif_500kbps_100ms_32pkts_queue",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     500000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.queue_length_packets = 32;
  foreman_cif.link.queue_delay_ms = 100;
  foreman_cif.link.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif1000kbps100msLimitedQueue) {
  FullStackTestParams foreman_cif = {"foreman_cif_1000kbps_100ms_32pkts_queue",
                                     {"foreman_cif", 352, 288, 30},
                                     30000,
                                     2000000,
                                     2000000,
                                     0.0,
                                     0.0
                                    };
  foreman_cif.link.queue_length_packets = 32;
  foreman_cif.link.queue_delay_ms = 100;
  foreman_cif.link.link_capacity_kbps = 1000;
  RunTest(foreman_cif);
}
}  // namespace webrtc
