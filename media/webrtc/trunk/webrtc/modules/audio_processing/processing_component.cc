/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/processing_component.h"

#include <assert.h>

#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

ProcessingComponent::ProcessingComponent()
  : initialized_(false),
    enabled_(false),
    num_handles_(0) {}

ProcessingComponent::~ProcessingComponent() {
  assert(initialized_ == false);
}

int ProcessingComponent::Destroy() {
  while (!handles_.empty()) {
    DestroyHandle(handles_.back());
    handles_.pop_back();
  }
  initialized_ = false;

  return AudioProcessing::kNoError;
}

int ProcessingComponent::EnableComponent(bool enable) {
  if (enable && !enabled_) {
    enabled_ = enable; // Must be set before Initialize() is called.

    int err = Initialize();
    if (err != AudioProcessing::kNoError) {
      enabled_ = false;
      return err;
    }
  } else {
    enabled_ = enable;
  }

  return AudioProcessing::kNoError;
}

bool ProcessingComponent::is_component_enabled() const {
  return enabled_;
}

void* ProcessingComponent::handle(size_t index) const {
  assert(index < num_handles_);
  return handles_[index];
}

size_t ProcessingComponent::num_handles() const {
  return num_handles_;
}

int ProcessingComponent::Initialize() {
  if (!enabled_) {
    return AudioProcessing::kNoError;
  }

  num_handles_ = num_handles_required();
  if (num_handles_ > handles_.size()) {
    handles_.resize(num_handles_, NULL);
  }

  assert(handles_.size() >= num_handles_);
  for (size_t i = 0; i < num_handles_; i++) {
    if (handles_[i] == NULL) {
      handles_[i] = CreateHandle();
      if (handles_[i] == NULL) {
        return AudioProcessing::kCreationFailedError;
      }
    }

    int err = InitializeHandle(handles_[i]);
    if (err != AudioProcessing::kNoError) {
      return GetHandleError(handles_[i]);
    }
  }

  initialized_ = true;
  return Configure();
}

int ProcessingComponent::Configure() {
  if (!initialized_) {
    return AudioProcessing::kNoError;
  }

  assert(handles_.size() >= num_handles_);
  for (size_t i = 0; i < num_handles_; i++) {
    int err = ConfigureHandle(handles_[i]);
    if (err != AudioProcessing::kNoError) {
      return GetHandleError(handles_[i]);
    }
  }

  return AudioProcessing::kNoError;
}
}  // namespace webrtc
