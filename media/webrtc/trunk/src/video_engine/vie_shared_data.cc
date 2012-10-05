/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/utility/interface/process_thread.h"
#include "system_wrappers/interface/cpu_info.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_render_manager.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

// Active instance counter
int ViESharedData::instance_counter_ = 0;

ViESharedData::ViESharedData()
    : instance_id_(++instance_counter_),
      initialized_(false),
      number_cores_(CpuInfo::DetectNumberOfCores()),
      over_use_detector_options_(),
      vie_performance_monitor_(ViEPerformanceMonitor(instance_id_)),
      channel_manager_(*new ViEChannelManager(instance_id_, number_cores_,
                                              &vie_performance_monitor_,
                                              over_use_detector_options_)),
      input_manager_(*new ViEInputManager(instance_id_)),
      render_manager_(*new ViERenderManager(instance_id_)),
      module_process_thread_(ProcessThread::CreateProcessThread()),
      last_error_(0) {
  Trace::CreateTrace();
  channel_manager_.SetModuleProcessThread(module_process_thread_);
  input_manager_.SetModuleProcessThread(module_process_thread_);
  module_process_thread_->Start();
}

ViESharedData::~ViESharedData() {
  delete &input_manager_;
  delete &channel_manager_;
  delete &render_manager_;

  module_process_thread_->Stop();
  ProcessThread::DestroyProcessThread(module_process_thread_);
  Trace::ReturnTrace();
}

bool ViESharedData::Initialized() const {
  return initialized_;
}

int ViESharedData::SetInitialized() {
  initialized_ = true;
  return 0;
}

int ViESharedData::SetUnInitialized() {
  initialized_ = false;
  return 0;
}

void ViESharedData::SetLastError(const int error) const {
  last_error_ = error;
}

int ViESharedData::LastErrorInternal() const {
  int error = last_error_;
  last_error_ = 0;
  return error;
}

void ViESharedData::SetOverUseDetectorOptions(
    const OverUseDetectorOptions& options) {
  over_use_detector_options_ = options;
}

int ViESharedData::NumberOfCores() const {
  return number_cores_;
}

}  // namespace webrtc
