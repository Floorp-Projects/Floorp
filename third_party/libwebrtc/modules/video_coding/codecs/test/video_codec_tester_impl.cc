/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <map>
#include <memory>
#include <utility>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {
namespace test {

namespace {
using RawVideoSource = VideoCodecTester::RawVideoSource;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using Decoder = VideoCodecTester::Decoder;
using Encoder = VideoCodecTester::Encoder;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

// A thread-safe wrapper for video source to be shared with the quality analyzer
// that reads reference frames from a separate thread.
class SyncRawVideoSource : public VideoCodecAnalyzer::ReferenceVideoSource {
 public:
  explicit SyncRawVideoSource(std::unique_ptr<RawVideoSource> video_source)
      : video_source_(std::move(video_source)) {}

  absl::optional<VideoFrame> PullFrame() {
    MutexLock lock(&mutex_);
    return video_source_->PullFrame();
  }

  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    MutexLock lock(&mutex_);
    return video_source_->GetFrame(timestamp_rtp, resolution);
  }

 protected:
  std::unique_ptr<RawVideoSource> video_source_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

// Pacer calculates delay necessary to keep frame encode or decode call spaced
// from the previous calls by the pacing time. `Delay` is expected to be called
// as close as possible to posting frame encode or decode task. This class is
// not thread safe.
class Pacer {
 public:
  explicit Pacer(PacingSettings settings)
      : settings_(settings), delay_(TimeDelta::Zero()) {}
  TimeDelta Delay(Timestamp beat) {
    if (settings_.mode == PacingMode::kNoPacing) {
      return TimeDelta::Zero();
    }

    Timestamp now = Timestamp::Micros(rtc::TimeMicros());
    if (prev_time_.has_value()) {
      delay_ += PacingTime(beat);
      delay_ -= (now - *prev_time_);
      if (delay_.ns() < 0) {
        delay_ = TimeDelta::Zero();
      }
    }

    prev_beat_ = beat;
    prev_time_ = now;
    return delay_;
  }

 private:
  TimeDelta PacingTime(Timestamp beat) {
    if (settings_.mode == PacingMode::kRealTime) {
      return beat - *prev_beat_;
    }
    RTC_CHECK_EQ(PacingMode::kConstantRate, settings_.mode);
    return 1 / settings_.constant_rate;
  }

  PacingSettings settings_;
  absl::optional<Timestamp> prev_beat_;
  absl::optional<Timestamp> prev_time_;
  TimeDelta delay_;
};

// Task queue that keeps the number of queued tasks below a certain limit. If
// the limit is reached, posting of a next task is blocked until execution of a
// previously posted task starts. This class is not thread-safe.
class LimitedTaskQueue {
 public:
  // The codec tester reads frames from video source in the main thread.
  // Encoding and decoding are done in separate threads. If encoding or
  // decoding is slow, the reading may go far ahead and may buffer too many
  // frames in memory. To prevent this we limit the encoding/decoding queue
  // size. When the queue is full, the main thread and, hence, reading frames
  // from video source is blocked until a previously posted encoding/decoding
  // task starts.
  static constexpr int kMaxTaskQueueSize = 3;

  explicit LimitedTaskQueue(rtc::TaskQueue& task_queue)
      : task_queue_(task_queue), queue_size_(0) {}

  void PostDelayedTask(absl::AnyInvocable<void() &&> task, TimeDelta delay) {
    ++queue_size_;
    task_queue_.PostDelayedTask(
        [this, task = std::move(task)]() mutable {
          std::move(task)();
          --queue_size_;
          task_executed_.Set();
        },
        delay);

    task_executed_.Reset();
    if (queue_size_ > kMaxTaskQueueSize) {
      task_executed_.Wait(rtc::Event::kForever);
    }
    RTC_CHECK(queue_size_ <= kMaxTaskQueueSize);
  }

  void WaitForPreviouslyPostedTasks() {
    while (queue_size_ > 0) {
      task_executed_.Wait(rtc::Event::kForever);
      task_executed_.Reset();
    }
  }

  rtc::TaskQueue& task_queue_;
  std::atomic_int queue_size_;
  rtc::Event task_executed_;
};

class TesterDecoder {
 public:
  TesterDecoder(std::unique_ptr<Decoder> decoder,
                VideoCodecAnalyzer* analyzer,
                const DecoderSettings& settings,
                rtc::TaskQueue& task_queue)
      : decoder_(std::move(decoder)),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing),
        task_queue_(task_queue) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";
  }

  void Decode(const EncodedImage& frame) {
    Timestamp timestamp = Timestamp::Micros((frame.Timestamp() / k90kHz).us());

    task_queue_.PostDelayedTask(
        [this, frame] {
          analyzer_->StartDecode(frame);
          decoder_->Decode(
              frame, [this, spatial_idx = frame.SpatialIndex().value_or(0)](
                         const VideoFrame& decoded_frame) {
                this->analyzer_->FinishDecode(decoded_frame, spatial_idx);
              });
        },
        pacer_.Delay(timestamp));
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<Decoder> decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const DecoderSettings& settings_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
};

class TesterEncoder {
 public:
  TesterEncoder(std::unique_ptr<Encoder> encoder,
                TesterDecoder* decoder,
                VideoCodecAnalyzer* analyzer,
                const EncoderSettings& settings,
                rtc::TaskQueue& task_queue)
      : encoder_(std::move(encoder)),
        decoder_(decoder),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing),
        task_queue_(task_queue) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";
  }

  void Encode(const VideoFrame& frame) {
    Timestamp timestamp = Timestamp::Micros((frame.timestamp() / k90kHz).us());

    task_queue_.PostDelayedTask(
        [this, frame] {
          analyzer_->StartEncode(frame);
          encoder_->Encode(frame, [this](const EncodedImage& encoded_frame) {
            this->analyzer_->FinishEncode(encoded_frame);
            if (decoder_ != nullptr) {
              this->decoder_->Decode(encoded_frame);
            }
          });
        },
        pacer_.Delay(timestamp));
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<Encoder> encoder_;
  TesterDecoder* const decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const EncoderSettings& settings_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
};

}  // namespace

VideoCodecTesterImpl::VideoCodecTesterImpl()
    : VideoCodecTesterImpl(/*task_queue_factory=*/nullptr) {}

VideoCodecTesterImpl::VideoCodecTesterImpl(TaskQueueFactory* task_queue_factory)
    : task_queue_factory_(task_queue_factory) {
  if (task_queue_factory_ == nullptr) {
    owned_task_queue_factory_ = CreateDefaultTaskQueueFactory();
    task_queue_factory_ = owned_task_queue_factory_.get();
  }
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunDecodeTest(
    std::unique_ptr<CodedVideoSource> video_source,
    std::unique_ptr<Decoder> decoder,
    const DecoderSettings& decoder_settings) {
  rtc::TaskQueue analyser_task_queue(task_queue_factory_->CreateTaskQueue(
      "Analyzer", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue decoder_task_queue(task_queue_factory_->CreateTaskQueue(
      "Decoder", TaskQueueFactory::Priority::NORMAL));

  VideoCodecAnalyzer perf_analyzer(analyser_task_queue);
  TesterDecoder tester_decoder(std::move(decoder), &perf_analyzer,
                               decoder_settings, decoder_task_queue);

  while (auto frame = video_source->PullFrame()) {
    tester_decoder.Decode(*frame);
  }

  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeTest(
    std::unique_ptr<RawVideoSource> video_source,
    std::unique_ptr<Encoder> encoder,
    const EncoderSettings& encoder_settings) {
  rtc::TaskQueue analyser_task_queue(task_queue_factory_->CreateTaskQueue(
      "Analyzer", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue encoder_task_queue(task_queue_factory_->CreateTaskQueue(
      "Encoder", TaskQueueFactory::Priority::NORMAL));

  SyncRawVideoSource sync_source(std::move(video_source));
  VideoCodecAnalyzer perf_analyzer(analyser_task_queue);
  TesterEncoder tester_encoder(std::move(encoder), /*decoder=*/nullptr,
                               &perf_analyzer, encoder_settings,
                               encoder_task_queue);

  while (auto frame = sync_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    std::unique_ptr<RawVideoSource> video_source,
    std::unique_ptr<Encoder> encoder,
    std::unique_ptr<Decoder> decoder,
    const EncoderSettings& encoder_settings,
    const DecoderSettings& decoder_settings) {
  rtc::TaskQueue analyser_task_queue(task_queue_factory_->CreateTaskQueue(
      "Analyzer", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue decoder_task_queue(task_queue_factory_->CreateTaskQueue(
      "Decoder", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue encoder_task_queue(task_queue_factory_->CreateTaskQueue(
      "Encoder", TaskQueueFactory::Priority::NORMAL));

  SyncRawVideoSource sync_source(std::move(video_source));
  VideoCodecAnalyzer perf_analyzer(analyser_task_queue, &sync_source);
  TesterDecoder tester_decoder(std::move(decoder), &perf_analyzer,
                               decoder_settings, decoder_task_queue);
  TesterEncoder tester_encoder(std::move(encoder), &tester_decoder,
                               &perf_analyzer, encoder_settings,
                               encoder_task_queue);

  while (auto frame = sync_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();
  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

}  // namespace test
}  // namespace webrtc
