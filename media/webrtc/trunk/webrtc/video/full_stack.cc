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

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/statistics.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"

DEFINE_int32(seconds, 10, "Seconds to run each clip.");

namespace webrtc {

static const uint32_t kSendSsrc = 0x654321;

struct FullStackTestParams {
  const char* test_label;
  struct {
    const char* name;
    size_t width, height;
    int fps;
  } clip;
  unsigned int bitrate;
  double avg_psnr_threshold;
  double avg_ssim_threshold;
};

FullStackTestParams paris_qcif = {
    "net_delay_0_0_plr_0", {"paris_qcif", 176, 144, 30}, 300, 36.0, 0.96};

// TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
FullStackTestParams foreman_cif = {
    "foreman_cif_net_delay_0_0_plr_0",
    {"foreman_cif", 352, 288, 30},
    700,
    0.0,
    0.0};

class FullStackTest : public ::testing::TestWithParam<FullStackTestParams> {
 protected:
  std::map<uint32_t, bool> reserved_ssrcs_;
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
        dropped_frames_(0),
        rtp_timestamp_delta_(0),
        first_send_frame_(NULL),
        last_render_time_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        frames_left_(duration_frames),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        comparison_lock_(CriticalSectionWrapper::CreateCriticalSection()),
        comparison_thread_(ThreadWrapper::CreateThread(&FrameComparisonThread,
                                                       this)),
        trigger_(EventWrapper::Create()) {
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

  virtual bool DeliverPacket(const uint8_t* packet, size_t length) OVERRIDE {
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, static_cast<int>(length), &header);
    {
      CriticalSectionScoped cs(crit_.get());
      recv_times_[header.timestamp - rtp_timestamp_delta_] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(packet, length);
  }

  virtual void PutFrame(const I420VideoFrame& video_frame) OVERRIDE {
    ADD_FAILURE() << "PutFrame() should not have been called in this test.";
  }

  virtual void SwapFrame(I420VideoFrame* video_frame) OVERRIDE {
    I420VideoFrame* copy = NULL;
    {
      CriticalSectionScoped cs(crit_.get());
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
      CriticalSectionScoped cs(crit_.get());
      if (first_send_frame_ == NULL && rtp_timestamp_delta_ == 0)
        first_send_frame_ = copy;

      frames_.push_back(copy);
    }

    input_->SwapFrame(video_frame);
  }

  virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, static_cast<int>(length), &header);

    {
      CriticalSectionScoped cs(crit_.get());
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

    {
      CriticalSectionScoped cs(crit_.get());
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
    }

    last_rendered_frame_.CopyFrame(video_frame);
  }

  void Wait() { trigger_->Wait(120 * 1000); }

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
                          int64_t render_time_ms) {
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
        trigger_->Set();

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

  const char* test_label_;
  test::Statistics sender_time_;
  test::Statistics receiver_time_;
  test::Statistics psnr_;
  test::Statistics ssim_;
  test::Statistics end_to_end_;
  test::Statistics rendered_delta_;

  int dropped_frames_;
  std::deque<I420VideoFrame*> frames_;
  std::deque<I420VideoFrame*> frame_pool_;
  I420VideoFrame last_rendered_frame_;
  std::map<uint32_t, int64_t> send_times_;
  std::map<uint32_t, int64_t> recv_times_;
  uint32_t rtp_timestamp_delta_;
  I420VideoFrame* first_send_frame_;
  int64_t last_render_time_;
  double avg_psnr_threshold_;
  double avg_ssim_threshold_;
  int frames_left_;
  scoped_ptr<CriticalSectionWrapper> crit_;
  scoped_ptr<CriticalSectionWrapper> comparison_lock_;
  scoped_ptr<ThreadWrapper> comparison_thread_;
  std::deque<FrameComparison> comparisons_;
  scoped_ptr<EventWrapper> trigger_;
};

TEST_P(FullStackTest, NoPacketLoss) {
  static const uint32_t kReceiverLocalSsrc = 0x123456;
  FullStackTestParams params = GetParam();

  test::DirectTransport transport;
  VideoAnalyzer analyzer(NULL,
                         &transport,
                         params.test_label,
                         params.avg_psnr_threshold,
                         params.avg_ssim_threshold,
                         FLAGS_seconds * params.clip.fps);

  Call::Config call_config(&analyzer);

  scoped_ptr<Call> call(Call::Create(call_config));
  analyzer.SetReceiver(call->Receiver());
  transport.SetReceiver(&analyzer);

  VideoSendStream::Config send_config = call->GetDefaultSendConfig();
  send_config.rtp.ssrcs.push_back(kSendSsrc);

  // TODO(pbos): static_cast shouldn't be required after mflodman refactors the
  //             VideoCodec struct.
  send_config.codec.width = static_cast<uint16_t>(params.clip.width);
  send_config.codec.height = static_cast<uint16_t>(params.clip.height);
  send_config.codec.minBitrate = params.bitrate;
  send_config.codec.startBitrate = params.bitrate;
  send_config.codec.maxBitrate = params.bitrate;

  VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);
  analyzer.input_ = send_stream->Input();

  scoped_ptr<test::FrameGeneratorCapturer> file_capturer(
      test::FrameGeneratorCapturer::CreateFromYuvFile(
          &analyzer,
          test::ResourcePath(params.clip.name, "yuv").c_str(),
          params.clip.width,
          params.clip.height,
          params.clip.fps,
          Clock::GetRealTimeClock()));
  ASSERT_TRUE(file_capturer.get() != NULL)
      << "Could not create capturer for " << params.clip.name
      << ".yuv. Is this resource file present?";

  VideoReceiveStream::Config receive_config = call->GetDefaultReceiveConfig();
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.renderer = &analyzer;

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(receive_config);

  receive_stream->StartReceiving();
  send_stream->StartSending();

  file_capturer->Start();

  analyzer.Wait();

  file_capturer->Stop();
  send_stream->StopSending();
  receive_stream->StopReceiving();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(send_stream);

  transport.StopSending();
}

INSTANTIATE_TEST_CASE_P(FullStack,
                        FullStackTest,
                        ::testing::Values(paris_qcif, foreman_cif));

}  // namespace webrtc
