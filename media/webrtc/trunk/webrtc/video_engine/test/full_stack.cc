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

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/new_include/call.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/frame_generator_capturer.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"
#include "webrtc/video_engine/test/common/statistics.h"
#include "webrtc/video_engine/test/common/video_renderer.h"

DEFINE_int32(seconds, 10, "Seconds to run each clip.");

namespace webrtc {

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
                VideoRenderer* loopback_video,
                const char* test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                uint64_t duration_frames)
      : input_(input),
        transport_(transport),
        renderer_(loopback_video),
        receiver_(NULL),
        test_label_(test_label),
        rtp_timestamp_delta_(0),
        first_send_frame_(NULL),
        last_render_time_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        frames_left_(duration_frames),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        trigger_(EventWrapper::Create()) {}

  ~VideoAnalyzer() {
    while (!frames_.empty()) {
      delete frames_.back();
      frames_.pop_back();
    }
    while (!frame_pool_.empty()) {
      delete frame_pool_.back();
      frame_pool_.pop_back();
    }
  }

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

  virtual void PutFrame(const I420VideoFrame& video_frame,
                        uint32_t delta_capture_ms) OVERRIDE {
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

    copy->CopyFrame(video_frame);
    copy->set_timestamp(copy->render_time_ms() * 90);

    {
      CriticalSectionScoped cs(crit_.get());
      if (first_send_frame_ == NULL && rtp_timestamp_delta_ == 0)
        first_send_frame_ = copy;

      frames_.push_back(copy);
    }

    input_->PutFrame(video_frame, delta_capture_ms);
  }

  virtual bool SendRTP(const uint8_t* packet, size_t length) OVERRIDE {
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

    return transport_->SendRTP(packet, length);
  }

  virtual bool SendRTCP(const uint8_t* packet, size_t length) OVERRIDE {
    return transport_->SendRTCP(packet, length);
  }

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    uint32_t send_timestamp = video_frame.timestamp() - rtp_timestamp_delta_;

    {
      CriticalSectionScoped cs(crit_.get());
      while (frames_.front()->timestamp() < send_timestamp) {
        AddFrameComparison(frames_.front(), &last_rendered_frame_, true);
        frame_pool_.push_back(frames_.front());
        frames_.pop_front();
      }

      I420VideoFrame* reference_frame = frames_.front();
      frames_.pop_front();
      assert(reference_frame != NULL);
      assert(reference_frame->timestamp() == send_timestamp);

      AddFrameComparison(reference_frame, &video_frame, false);
      frame_pool_.push_back(reference_frame);

      if (--frames_left_ == 0) {
        PrintResult("psnr", psnr_, " dB");
        PrintResult("ssim", ssim_, "");
        PrintResult("sender_time", sender_time_, " ms");
        PrintResult("receiver_time", receiver_time_, " ms");
        PrintResult("total_delay_incl_network", end_to_end_, " ms");
        PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
        EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
        EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
        trigger_->Set();
      }
    }

    renderer_->RenderFrame(video_frame, time_to_render_ms);
    last_rendered_frame_.CopyFrame(video_frame);
  }

  void Wait() { trigger_->Wait(WEBRTC_EVENT_INFINITE); }

  VideoSendStreamInput* input_;
  Transport* transport_;
  VideoRenderer* renderer_;
  PacketReceiver* receiver_;

 private:
  void AddFrameComparison(const I420VideoFrame* reference_frame,
                          const I420VideoFrame* render,
                          bool dropped) {
    int64_t render_time = Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    psnr_.AddSample(I420PSNR(reference_frame, render));
    ssim_.AddSample(I420SSIM(reference_frame, render));
    if (dropped)
      return;
    if (last_render_time_ != 0)
      rendered_delta_.AddSample(render_time - last_render_time_);
    last_render_time_ = render_time;

    int64_t input_time = reference_frame->render_time_ms();
    int64_t send_time = send_times_[reference_frame->timestamp()];
    send_times_.erase(reference_frame->timestamp());
    sender_time_.AddSample(send_time - input_time);
    int64_t recv_time = recv_times_[reference_frame->timestamp()];
    recv_times_.erase(reference_frame->timestamp());
    receiver_time_.AddSample(render_time - recv_time);
    end_to_end_.AddSample(render_time - input_time);
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
  uint32_t frames_left_;
  scoped_ptr<CriticalSectionWrapper> crit_;
  scoped_ptr<EventWrapper> trigger_;
};

TEST_P(FullStackTest, DISABLED_NoPacketLoss) {
  FullStackTestParams params = GetParam();

  scoped_ptr<test::VideoRenderer> local_preview(test::VideoRenderer::Create(
      "Local Preview", params.clip.width, params.clip.height));
  scoped_ptr<test::VideoRenderer> loopback_video(test::VideoRenderer::Create(
      "Loopback Video", params.clip.width, params.clip.height));

  test::DirectTransport transport;
  VideoAnalyzer analyzer(
      NULL,
      &transport,
      loopback_video.get(),
      params.test_label,
      params.avg_psnr_threshold,
      params.avg_ssim_threshold,
      static_cast<uint64_t>(FLAGS_seconds * params.clip.fps));

  Call::Config call_config(&analyzer);

  scoped_ptr<Call> call(Call::Create(call_config));
  analyzer.receiver_ = call->Receiver();
  transport.SetReceiver(&analyzer);

  VideoSendStream::Config send_config = call->GetDefaultSendConfig();
  test::GenerateRandomSsrcs(&send_config, &reserved_ssrcs_);

  send_config.local_renderer = local_preview.get();

  // TODO(pbos): static_cast shouldn't be required after mflodman refactors the
  //             VideoCodec struct.
  send_config.codec.width = static_cast<uint16_t>(params.clip.width);
  send_config.codec.height = static_cast<uint16_t>(params.clip.height);
  send_config.codec.minBitrate = params.bitrate;
  send_config.codec.startBitrate = params.bitrate;
  send_config.codec.maxBitrate = params.bitrate;

  VideoSendStream* send_stream = call->CreateSendStream(send_config);
  analyzer.input_ = send_stream->Input();

  scoped_ptr<test::FrameGeneratorCapturer> file_capturer(
      test::FrameGeneratorCapturer::CreateFromYuvFile(
          &analyzer,
          test::ResourcePath(params.clip.name, "yuv").c_str(),
          params.clip.width,
          params.clip.height,
          params.clip.fps,
          Clock::GetRealTimeClock()));

  VideoReceiveStream::Config receive_config = call->GetDefaultReceiveConfig();
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.renderer = &analyzer;

  VideoReceiveStream* receive_stream =
      call->CreateReceiveStream(receive_config);

  receive_stream->StartReceive();
  send_stream->StartSend();

  file_capturer->Start();

  analyzer.Wait();

  file_capturer->Stop();
  send_stream->StopSend();
  receive_stream->StopReceive();

  call->DestroyReceiveStream(receive_stream);
  call->DestroySendStream(send_stream);

  transport.StopSending();
}

INSTANTIATE_TEST_CASE_P(FullStack,
                        FullStackTest,
                        ::testing::Values(paris_qcif, foreman_cif));

}  // namespace webrtc
