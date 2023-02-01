/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"

#include <stdio.h>

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_decoder.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"
#include "test/testsupport/fixed_fps_video_frame_writer_adapter.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using EmulatedSFUConfigMap =
    ::webrtc::webrtc_pc_e2e::QualityAnalyzingVideoEncoder::EmulatedSFUConfigMap;

class AnalyzingFramePreprocessor
    : public test::TestVideoCapturer::FramePreprocessor {
 public:
  AnalyzingFramePreprocessor(
      absl::string_view peer_name,
      absl::string_view stream_label,
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : peer_name_(peer_name),
        stream_label_(stream_label),
        analyzer_(analyzer),
        sinks_(std::move(sinks)) {}
  ~AnalyzingFramePreprocessor() override = default;

  VideoFrame Preprocess(const VideoFrame& source_frame) override {
    // Copy VideoFrame to be able to set id on it.
    VideoFrame frame = source_frame;
    uint16_t frame_id =
        analyzer_->OnFrameCaptured(peer_name_, stream_label_, frame);
    frame.set_id(frame_id);

    for (auto& sink : sinks_) {
      sink->OnFrame(frame);
    }
    return frame;
  }

 private:
  const std::string peer_name_;
  const std::string stream_label_;
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

}  // namespace

VideoQualityAnalyzerInjectionHelper::VideoFrameIdsWriter::VideoFrameIdsWriter(
    absl::string_view file_name)
    : file_name_(file_name) {
  output_file_ = fopen(file_name_.c_str(), "wb");
  RTC_CHECK(output_file_ != nullptr)
      << "Failed to open file to dump frame ids for writing: " << file_name_;
}

VideoQualityAnalyzerInjectionHelper::VideoFrameIdsWriter::
    ~VideoFrameIdsWriter() {
  fclose(output_file_);
}

void VideoQualityAnalyzerInjectionHelper::VideoFrameIdsWriter::WriteFrameId(
    uint16_t frame_id) {
  int chars_written = fprintf(output_file_, "%d\n", frame_id);
  RTC_CHECK_GE(chars_written, 2)
      << "Failed to write frame id to the output file: " << file_name_;
}

VideoQualityAnalyzerInjectionHelper::VideoWriter::VideoWriter(
    test::VideoFrameWriter* video_writer,
    VideoFrameIdsWriter* frame_ids_writer,
    int sampling_modulo)
    : video_writer_(video_writer),
      frame_ids_writer_(frame_ids_writer),
      sampling_modulo_(sampling_modulo) {}

void VideoQualityAnalyzerInjectionHelper::VideoWriter::OnFrame(
    const VideoFrame& frame) {
  if (frames_counter_++ % sampling_modulo_ != 0) {
    return;
  }
  bool result = video_writer_->WriteFrame(frame);
  RTC_CHECK(result) << "Failed to write frame";
  if (frame_ids_writer_) {
    frame_ids_writer_->WriteFrameId(frame.id());
  }
}

VideoQualityAnalyzerInjectionHelper::VideoQualityAnalyzerInjectionHelper(
    Clock* clock,
    std::unique_ptr<VideoQualityAnalyzerInterface> analyzer,
    EncodedImageDataInjector* injector,
    EncodedImageDataExtractor* extractor)
    : clock_(clock),
      analyzer_(std::move(analyzer)),
      injector_(injector),
      extractor_(extractor) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(injector_);
  RTC_DCHECK(extractor_);
}
VideoQualityAnalyzerInjectionHelper::~VideoQualityAnalyzerInjectionHelper() =
    default;

std::unique_ptr<VideoEncoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoEncoderFactory(
    absl::string_view peer_name,
    std::unique_ptr<VideoEncoderFactory> delegate,
    double bitrate_multiplier,
    EmulatedSFUConfigMap stream_to_sfu_config) const {
  return std::make_unique<QualityAnalyzingVideoEncoderFactory>(
      peer_name, std::move(delegate), bitrate_multiplier,
      std::move(stream_to_sfu_config), injector_, analyzer_.get());
}

std::unique_ptr<VideoDecoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoDecoderFactory(
    absl::string_view peer_name,
    std::unique_ptr<VideoDecoderFactory> delegate) const {
  return std::make_unique<QualityAnalyzingVideoDecoderFactory>(
      peer_name, std::move(delegate), extractor_, analyzer_.get());
}

std::unique_ptr<test::TestVideoCapturer::FramePreprocessor>
VideoQualityAnalyzerInjectionHelper::CreateFramePreprocessor(
    absl::string_view peer_name,
    const VideoConfig& config) {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  test::VideoFrameWriter* writer = nullptr;
  if (config.input_dump_options.has_value()) {
    writer = CreateVideoWriter(
        config.input_dump_options->GetInputDumpFileName(*config.stream_label),
        config);
    VideoFrameIdsWriter* frame_ids_writer = MaybeCreateVideoFrameIdsWriter(
        config.input_dump_options->GetInputFrameIdsDumpFileName(
            *config.stream_label));
    sinks.push_back(std::make_unique<VideoWriter>(
        writer, frame_ids_writer,
        config.input_dump_options->sampling_modulo()));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-capture").c_str(),
                                    config.width, config.height)));
  }
  {
    MutexLock lock(&mutex_);
    known_video_configs_.insert({*config.stream_label, config});
  }
  return std::make_unique<AnalyzingFramePreprocessor>(
      peer_name, std::move(*config.stream_label), analyzer_.get(),
      std::move(sinks));
}

std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>
VideoQualityAnalyzerInjectionHelper::CreateVideoSink(
    absl::string_view peer_name) {
  return std::make_unique<AnalyzingVideoSink>(peer_name, this);
}

void VideoQualityAnalyzerInjectionHelper::Start(
    std::string test_case_name,
    rtc::ArrayView<const std::string> peer_names,
    int max_threads_count) {
  analyzer_->Start(std::move(test_case_name), peer_names, max_threads_count);
  extractor_->Start(peer_names.size());
  MutexLock lock(&mutex_);
  peers_count_ = peer_names.size();
}

void VideoQualityAnalyzerInjectionHelper::RegisterParticipantInCall(
    absl::string_view peer_name) {
  analyzer_->RegisterParticipantInCall(peer_name);
  extractor_->AddParticipantInCall();
  MutexLock lock(&mutex_);
  peers_count_++;
}

void VideoQualityAnalyzerInjectionHelper::UnregisterParticipantInCall(
    absl::string_view peer_name) {
  analyzer_->UnregisterParticipantInCall(peer_name);
  extractor_->RemoveParticipantInCall();
  MutexLock lock(&mutex_);
  peers_count_--;
}

void VideoQualityAnalyzerInjectionHelper::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  analyzer_->OnStatsReports(pc_label, report);
}

void VideoQualityAnalyzerInjectionHelper::Stop() {
  analyzer_->Stop();
  for (const auto& video_writer : video_writers_) {
    video_writer->Close();
  }
  video_writers_.clear();
  frame_ids_writers_.clear();
}

test::VideoFrameWriter* VideoQualityAnalyzerInjectionHelper::CreateVideoWriter(
    absl::string_view file_name,
    const PeerConnectionE2EQualityTestFixture::VideoConfig& config) {
  // TODO(titovartem) create only one file writer for simulcast video track.
  // For now this code will be invoked for each simulcast stream separately, but
  // only one file will be used.
  std::unique_ptr<test::VideoFrameWriter> video_writer =
      std::make_unique<test::Y4mVideoFrameWriterImpl>(
          std::string(file_name), config.width, config.height, config.fps);
  if (config.output_dump_use_fixed_framerate) {
    video_writer = std::make_unique<test::FixedFpsVideoFrameWriterAdapter>(
        config.fps, clock_, std::move(video_writer));
  }
  test::VideoFrameWriter* out = video_writer.get();
  video_writers_.push_back(std::move(video_writer));
  return out;
}

VideoQualityAnalyzerInjectionHelper::VideoFrameIdsWriter*
VideoQualityAnalyzerInjectionHelper::MaybeCreateVideoFrameIdsWriter(
    absl::optional<std::string> frame_ids_dump_file_name) {
  if (!frame_ids_dump_file_name.has_value()) {
    return nullptr;
  }
  frame_ids_writers_.push_back(
      std::make_unique<VideoFrameIdsWriter>(*frame_ids_dump_file_name));
  return frame_ids_writers_.back().get();
}

void VideoQualityAnalyzerInjectionHelper::OnFrame(absl::string_view peer_name,
                                                  const VideoFrame& frame) {
  rtc::scoped_refptr<I420BufferInterface> i420_buffer =
      frame.video_frame_buffer()->ToI420();
  if (IsDummyFrameBuffer(i420_buffer)) {
    // This is dummy frame, so we  don't need to process it further.
    return;
  }
  // Copy entire video frame including video buffer to ensure that analyzer
  // won't hold any WebRTC internal buffers.
  VideoFrame frame_copy = frame;
  frame_copy.set_video_frame_buffer(I420Buffer::Copy(*i420_buffer));
  analyzer_->OnFrameRendered(peer_name, frame_copy);

  if (frame.id() != VideoFrame::kNotSetId) {
    std::string stream_label = analyzer_->GetStreamLabel(frame.id());
    std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>* sinks =
        PopulateSinks(ReceiverStream(peer_name, stream_label));
    if (sinks == nullptr) {
      return;
    }
    for (auto& sink : *sinks) {
      sink->OnFrame(frame);
    }
  }
}

std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>*
VideoQualityAnalyzerInjectionHelper::PopulateSinks(
    const ReceiverStream& receiver_stream) {
  MutexLock lock(&mutex_);
  auto sinks_it = sinks_.find(receiver_stream);
  if (sinks_it != sinks_.end()) {
    return &sinks_it->second;
  }
  auto it = known_video_configs_.find(receiver_stream.stream_label);
  RTC_DCHECK(it != known_video_configs_.end())
      << "No video config for stream " << receiver_stream.stream_label;
  const VideoConfig& config = it->second;

  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  test::VideoFrameWriter* writer = nullptr;
  if (config.output_dump_options.has_value()) {
    writer = CreateVideoWriter(
        config.output_dump_options->GetOutputDumpFileName(
            receiver_stream.stream_label, receiver_stream.peer_name),
        config);
    VideoFrameIdsWriter* frame_ids_writer = MaybeCreateVideoFrameIdsWriter(
        config.output_dump_options->GetOutputFrameIdsDumpFileName(
            receiver_stream.stream_label, receiver_stream.peer_name));
    sinks.push_back(std::make_unique<VideoWriter>(
        writer, frame_ids_writer,
        config.output_dump_options->sampling_modulo()));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-render").c_str(),
                                    config.width, config.height)));
  }
  sinks_.insert({receiver_stream, std::move(sinks)});
  return &(sinks_.find(receiver_stream)->second);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
