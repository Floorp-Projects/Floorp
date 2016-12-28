/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/audio_file_processor.h"

#include <algorithm>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/test/protobuf_utils.h"

using rtc::scoped_ptr;
using rtc::CheckedDivExact;
using std::vector;
using webrtc::audioproc::Event;
using webrtc::audioproc::Init;
using webrtc::audioproc::ReverseStream;
using webrtc::audioproc::Stream;

namespace webrtc {
namespace {

// Returns a StreamConfig corresponding to file.
StreamConfig GetStreamConfig(const WavFile& file) {
  return StreamConfig(file.sample_rate(), file.num_channels());
}

// Returns a ChannelBuffer corresponding to file.
ChannelBuffer<float> GetChannelBuffer(const WavFile& file) {
  return ChannelBuffer<float>(
      CheckedDivExact(file.sample_rate(), AudioFileProcessor::kChunksPerSecond),
      file.num_channels());
}

}  // namespace

WavFileProcessor::WavFileProcessor(scoped_ptr<AudioProcessing> ap,
                                   scoped_ptr<WavReader> in_file,
                                   scoped_ptr<WavWriter> out_file)
    : ap_(std::move(ap)),
      in_buf_(GetChannelBuffer(*in_file)),
      out_buf_(GetChannelBuffer(*out_file)),
      input_config_(GetStreamConfig(*in_file)),
      output_config_(GetStreamConfig(*out_file)),
      buffer_reader_(std::move(in_file)),
      buffer_writer_(std::move(out_file)) {}

bool WavFileProcessor::ProcessChunk() {
  if (!buffer_reader_.Read(&in_buf_)) {
    return false;
  }
  {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(kNoErr,
                 ap_->ProcessStream(in_buf_.channels(), input_config_,
                                    output_config_, out_buf_.channels()));
  }
  buffer_writer_.Write(out_buf_);
  return true;
}

AecDumpFileProcessor::AecDumpFileProcessor(scoped_ptr<AudioProcessing> ap,
                                           FILE* dump_file,
                                           scoped_ptr<WavWriter> out_file)
    : ap_(std::move(ap)),
      dump_file_(dump_file),
      out_buf_(GetChannelBuffer(*out_file)),
      output_config_(GetStreamConfig(*out_file)),
      buffer_writer_(std::move(out_file)) {
  RTC_CHECK(dump_file_) << "Could not open dump file for reading.";
}

AecDumpFileProcessor::~AecDumpFileProcessor() {
  fclose(dump_file_);
}

bool AecDumpFileProcessor::ProcessChunk() {
  Event event_msg;

  // Continue until we process our first Stream message.
  do {
    if (!ReadMessageFromFile(dump_file_, &event_msg)) {
      return false;
    }

    if (event_msg.type() == Event::INIT) {
      RTC_CHECK(event_msg.has_init());
      HandleMessage(event_msg.init());

    } else if (event_msg.type() == Event::STREAM) {
      RTC_CHECK(event_msg.has_stream());
      HandleMessage(event_msg.stream());

    } else if (event_msg.type() == Event::REVERSE_STREAM) {
      RTC_CHECK(event_msg.has_reverse_stream());
      HandleMessage(event_msg.reverse_stream());
    }
  } while (event_msg.type() != Event::STREAM);

  return true;
}

void AecDumpFileProcessor::HandleMessage(const Init& msg) {
  RTC_CHECK(msg.has_sample_rate());
  RTC_CHECK(msg.has_num_input_channels());
  RTC_CHECK(msg.has_num_reverse_channels());

  in_buf_.reset(new ChannelBuffer<float>(
      CheckedDivExact(msg.sample_rate(), kChunksPerSecond),
      msg.num_input_channels()));
  const int reverse_sample_rate = msg.has_reverse_sample_rate()
                                      ? msg.reverse_sample_rate()
                                      : msg.sample_rate();
  reverse_buf_.reset(new ChannelBuffer<float>(
      CheckedDivExact(reverse_sample_rate, kChunksPerSecond),
      msg.num_reverse_channels()));
  input_config_ = StreamConfig(msg.sample_rate(), msg.num_input_channels());
  reverse_config_ =
      StreamConfig(reverse_sample_rate, msg.num_reverse_channels());

  const ProcessingConfig config = {
      {input_config_, output_config_, reverse_config_, reverse_config_}};
  RTC_CHECK_EQ(kNoErr, ap_->Initialize(config));
}

void AecDumpFileProcessor::HandleMessage(const Stream& msg) {
  RTC_CHECK(!msg.has_input_data());
  RTC_CHECK_EQ(in_buf_->num_channels(),
               static_cast<size_t>(msg.input_channel_size()));

  for (int i = 0; i < msg.input_channel_size(); ++i) {
    RTC_CHECK_EQ(in_buf_->num_frames() * sizeof(*in_buf_->channels()[i]),
                 msg.input_channel(i).size());
    std::memcpy(in_buf_->channels()[i], msg.input_channel(i).data(),
                msg.input_channel(i).size());
  }
  {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(kNoErr, ap_->set_stream_delay_ms(msg.delay()));
    ap_->echo_cancellation()->set_stream_drift_samples(msg.drift());
    if (msg.has_keypress()) {
      ap_->set_stream_key_pressed(msg.keypress());
    }
    RTC_CHECK_EQ(kNoErr,
                 ap_->ProcessStream(in_buf_->channels(), input_config_,
                                    output_config_, out_buf_.channels()));
  }

  buffer_writer_.Write(out_buf_);
}

void AecDumpFileProcessor::HandleMessage(const ReverseStream& msg) {
  RTC_CHECK(!msg.has_data());
  RTC_CHECK_EQ(reverse_buf_->num_channels(),
               static_cast<size_t>(msg.channel_size()));

  for (int i = 0; i < msg.channel_size(); ++i) {
    RTC_CHECK_EQ(reverse_buf_->num_frames() * sizeof(*in_buf_->channels()[i]),
                 msg.channel(i).size());
    std::memcpy(reverse_buf_->channels()[i], msg.channel(i).data(),
                msg.channel(i).size());
  }
  {
    const auto st = ScopedTimer(mutable_proc_time());
    // TODO(ajm): This currently discards the processed output, which is needed
    // for e.g. intelligibility enhancement.
    RTC_CHECK_EQ(kNoErr, ap_->ProcessReverseStream(
                             reverse_buf_->channels(), reverse_config_,
                             reverse_config_, reverse_buf_->channels()));
  }
}

}  // namespace webrtc
