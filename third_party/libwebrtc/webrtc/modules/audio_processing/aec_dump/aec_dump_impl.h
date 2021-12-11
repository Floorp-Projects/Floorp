/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "modules/audio_processing/aec_dump/capture_stream_info.h"
#include "modules/audio_processing/aec_dump/write_to_file_task.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/platform_file.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/file_wrapper.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "modules/audio_processing/debug.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace rtc {
class TaskQueue;
}  // namespace rtc

namespace webrtc {

// Task-queue based implementation of AecDump. It is thread safe by
// relying on locks in TaskQueue.
class AecDumpImpl : public AecDump {
 public:
  // Does member variables initialization shared across all c-tors.
  AecDumpImpl(std::unique_ptr<FileWrapper> debug_file,
              int64_t max_log_size_bytes,
              rtc::TaskQueue* worker_queue);

  ~AecDumpImpl() override;

  void WriteInitMessage(const InternalAPMStreamsConfig& api_format) override;

  void AddCaptureStreamInput(const FloatAudioFrame& src) override;
  void AddCaptureStreamOutput(const FloatAudioFrame& src) override;
  void AddCaptureStreamInput(const AudioFrame& frame) override;
  void AddCaptureStreamOutput(const AudioFrame& frame) override;
  void AddAudioProcessingState(const AudioProcessingState& state) override;
  void WriteCaptureStreamMessage() override;

  void WriteRenderStreamMessage(const AudioFrame& frame) override;
  void WriteRenderStreamMessage(const FloatAudioFrame& src) override;

  void WriteConfig(const InternalAPMConfig& config) override;

 private:
  std::unique_ptr<WriteToFileTask> CreateWriteToFileTask();

  std::unique_ptr<FileWrapper> debug_file_;
  int64_t num_bytes_left_for_log_ = 0;
  rtc::RaceChecker race_checker_;
  rtc::TaskQueue* worker_queue_;
  CaptureStreamInfo capture_stream_info_;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_IMPL_H_
