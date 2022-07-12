/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_
#define RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_

#include <memory>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

// This template allows the instantiation of a pointer to Interface in such a
// way that if it is passed a null pointer, an object of class Default will be
// created, which will be deallocated when the pointer is deleted.
template <typename Interface, typename Default = Interface>
class AlwaysValidPointer {
 public:
  explicit AlwaysValidPointer(Interface* pointer)
      : owned_instance_(pointer ? nullptr : std::make_unique<Default>()),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  template <typename... Args>
  AlwaysValidPointer(Interface* pointer, Args... args)
      : owned_instance_(
            pointer ? nullptr : std::make_unique<Default>(std::move(args...))),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  Interface* get() { return pointer_; }
  Interface* operator->() { return pointer_; }
  Interface& operator*() { return *pointer_; }

  Interface* get() const { return pointer_; }
  Interface* operator->() const { return pointer_; }
  Interface& operator*() const { return *pointer_; }

 private:
  const std::unique_ptr<Interface> owned_instance_;
  Interface* const pointer_;
};

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_
