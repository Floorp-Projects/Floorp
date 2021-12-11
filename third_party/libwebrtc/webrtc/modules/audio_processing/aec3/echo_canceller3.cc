/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/echo_canceller3.h"

#include <sstream>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {

namespace {

enum class EchoCanceller3ApiCall { kCapture, kRender };

bool DetectSaturation(rtc::ArrayView<const float> y) {
  for (auto y_k : y) {
    if (y_k >= 32700.0f || y_k <= -32700.0f) {
      return true;
    }
  }
  return false;
}

void FillSubFrameView(AudioBuffer* frame,
                      size_t sub_frame_index,
                      std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  RTC_DCHECK_GE(1, sub_frame_index);
  RTC_DCHECK_LE(0, sub_frame_index);
  RTC_DCHECK_EQ(frame->num_bands(), sub_frame_view->size());
  for (size_t k = 0; k < sub_frame_view->size(); ++k) {
    (*sub_frame_view)[k] = rtc::ArrayView<float>(
        &frame->split_bands_f(0)[k][sub_frame_index * kSubFrameLength],
        kSubFrameLength);
  }
}

void FillSubFrameView(std::vector<std::vector<float>>* frame,
                      size_t sub_frame_index,
                      std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  RTC_DCHECK_GE(1, sub_frame_index);
  RTC_DCHECK_EQ(frame->size(), sub_frame_view->size());
  for (size_t k = 0; k < frame->size(); ++k) {
    (*sub_frame_view)[k] = rtc::ArrayView<float>(
        &(*frame)[k][sub_frame_index * kSubFrameLength], kSubFrameLength);
  }
}

void ProcessCaptureFrameContent(
    AudioBuffer* capture,
    bool level_change,
    bool saturated_microphone_signal,
    size_t sub_frame_index,
    FrameBlocker* capture_blocker,
    BlockFramer* output_framer,
    BlockProcessor* block_processor,
    std::vector<std::vector<float>>* block,
    std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  FillSubFrameView(capture, sub_frame_index, sub_frame_view);
  capture_blocker->InsertSubFrameAndExtractBlock(*sub_frame_view, block);
  block_processor->ProcessCapture(level_change, saturated_microphone_signal,
                                  block);
  output_framer->InsertBlockAndExtractSubFrame(*block, sub_frame_view);
}

void ProcessRemainingCaptureFrameContent(
    bool level_change,
    bool saturated_microphone_signal,
    FrameBlocker* capture_blocker,
    BlockFramer* output_framer,
    BlockProcessor* block_processor,
    std::vector<std::vector<float>>* block) {
  if (!capture_blocker->IsBlockAvailable()) {
    return;
  }

  capture_blocker->ExtractBlock(block);
  block_processor->ProcessCapture(level_change, saturated_microphone_signal,
                                  block);
  output_framer->InsertBlock(*block);
}

void BufferRenderFrameContent(
    std::vector<std::vector<float>>* render_frame,
    size_t sub_frame_index,
    FrameBlocker* render_blocker,
    BlockProcessor* block_processor,
    std::vector<std::vector<float>>* block,
    std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  FillSubFrameView(render_frame, sub_frame_index, sub_frame_view);
  render_blocker->InsertSubFrameAndExtractBlock(*sub_frame_view, block);
  block_processor->BufferRender(*block);
}

void BufferRemainingRenderFrameContent(FrameBlocker* render_blocker,
                                       BlockProcessor* block_processor,
                                       std::vector<std::vector<float>>* block) {
  if (!render_blocker->IsBlockAvailable()) {
    return;
  }
  render_blocker->ExtractBlock(block);
  block_processor->BufferRender(*block);
}

void CopyBufferIntoFrame(AudioBuffer* buffer,
                         size_t num_bands,
                         size_t frame_length,
                         std::vector<std::vector<float>>* frame) {
  RTC_DCHECK_EQ(num_bands, frame->size());
  RTC_DCHECK_EQ(frame_length, (*frame)[0].size());
  for (size_t k = 0; k < num_bands; ++k) {
    rtc::ArrayView<float> buffer_view(&buffer->split_bands_f(0)[k][0],
                                      frame_length);
    std::copy(buffer_view.begin(), buffer_view.end(), (*frame)[k].begin());
  }
}

// [B,A] = butter(2,100/4000,'high')
const CascadedBiQuadFilter::BiQuadCoefficients
    kHighPassFilterCoefficients_8kHz = {{0.94598f, -1.89195f, 0.94598f},
                                        {-1.88903f, 0.89487f}};
const int kNumberOfHighPassBiQuads_8kHz = 1;

// [B,A] = butter(2,100/8000,'high')
const CascadedBiQuadFilter::BiQuadCoefficients
    kHighPassFilterCoefficients_16kHz = {{0.97261f, -1.94523f, 0.97261f},
                                         {-1.94448f, 0.94598f}};
const int kNumberOfHighPassBiQuads_16kHz = 1;

}  // namespace

class EchoCanceller3::RenderWriter {
 public:
  RenderWriter(ApmDataDumper* data_dumper,
               SwapQueue<std::vector<std::vector<float>>,
                         Aec3RenderQueueItemVerifier>* render_transfer_queue,
               std::unique_ptr<CascadedBiQuadFilter> render_highpass_filter,
               int sample_rate_hz,
               int frame_length,
               int num_bands);
  ~RenderWriter();
  void Insert(AudioBuffer* input);

 private:
  ApmDataDumper* data_dumper_;
  const int sample_rate_hz_;
  const size_t frame_length_;
  const int num_bands_;
  std::unique_ptr<CascadedBiQuadFilter> render_highpass_filter_;
  std::vector<std::vector<float>> render_queue_input_frame_;
  SwapQueue<std::vector<std::vector<float>>, Aec3RenderQueueItemVerifier>*
      render_transfer_queue_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWriter);
};

EchoCanceller3::RenderWriter::RenderWriter(
    ApmDataDumper* data_dumper,
    SwapQueue<std::vector<std::vector<float>>, Aec3RenderQueueItemVerifier>*
        render_transfer_queue,
    std::unique_ptr<CascadedBiQuadFilter> render_highpass_filter,
    int sample_rate_hz,
    int frame_length,
    int num_bands)
    : data_dumper_(data_dumper),
      sample_rate_hz_(sample_rate_hz),
      frame_length_(frame_length),
      num_bands_(num_bands),
      render_highpass_filter_(std::move(render_highpass_filter)),
      render_queue_input_frame_(num_bands_,
                                std::vector<float>(frame_length_, 0.f)),
      render_transfer_queue_(render_transfer_queue) {
  RTC_DCHECK(data_dumper);
}

EchoCanceller3::RenderWriter::~RenderWriter() = default;

void EchoCanceller3::RenderWriter::Insert(AudioBuffer* input) {
  RTC_DCHECK_EQ(1, input->num_channels());
  RTC_DCHECK_EQ(frame_length_, input->num_frames_per_band());
  data_dumper_->DumpWav("aec3_render_input", frame_length_,
                        &input->split_bands_f(0)[0][0],
                        LowestBandRate(sample_rate_hz_), 1);

  CopyBufferIntoFrame(input, num_bands_, frame_length_,
                      &render_queue_input_frame_);

  if (render_highpass_filter_) {
    render_highpass_filter_->Process(render_queue_input_frame_[0]);
  }

  static_cast<void>(render_transfer_queue_->Insert(&render_queue_input_frame_));
}

int EchoCanceller3::instance_count_ = 0;

EchoCanceller3::EchoCanceller3(const EchoCanceller3Config& config,
                               int sample_rate_hz,
                               bool use_highpass_filter)
    : EchoCanceller3(sample_rate_hz,
                     use_highpass_filter,
                     std::unique_ptr<BlockProcessor>(
                         BlockProcessor::Create(config, sample_rate_hz))) {}
EchoCanceller3::EchoCanceller3(int sample_rate_hz,
                               bool use_highpass_filter,
                               std::unique_ptr<BlockProcessor> block_processor)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      sample_rate_hz_(sample_rate_hz),
      num_bands_(NumBandsForRate(sample_rate_hz_)),
      frame_length_(rtc::CheckedDivExact(LowestBandRate(sample_rate_hz_), 100)),
      output_framer_(num_bands_),
      capture_blocker_(num_bands_),
      render_blocker_(num_bands_),
      render_transfer_queue_(
          kRenderTransferQueueSize,
          std::vector<std::vector<float>>(
              num_bands_,
              std::vector<float>(frame_length_, 0.f)),
          Aec3RenderQueueItemVerifier(num_bands_, frame_length_)),
      block_processor_(std::move(block_processor)),
      render_queue_output_frame_(num_bands_,
                                 std::vector<float>(frame_length_, 0.f)),
      block_(num_bands_, std::vector<float>(kBlockSize, 0.f)),
      sub_frame_view_(num_bands_) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz_));

  std::unique_ptr<CascadedBiQuadFilter> render_highpass_filter;
  if (use_highpass_filter) {
    render_highpass_filter.reset(new CascadedBiQuadFilter(
        sample_rate_hz_ == 8000 ? kHighPassFilterCoefficients_8kHz
                                : kHighPassFilterCoefficients_16kHz,
        sample_rate_hz_ == 8000 ? kNumberOfHighPassBiQuads_8kHz
                                : kNumberOfHighPassBiQuads_16kHz));
    capture_highpass_filter_.reset(new CascadedBiQuadFilter(
        sample_rate_hz_ == 8000 ? kHighPassFilterCoefficients_8kHz
                                : kHighPassFilterCoefficients_16kHz,
        sample_rate_hz_ == 8000 ? kNumberOfHighPassBiQuads_8kHz
                                : kNumberOfHighPassBiQuads_16kHz));
  }

  render_writer_.reset(
      new RenderWriter(data_dumper_.get(), &render_transfer_queue_,
                       std::move(render_highpass_filter), sample_rate_hz_,
                       frame_length_, num_bands_));

  RTC_DCHECK_EQ(num_bands_, std::max(sample_rate_hz_, 16000) / 16000);
  RTC_DCHECK_GE(kMaxNumBands, num_bands_);
}

EchoCanceller3::~EchoCanceller3() = default;

void EchoCanceller3::AnalyzeRender(AudioBuffer* render) {
  RTC_DCHECK_RUNS_SERIALIZED(&render_race_checker_);
  RTC_DCHECK(render);
  data_dumper_->DumpRaw("aec3_call_order",
                        static_cast<int>(EchoCanceller3ApiCall::kRender));

  return render_writer_->Insert(render);
}

void EchoCanceller3::AnalyzeCapture(AudioBuffer* capture) {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  RTC_DCHECK(capture);
  data_dumper_->DumpWav("aec3_capture_analyze_input", capture->num_frames(),
                        capture->channels_f()[0], sample_rate_hz_, 1);

  saturated_microphone_signal_ = false;
  for (size_t k = 0; k < capture->num_channels(); ++k) {
    saturated_microphone_signal_ |=
        DetectSaturation(rtc::ArrayView<const float>(capture->channels_f()[k],
                                                     capture->num_frames()));
    if (saturated_microphone_signal_) {
      break;
    }
  }
}

void EchoCanceller3::ProcessCapture(AudioBuffer* capture, bool level_change) {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  RTC_DCHECK(capture);
  RTC_DCHECK_EQ(1u, capture->num_channels());
  RTC_DCHECK_EQ(num_bands_, capture->num_bands());
  RTC_DCHECK_EQ(frame_length_, capture->num_frames_per_band());
  data_dumper_->DumpRaw("aec3_call_order",
                        static_cast<int>(EchoCanceller3ApiCall::kCapture));

  rtc::ArrayView<float> capture_lower_band =
      rtc::ArrayView<float>(&capture->split_bands_f(0)[0][0], frame_length_);

  data_dumper_->DumpWav("aec3_capture_input", capture_lower_band,
                        LowestBandRate(sample_rate_hz_), 1);

  EmptyRenderQueue();

  if (capture_highpass_filter_) {
    capture_highpass_filter_->Process(capture_lower_band);
  }

  ProcessCaptureFrameContent(
      capture, level_change, saturated_microphone_signal_, 0, &capture_blocker_,
      &output_framer_, block_processor_.get(), &block_, &sub_frame_view_);

  if (sample_rate_hz_ != 8000) {
    ProcessCaptureFrameContent(
        capture, level_change, saturated_microphone_signal_, 1,
        &capture_blocker_, &output_framer_, block_processor_.get(), &block_,
        &sub_frame_view_);
  }

  ProcessRemainingCaptureFrameContent(
      level_change, saturated_microphone_signal_, &capture_blocker_,
      &output_framer_, block_processor_.get(), &block_);

  data_dumper_->DumpWav("aec3_capture_output", frame_length_,
                        &capture->split_bands_f(0)[0][0],
                        LowestBandRate(sample_rate_hz_), 1);
}

EchoControl::Metrics EchoCanceller3::GetMetrics() const {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  Metrics metrics;
  block_processor_->GetMetrics(&metrics);
  return metrics;
}

bool EchoCanceller3::Validate(const EchoCanceller3Config& config) {
  return true;
}

void EchoCanceller3::EmptyRenderQueue() {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  bool frame_to_buffer =
      render_transfer_queue_.Remove(&render_queue_output_frame_);
  while (frame_to_buffer) {
    BufferRenderFrameContent(&render_queue_output_frame_, 0, &render_blocker_,
                             block_processor_.get(), &block_, &sub_frame_view_);

    if (sample_rate_hz_ != 8000) {
      BufferRenderFrameContent(&render_queue_output_frame_, 1, &render_blocker_,
                               block_processor_.get(), &block_,
                               &sub_frame_view_);
    }

    BufferRemainingRenderFrameContent(&render_blocker_, block_processor_.get(),
                                      &block_);

    frame_to_buffer =
        render_transfer_queue_.Remove(&render_queue_output_frame_);
  }
}

EchoCanceller3Factory::EchoCanceller3Factory() {}

EchoCanceller3Factory::EchoCanceller3Factory(const EchoCanceller3Config& config)
    : config_(config) {
  // Revert to default configuration if needed.
  if (!EchoCanceller3::Validate(config_)) {
    config_ = EchoCanceller3Config();
  }
}

std::unique_ptr<EchoControl> EchoCanceller3Factory::Create(int sample_rate_hz) {
  return std::unique_ptr<EchoControl>(
      new EchoCanceller3(config_, sample_rate_hz, true));
}

}  // namespace webrtc
