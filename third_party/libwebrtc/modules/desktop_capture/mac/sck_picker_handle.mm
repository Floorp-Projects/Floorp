/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sck_picker_handle.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "absl/base/attributes.h"
#include "rtc_base/synchronization/mutex.h"

#include <memory>
#include <optional>

namespace webrtc {

class SckPickerProxy;

class API_AVAILABLE(macos(14.0)) SckPickerProxy {
 public:
  static SckPickerProxy* Get() {
    static SckPickerProxy* sPicker = new SckPickerProxy();
    return sPicker;
  }

  bool AtCapacity() const {
    MutexLock lock(&mutex_);
    return AtCapacityLocked();
  }

  SCContentSharingPicker* GetPicker() const { return SCContentSharingPicker.sharedPicker; }

  ABSL_MUST_USE_RESULT std::optional<DesktopCapturer::SourceId> AcquireSourceId() {
    MutexLock lock(&mutex_);
    if (AtCapacityLocked()) {
      return std::nullopt;
    }
    if (handle_count_++ == 0) {
      auto* picker = GetPicker();
      picker.maximumStreamCount = [NSNumber numberWithUnsignedInt:maximumStreamCount];
      picker.active = YES;
    }
    return ++unique_source_id_;
  }

  void RelinquishSourceId(DesktopCapturer::SourceId source) {
    MutexLock lock(&mutex_);
    if (--handle_count_ > 0) {
      return;
    }
    GetPicker().active = NO;
  }

 private:
  bool AtCapacityLocked() const {
    mutex_.AssertHeld();
    return handle_count_ == maximumStreamCount;
  }

  mutable Mutex mutex_;
  // 100 is an arbitrary number that seems high enough to never get reached, while still providing
  // a reasonably low upper bound.
  static constexpr size_t maximumStreamCount = 100;
  size_t handle_count_ RTC_GUARDED_BY(mutex_) = 0;
  DesktopCapturer::SourceId unique_source_id_ RTC_GUARDED_BY(mutex_) = 0;
};

class API_AVAILABLE(macos(14.0)) SckPickerHandle : public SckPickerHandleInterface {
 public:
  static std::unique_ptr<SckPickerHandle> Create(SckPickerProxy* proxy) {
    std::optional<DesktopCapturer::SourceId> id = proxy->AcquireSourceId();
    if (!id) {
      return nullptr;
    }
    return std::unique_ptr<SckPickerHandle>(new SckPickerHandle(proxy, *id));
  }

  ~SckPickerHandle() { proxy_->RelinquishSourceId(source_); }

  SCContentSharingPicker* GetPicker() const override { return proxy_->GetPicker(); }

  DesktopCapturer::SourceId Source() const override { return source_; }

 private:
  SckPickerHandle(SckPickerProxy* proxy, DesktopCapturer::SourceId source)
      : proxy_(proxy), source_(source) {}

  SckPickerProxy* const proxy_;
  const DesktopCapturer::SourceId source_;
};

std::unique_ptr<SckPickerHandleInterface> CreateSckPickerHandle() {
  return SckPickerHandle::Create(SckPickerProxy::Get());
}

}  // namespace webrtc
