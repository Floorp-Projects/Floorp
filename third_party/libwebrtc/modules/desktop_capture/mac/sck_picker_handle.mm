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

#include "api/sequence_checker.h"

namespace webrtc {

class API_AVAILABLE(macos(14.0)) SckPickerHandle : public SckPickerHandleInterface {
 public:
  explicit SckPickerHandle(DesktopCapturer::SourceId source) : source_(source) {
    RTC_DCHECK_RUN_ON(&checker_);
    RTC_CHECK_LE(sHandleCount, maximumStreamCount);
    if (sHandleCount++ == 0) {
      auto* picker = GetPicker();
      picker.maximumStreamCount = [NSNumber numberWithUnsignedInt:maximumStreamCount];
      picker.active = YES;
    }
  }

  ~SckPickerHandle() {
    RTC_DCHECK_RUN_ON(&checker_);
    if (--sHandleCount > 0) {
      return;
    }
    GetPicker().active = NO;
  }

  SCContentSharingPicker* GetPicker() const override {
    return SCContentSharingPicker.sharedPicker;
  }

  DesktopCapturer::SourceId Source() const override {
    return source_;
  }

  static bool AtCapacity() { return sHandleCount == maximumStreamCount; }

 private:
  // 100 is an arbitrary number that seems high enough to never get reached, while still providing
  // a reasonably low upper bound.
  static constexpr size_t maximumStreamCount = 100;
  static size_t sHandleCount;
  SequenceChecker checker_;
  const DesktopCapturer::SourceId source_;
};

size_t SckPickerHandle::sHandleCount = 0;

std::unique_ptr<SckPickerHandleInterface> CreateSckPickerHandle() API_AVAILABLE(macos(14.0)) {
  if (SckPickerHandle::AtCapacity()) {
    return nullptr;
  }
  static DesktopCapturer::SourceId unique_source_id = 0;
  return std::make_unique<SckPickerHandle>(++unique_source_id);
}

}  // namespace webrtc