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

#include "webrtc/video/loopback.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

static const int kAbsSendTimeExtensionId = 7;

static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kSendRtxSsrc = 0x654322;
static const uint32_t kReceiverLocalSsrc = 0x123456;

static const uint8_t kRtxPayloadType = 96;

Loopback::Loopback(const Config& config)
    : config_(config), clock_(Clock::GetRealTimeClock()) {
}

Loopback::~Loopback() {
}

void Loopback::Run() {
  rtc::scoped_ptr<test::TraceToStderr> trace_to_stderr_;
  if (config_.logs)
    trace_to_stderr_.reset(new test::TraceToStderr);

  rtc::scoped_ptr<test::VideoRenderer> local_preview(
      test::VideoRenderer::Create("Local Preview", config_.width,
                                  config_.height));
  rtc::scoped_ptr<test::VideoRenderer> loopback_video(
      test::VideoRenderer::Create("Loopback Video", config_.width,
                                  config_.height));

  FakeNetworkPipe::Config pipe_config;
  pipe_config.loss_percent = config_.loss_percent;
  pipe_config.link_capacity_kbps = config_.link_capacity_kbps;
  pipe_config.queue_length_packets = config_.queue_size;
  pipe_config.queue_delay_ms = config_.avg_propagation_delay_ms;
  pipe_config.delay_standard_deviation_ms = config_.std_propagation_delay_ms;
  test::DirectTransport transport(pipe_config);
  Call::Config call_config(&transport);

  call_config.bitrate_config.min_bitrate_bps =
      static_cast<int>(config_.min_bitrate_kbps) * 1000;
  call_config.bitrate_config.start_bitrate_bps =
      static_cast<int>(config_.start_bitrate_kbps) * 1000;
  call_config.bitrate_config.max_bitrate_bps =
      static_cast<int>(config_.max_bitrate_kbps) * 1000;
  rtc::scoped_ptr<Call> call(Call::Create(call_config));

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
  rtc::scoped_ptr<VideoEncoder> encoder;
  if (config_.codec == "VP8") {
    encoder.reset(VideoEncoder::Create(VideoEncoder::kVp8));
  } else if (config_.codec == "VP9") {
    encoder.reset(VideoEncoder::Create(VideoEncoder::kVp9));
  } else {
    // Codec not supported.
    RTC_NOTREACHED() << "Codec not supported!";
    return;
  }
  send_config.encoder_settings.encoder = encoder.get();
  send_config.encoder_settings.payload_name = config_.codec;
  send_config.encoder_settings.payload_type = 124;

  VideoEncoderConfig encoder_config(CreateEncoderConfig());

  VideoSendStream* send_stream =
      call->CreateVideoSendStream(send_config, encoder_config);

  rtc::scoped_ptr<test::VideoCapturer> capturer(CreateCapturer(send_stream));

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
  capturer->Start();

  test::PressEnterToContinue();

  capturer->Stop();
  send_stream->Stop();
  receive_stream->Stop();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(send_stream);

  delete decoder.decoder;

  transport.StopSending();
}

VideoEncoderConfig Loopback::CreateEncoderConfig() {
  VideoEncoderConfig encoder_config;
  encoder_config.streams = test::CreateVideoStreams(1);
  VideoStream* stream = &encoder_config.streams[0];
  stream->width = config_.width;
  stream->height = config_.height;
  stream->min_bitrate_bps = static_cast<int>(config_.min_bitrate_kbps) * 1000;
  stream->max_bitrate_bps = static_cast<int>(config_.max_bitrate_kbps) * 1000;
  stream->target_bitrate_bps =
      static_cast<int>(config_.max_bitrate_kbps) * 1000;
  stream->max_framerate = config_.fps;
  stream->max_qp = 56;
  return encoder_config;
}

test::VideoCapturer* Loopback::CreateCapturer(VideoSendStream* send_stream) {
  return test::VideoCapturer::Create(send_stream->Input(), config_.width,
                                     config_.height, config_.fps, clock_);
}

}  // namespace test
}  // namespace webrtc
