/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESharedData contains data and instances common to all interface
// implementations.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_

#include <map>

#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

class Config;
class CpuOveruseObserver;
class ProcessThread;
class ViEChannelManager;
class ViEInputManager;
class ViERenderManager;

class ViESharedData {
 public:
  explicit ViESharedData(const Config& config);
  ~ViESharedData();

  void SetLastError(const int error) const;
  int LastErrorInternal() const;
  int NumberOfCores() const;

  // TODO(mflodman) Remove all calls to 'instance_id()'.
  int instance_id() { return 0;}
  ViEChannelManager* channel_manager() { return channel_manager_.get(); }
  ViEInputManager* input_manager() { return input_manager_.get(); }
  ViERenderManager* render_manager() { return render_manager_.get(); }

  std::map<int, CpuOveruseObserver*>* overuse_observers() {
    return &overuse_observers_; }

  CPULoadStateCallbackInvoker* load_manager() { return load_manager_; }
  void set_load_manager(CPULoadStateCallbackInvoker* load_manager);

 private:
  const int number_cores_;

  rtc::scoped_ptr<ViEChannelManager> channel_manager_;
  rtc::scoped_ptr<ViEInputManager> input_manager_;
  rtc::scoped_ptr<ViERenderManager> render_manager_;
  rtc::scoped_ptr<ProcessThread> module_process_thread_;
  // Owned by PeerConnection, not ViEEngine
  CPULoadStateCallbackInvoker* load_manager_;
  mutable int last_error_;

  std::map<int, CpuOveruseObserver*> overuse_observers_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SHARED_DATA_H_
