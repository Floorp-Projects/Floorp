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

  template <typename Arg,
            typename std::enable_if<!(std::is_invocable<Arg>::value),
                                    bool>::type = true>
  AlwaysValidPointer(Interface* pointer, Arg arg)
      : owned_instance_(pointer ? nullptr
                                : std::make_unique<Default>(std::move(arg))),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  // Multiple arguments
  template <typename Arg1, typename... Args>
  AlwaysValidPointer(Interface* pointer, Arg1 arg1, Args... args)
      : owned_instance_(pointer
                            ? nullptr
                            : std::make_unique<Default>(std::move(arg1),
                                                        std::move(args...))),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) using |pointer|, without taking ownership
  // b) calling |function| and taking ownership of the result
  template <typename Func,
            typename std::enable_if<std::is_invocable<Func>::value,
                                    bool>::type = true>
  AlwaysValidPointer(Interface* pointer, Func function)
      : owned_instance_(pointer ? nullptr : function()),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) taking over ownership of |instance|
  // b) or fallback to |pointer|, without taking ownership.
  // c) or Default.
  AlwaysValidPointer(std::unique_ptr<Interface>&& instance, Interface* pointer)
      : owned_instance_(
            instance
                ? std::move(instance)
                : (pointer == nullptr ? std::make_unique<Default>() : nullptr)),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) taking over ownership of |instance|
  // b) or fallback to |pointer|, without taking ownership.
  // c) or Default (with forwarded args).
  template <typename... Args>
  AlwaysValidPointer(std::unique_ptr<Interface>&& instance,
                     Interface* pointer,
                     Args... args)
      : owned_instance_(
            instance ? std::move(instance)
                     : (pointer == nullptr
                            ? std::make_unique<Default>(std::move(args...))
                            : nullptr)),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
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
