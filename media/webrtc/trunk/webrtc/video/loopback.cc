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

#include <map>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/run_test.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

static const int kAbsSendTimeExtensionId = 7;

namespace flags {

DEFINE_int32(width, 640, "Video width.");
size_t Width() { return static_cast<size_t>(FLAGS_width); }

DEFINE_int32(height, 480, "Video height.");
size_t Height() { return static_cast<size_t>(FLAGS_height); }

DEFINE_int32(fps, 30, "Frames per second.");
int Fps() { return static_cast<int>(FLAGS_fps); }

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() { return static_cast<size_t>(FLAGS_min_bitrate); }

DEFINE_int32(start_bitrate, 300, "Video starting bitrate.");
size_t StartBitrate() { return static_cast<size_t>(FLAGS_start_bitrate); }

DEFINE_int32(max_bitrate, 800, "Maximum video bitrate.");
size_t MaxBitrate() { return static_cast<size_t>(FLAGS_max_bitrate); }

DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() { return static_cast<std::string>(FLAGS_codec); }

DEFINE_int32(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAGS_loss_percent);
}

DEFINE_int32(link_capacity,
             0,
             "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacity() {
  return static_cast<int>(FLAGS_link_capacity);
}

DEFINE_int32(queue_size, 0, "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAGS_queue_size);
}

DEFINE_int32(avg_propagation_delay_ms,
             0,
             "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAGS_avg_propagation_delay_ms);
}

DEFINE_int32(std_propagation_delay_ms,
             0,
             "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAGS_std_propagation_delay_ms);
}

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
}  // namespace flags

static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kSendRtxSsrc = 0x654322;
static const uint32_t kReceiverLocalSsrc = 0x123456;

static const uint8_t kRtxPayloadType = 96;

void Loopback() {
  scoped_ptr<test::TraceToStderr> trace_to_stderr_;
  if (webrtc::flags::FLAGS_logs)
    trace_to_stderr_.reset(new test::TraceToStderr);

  scoped_ptr<test::VideoRenderer> local_preview(test::VideoRenderer::Create(
      "Local Preview", flags::Width(), flags::Height()));
  scoped_ptr<test::VideoRenderer> loopback_video(test::VideoRenderer::Create(
      "Loopback Video", flags::Width(), flags::Height()));

  FakeNetworkPipe::Config pipe_config;
  pipe_config.loss_percent = flags::LossPercent();
  pipe_config.link_capacity_kbps = flags::LinkCapacity();
  pipe_config.queue_length_packets = flags::QueueSize();
  pipe_config.queue_delay_ms = flags::AvgPropagationDelayMs();
  pipe_config.delay_standard_deviation_ms = flags::StdPropagationDelayMs();
  test::DirectTransport transport(pipe_config);
  Call::Config call_config(&transport);
  call_config.stream_start_bitrate_bps =
      static_cast<int>(flags::StartBitrate()) * 1000;
  scoped_ptr<Call> call(Call::Create(call_config));

  // Loopback, call sends to itself.
  transport.SetReceiver(call->Receiver());

  VideoSendStream::Config send_config;
  send_config.rtp.ssrcs.push_back(kSendSsrc);
  send_config.rtp.rtx.ssrcs.push_back(kSendRtxSsrc);
  send_config.rtp.rtx.payload_type = kRtxPayloadType;
  send_config.rtp.nack.rtp_history_ms = 1000;
  send_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));

  send_config.local_renderer = local_preview.get();
  scoped_ptr<VideoEncoder> encoder;
  if (flags::Codec() == "VP8") {
    encoder.reset(VideoEncoder::Create(VideoEncoder::kVp8));
  } else if (flags::Codec() == "VP9") {
    encoder.reset(VideoEncoder::Create(VideoEncoder::kVp9));
  } else {
    // Codec not supported.
    assert(false && "Codec not supported!");
    return;
  }
  send_config.encoder_settings.encoder = encoder.get();
  send_config.encoder_settings.payload_name = flags::Codec();
  send_config.encoder_settings.payload_type = 124;
  VideoEncoderConfig encoder_config;
  encoder_config.streams = test::CreateVideoStreams(1);
  VideoStream* stream = &encoder_config.streams[0];
  stream->width = flags::Width();
  stream->height = flags::Height();
  stream->min_bitrate_bps = static_cast<int>(flags::MinBitrate()) * 1000;
  stream->target_bitrate_bps = static_cast<int>(flags::MaxBitrate()) * 1000;
  stream->max_bitrate_bps = static_cast<int>(flags::MaxBitrate()) * 1000;
  stream->max_framerate = 30;
  stream->max_qp = 56;

  VideoSendStream* send_stream =
      call->CreateVideoSendStream(send_config, encoder_config);

  Clock* test_clock = Clock::GetRealTimeClock();

  scoped_ptr<test::VideoCapturer> camera(
      test::VideoCapturer::Create(send_stream->Input(),
                                  flags::Width(),
                                  flags::Height(),
                                  flags::Fps(),
                                  test_clock));

  VideoReceiveStream::Config receive_config;
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.rtp.nack.rtp_history_ms = 1000;
  receive_config.rtp.rtx[kRtxPayloadType].ssrc = kSendRtxSsrc;
  receive_config.rtp.rtx[kRtxPayloadType].payload_type = kRtxPayloadType;
  receive_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
  receive_config.renderer = loopback_video.get();
  VideoReceiveStream::Decoder decoder =
      test::CreateMatchingDecoder(send_config.encoder_settings);
  receive_config.decoders.push_back(decoder);

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(receive_config);

  receive_stream->Start();
  send_stream->Start();
  camera->Start();

  test::PressEnterToContinue();

  camera->Stop();
  send_stream->Stop();
  receive_stream->Stop();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(send_stream);

  delete decoder.decoder;

  transport.StopSending();
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::InitFieldTrialsFromString(
      webrtc::flags::FLAGS_force_fieldtrials);
  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
