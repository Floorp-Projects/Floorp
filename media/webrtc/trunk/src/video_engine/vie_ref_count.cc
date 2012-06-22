/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_ref_count.h"

#include "critical_section_wrapper.h"

namespace webrtc {

ViERefCount::ViERefCount()
    : count_(0),
      crit_(CriticalSectionWrapper::CreateCriticalSection()) {
}

ViERefCount::~ViERefCount() {
}

ViERefCount& ViERefCount::operator++(int) {
  CriticalSectionScoped lock(crit_.get());
  count_++;
  return *this;
}

ViERefCount& ViERefCount::operator--(int) {
  CriticalSectionScoped lock(crit_.get());
  count_--;
  return *this;
}

void ViERefCount::Reset() {
  CriticalSectionScoped lock(crit_.get());
  count_ = 0;
}

int ViERefCount::GetCount() const {
  return count_;
}

}  // namespace webrtc
