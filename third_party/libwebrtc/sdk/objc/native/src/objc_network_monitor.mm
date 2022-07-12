/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_network_monitor.h"
#include "absl/strings/string_view.h"

#include "rtc_base/task_utils/to_queued_task.h"

#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"

namespace webrtc {

rtc::NetworkMonitorInterface* ObjCNetworkMonitorFactory::CreateNetworkMonitor(
    const WebRtcKeyValueConfig& field_trials) {
  return new ObjCNetworkMonitor();
}

ObjCNetworkMonitor::ObjCNetworkMonitor() {
  safety_flag_ = PendingTaskSafetyFlag::Create();
}

ObjCNetworkMonitor::~ObjCNetworkMonitor() {
  [network_monitor_ stop];
  network_monitor_ = nil;
}

void ObjCNetworkMonitor::Start() {
  if (started_) {
    return;
  }
  thread_ = rtc::Thread::Current();
  RTC_DCHECK_RUN_ON(thread_);
  safety_flag_->SetAlive();
  network_monitor_ = [[RTCNetworkMonitor alloc] initWithObserver:this];
  if (network_monitor_ == nil) {
    RTC_LOG(LS_WARNING) << "Failed to create RTCNetworkMonitor; not available on this OS?";
  }
  started_ = true;
}

void ObjCNetworkMonitor::Stop() {
  RTC_DCHECK_RUN_ON(thread_);
  if (!started_) {
    return;
  }
  safety_flag_->SetNotAlive();
  [network_monitor_ stop];
  network_monitor_ = nil;
  started_ = false;
}

rtc::AdapterType ObjCNetworkMonitor::GetAdapterType(absl::string_view interface_name) {
  RTC_DCHECK_RUN_ON(thread_);
  auto iter = adapter_type_by_name_.find(interface_name);
  if (iter == adapter_type_by_name_.end()) {
    return rtc::ADAPTER_TYPE_UNKNOWN;
  }
  return iter->second;
}

rtc::AdapterType ObjCNetworkMonitor::GetVpnUnderlyingAdapterType(absl::string_view interface_name) {
  return rtc::ADAPTER_TYPE_UNKNOWN;
}

rtc::NetworkPreference ObjCNetworkMonitor::GetNetworkPreference(absl::string_view interface_name) {
  return rtc::NetworkPreference::NEUTRAL;
}

bool ObjCNetworkMonitor::IsAdapterAvailable(absl::string_view interface_name) {
  RTC_DCHECK_RUN_ON(thread_);
  if (adapter_type_by_name_.empty()) {
    // If we have no path update, assume everything's available, because it's
    // preferable for WebRTC to try all interfaces rather than none at all.
    return true;
  }
  return adapter_type_by_name_.find(interface_name) != adapter_type_by_name_.end();
}

void ObjCNetworkMonitor::OnPathUpdate(
    std::map<std::string, rtc::AdapterType, rtc::AbslStringViewCmp> adapter_type_by_name) {
  RTC_DCHECK(network_monitor_ != nil);
  thread_->PostTask(ToQueuedTask(safety_flag_, [this, adapter_type_by_name] {
    RTC_DCHECK_RUN_ON(thread_);
    adapter_type_by_name_ = adapter_type_by_name;
    InvokeNetworksChangedCallback();
  }));
}

}  // namespace webrtc
