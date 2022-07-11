/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

BaseCapturerPipeWire::BaseCapturerPipeWire(const DesktopCaptureOptions& options)
    : options_(options) {
  screencast_portal_ = std::make_unique<ScreenCastPortal>(
      ScreenCastPortal::CaptureSourceType::kAnyScreenContent, this);
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {}

void BaseCapturerPipeWire::OnScreenCastRequestResult(
    ScreenCastPortal::RequestResponse result,
    uint32_t stream_node_id,
    int fd) {
  if (result != ScreenCastPortal::RequestResponse::kSuccess ||
      !options_.screencast_stream()->StartScreenCastStream(stream_node_id,
                                                           fd)) {
    capturer_failed_ = true;
    RTC_LOG(LS_ERROR) << "ScreenCastPortal failed: "
                      << static_cast<uint>(result);
  }
}

void BaseCapturerPipeWire::OnScreenCastSessionClosed() {
  if (!capturer_failed_) {
    options_.screencast_stream()->StopScreenCastStream();
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  screencast_portal_->Start();
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  std::unique_ptr<DesktopFrame> frame =
      options_.screencast_stream()->CaptureFrame();

  if (!frame || !frame->data()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on
  // the frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

// Keep in sync with defines at browser/actors/WebRTCParent.jsm
// With PipeWire we can't select which system resource is shared so
// we don't create a window/screen list. Instead we place these constants
// as window name/id so frontend code can identify PipeWire backend
// and does not try to create screen/window preview.

#define PIPEWIRE_ID   0xaffffff
#define PIPEWIRE_NAME "####_PIPEWIRE_PORTAL_####"

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  sources->push_back({PIPEWIRE_ID, 0, PIPEWIRE_NAME});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return id == PIPEWIRE_ID;
}

}  // namespace webrtc
