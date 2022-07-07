/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_

#include <memory>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class SharedScreenCastStreamPrivate;

class RTC_EXPORT SharedScreenCastStream
    : public rtc::RefCountedNonVirtual<SharedScreenCastStream> {
 public:
  static rtc::scoped_refptr<SharedScreenCastStream> CreateDefault();

  bool StartScreenCastStream(uint32_t stream_node_id, int fd);
  void StopScreenCastStream();
  std::unique_ptr<BasicDesktopFrame> CaptureFrame();

  ~SharedScreenCastStream();

 protected:
  SharedScreenCastStream();

 private:
  SharedScreenCastStream(const SharedScreenCastStream&) = delete;
  SharedScreenCastStream& operator=(const SharedScreenCastStream&) = delete;

  std::unique_ptr<SharedScreenCastStreamPrivate> private_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_
