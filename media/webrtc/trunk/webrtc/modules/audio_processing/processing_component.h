/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_PROCESSING_COMPONENT_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_PROCESSING_COMPONENT_H_

#include <vector>

#include "webrtc/common.h"

namespace webrtc {

// Functor to use when supplying a verifier function for the queue item
// verifcation.
template <typename T>
class RenderQueueItemVerifier {
 public:
  explicit RenderQueueItemVerifier(size_t minimum_capacity)
      : minimum_capacity_(minimum_capacity) {}

  bool operator()(const std::vector<T>& v) const {
    return v.capacity() >= minimum_capacity_;
  }

 private:
  size_t minimum_capacity_;
};

class ProcessingComponent {
 public:
  ProcessingComponent();
  virtual ~ProcessingComponent();

  virtual int Initialize();
  virtual void SetExtraOptions(const Config& config) {}
  virtual int Destroy();

  bool is_component_enabled() const;

 protected:
  virtual int Configure();
  int EnableComponent(bool enable);
  void* handle(size_t index) const;
  size_t num_handles() const;

 private:
  virtual void* CreateHandle() const = 0;
  virtual int InitializeHandle(void* handle) const = 0;
  virtual int ConfigureHandle(void* handle) const = 0;
  virtual void DestroyHandle(void* handle) const = 0;
  virtual size_t num_handles_required() const = 0;
  virtual int GetHandleError(void* handle) const = 0;

  std::vector<void*> handles_;
  bool initialized_;
  bool enabled_;
  size_t num_handles_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_PROCESSING_COMPONENT_H__
