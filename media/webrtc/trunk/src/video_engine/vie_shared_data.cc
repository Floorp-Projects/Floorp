/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "cpu_info.h"
#include "process_thread.h"
#include "trace.h"
#include "vie_channel_manager.h"
#include "vie_defines.h"
#include "vie_input_manager.h"
#include "vie_render_manager.h"
#include "vie_shared_data.h"

namespace webrtc {

// Active instance counter
int ViESharedData::instance_counter_ = 0;

ViESharedData::ViESharedData()
    : instance_id_(++instance_counter_),
      initialized_(false),
      number_cores_(CpuInfo::DetectNumberOfCores()),
      vie_performance_monitor_(ViEPerformanceMonitor(instance_id_)),
      channel_manager_(*new ViEChannelManager(instance_id_, number_cores_,
                                              vie_performance_monitor_)),
      input_manager_(*new ViEInputManager(instance_id_)),
      render_manager_(*new ViERenderManager(instance_id_)),
      module_process_thread_(ProcessThread::CreateProcessThread()),
      last_error_(0) {
  Trace::CreateTrace();
  channel_manager_.SetModuleProcessThread(*module_process_thread_);
  input_manager_.SetModuleProcessThread(*module_process_thread_);
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

int ViESharedData::NumberOfCores() const {
  return number_cores_;
}

}  // namespace webrtc
