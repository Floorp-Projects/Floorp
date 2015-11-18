/*
*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
#include "webrtc/modules/desktop_capture/window_capturer.h"
#include "webrtc/modules/desktop_capture/app_capturer.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/modules/desktop_capture/x11/shared_x_util.h"

#include <assert.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>
#include <X11/Xregion.h>

#include <algorithm>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"
#include "webrtc/modules/desktop_capture/x11/x_error_trap.h"
#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"

namespace webrtc {

namespace {

class WindowsCapturerProxy : DesktopCapturer::Callback {
public:
  WindowsCapturerProxy() : window_capturer_(WindowCapturer::Create()) {
    window_capturer_->Start(this);
  }
  ~WindowsCapturerProxy() {}
  void SelectWindow(WindowId windowId) { window_capturer_->SelectWindow(windowId); }
  rtc::scoped_ptr<DesktopFrame>& GetFrame() { return frame_; }
  void Capture(const DesktopRegion& region) { window_capturer_->Capture(region); }

  // Callback interface
  virtual SharedMemory *CreateSharedMemory(size_t) override { return NULL; }
  virtual void OnCaptureCompleted(DesktopFrame *frame) override { frame_.reset(frame); }

private:
  rtc::scoped_ptr<WindowCapturer> window_capturer_;
  rtc::scoped_ptr<DesktopFrame> frame_;
};

class ScreenCapturerProxy : DesktopCapturer::Callback {
public:
  ScreenCapturerProxy() : screen_capturer_(ScreenCapturer::Create()) {
    screen_capturer_->SelectScreen(kFullDesktopScreenId);
    screen_capturer_->Start(this);
  }
  void Capture(const DesktopRegion& region) { screen_capturer_->Capture(region); }
  rtc::scoped_ptr<DesktopFrame>& GetFrame() { return frame_; }

  // Callback interface
  virtual SharedMemory *CreateSharedMemory(size_t) override { return NULL; }
  virtual void OnCaptureCompleted(DesktopFrame *frame) override { frame_.reset(frame); }
protected:
  rtc::scoped_ptr<ScreenCapturer> screen_capturer_;
  rtc::scoped_ptr<DesktopFrame> frame_;
};

class AppCapturerLinux : public AppCapturer {
public:
  AppCapturerLinux(const DesktopCaptureOptions& options);
  virtual ~AppCapturerLinux();

  // AppCapturer interface.
  virtual bool GetAppList(AppList* apps) override;
  virtual bool SelectApp(ProcessId processId) override;
  virtual bool BringAppToFront() override;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) override;
  virtual void Capture(const DesktopRegion& region) override;

protected:
  Display* GetDisplay() { return x_display_->display(); }
  bool UpdateRegions();

  void CaptureWebRTC(const DesktopRegion& region);
  void CaptureSample(const DesktopRegion& region);

  void FillDesktopFrameRegionWithColor(DesktopFrame* pDesktopFrame,Region rgn, uint32_t color);
private:
  Callback* callback_;
  ProcessId selected_process_;

  // Sample Mode
  ScreenCapturerProxy screen_capturer_proxy_;
  // Mask of foreground (non-app windows in front of selected)
  Region rgn_mask_;
  // Region of selected windows
  Region rgn_visual_;
  // Mask of background (desktop, non-app windows behind selected)
  Region rgn_background_;

  // WebRtc Window mode
  WindowsCapturerProxy window_capturer_proxy_;

  scoped_refptr<SharedXDisplay> x_display_;
  DISALLOW_COPY_AND_ASSIGN(AppCapturerLinux);
};

AppCapturerLinux::AppCapturerLinux(const DesktopCaptureOptions& options)
    : callback_(NULL),
      selected_process_(0),
      x_display_(options.x_display()) {
  rgn_mask_ = XCreateRegion();
  rgn_visual_ = XCreateRegion();
  rgn_background_ = XCreateRegion();
}

AppCapturerLinux::~AppCapturerLinux() {
  if (rgn_mask_) {
    XDestroyRegion(rgn_mask_);
  }
  if (rgn_visual_) {
    XDestroyRegion(rgn_visual_);
  }
  if (rgn_background_) {
    XDestroyRegion(rgn_background_);
  }
}

// AppCapturer interface.
bool AppCapturerLinux::GetAppList(AppList* apps) {
  // Implemented in DesktopDeviceInfo
  return true;
}
bool AppCapturerLinux::SelectApp(ProcessId processId) {
  selected_process_ = processId;
  return true;
}
bool AppCapturerLinux::BringAppToFront() {
  // Not implemented yet: See Bug 1036653
  return true;
}

// DesktopCapturer interface.
void AppCapturerLinux::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void AppCapturerLinux::Capture(const DesktopRegion& region) {
  CaptureSample(region);
}

void AppCapturerLinux::CaptureWebRTC(const DesktopRegion& region) {
  XErrorTrap error_trap(GetDisplay());

  int nScreenWidth = DisplayWidth(GetDisplay(), DefaultScreen(GetDisplay()));
  int nScreenHeight = DisplayHeight(GetDisplay(), DefaultScreen(GetDisplay()));
  rtc::scoped_ptr<DesktopFrame> frame(new BasicDesktopFrame(DesktopSize(nScreenWidth, nScreenHeight)));

  WindowUtilX11 window_util_x11(x_display_);

  ::Window root_window = XRootWindow(GetDisplay(), DefaultScreen(GetDisplay()));
  ::Window parent;
  ::Window root_return;
  ::Window *children;
  unsigned int num_children;
  int status = XQueryTree(GetDisplay(), root_window, &root_return, &parent,
      &children, &num_children);
  if (status == 0) {
    LOG(LS_ERROR) << "Failed to query for child windows for screen "
        << DefaultScreen(GetDisplay());
    return;
  }

  for (unsigned int i = 0; i < num_children; ++i) {
    ::Window app_window = window_util_x11.GetApplicationWindow(children[i]);
    if (!app_window) {
      continue;
    }

    unsigned int processId = window_util_x11.GetWindowProcessID(app_window);
    if(processId != 0 && processId == selected_process_) {
      // capture
      window_capturer_proxy_.SelectWindow(app_window);
      window_capturer_proxy_.Capture(region);
      DesktopFrame* frameWin = window_capturer_proxy_.GetFrame().get();
      if (frameWin == NULL) {
        continue;
      }

      XRectangle  win_rect;
      window_util_x11.GetWindowRect(app_window,win_rect, false);
      if (win_rect.width <= 0 || win_rect.height <= 0) {
        continue;
      }

      DesktopSize winFrameSize = frameWin->size();
      DesktopRect target_rect = DesktopRect::MakeXYWH(win_rect.x,
                                                      win_rect.y,
                                                      winFrameSize.width(),
                                                      winFrameSize.height());

      // bitblt into background frame
      frame->CopyPixelsFrom(*frameWin, DesktopVector(0, 0), target_rect);
    }
  }

  if (children) {
    XFree(children);
  }

  // trigger event
  if (callback_) {
    callback_->OnCaptureCompleted(error_trap.GetLastErrorAndDisable() != 0 ?
                                  NULL :
                                  frame.release());
  }
}

void AppCapturerLinux::CaptureSample(const DesktopRegion& region) {
  XErrorTrap error_trap(GetDisplay());

  //Capture screen >> set root window as capture window
  screen_capturer_proxy_.Capture(region);
  DesktopFrame* frame = screen_capturer_proxy_.GetFrame().get();
  if (frame) {
    // calculate app visual/foreground region
    UpdateRegions();

    // TODO: background/foreground mask colors should be configurable; see Bug 1054503
    // fill background with black
    FillDesktopFrameRegionWithColor(frame, rgn_background_, 0xFF000000);

    // fill foreground with yellow
    FillDesktopFrameRegionWithColor(frame, rgn_mask_, 0xFFFFFF00);
 }

  // trigger event
  if (callback_) {
    callback_->OnCaptureCompleted(error_trap.GetLastErrorAndDisable() != 0 ?
                                  NULL :
                                  screen_capturer_proxy_.GetFrame().release());
  }
}

void AppCapturerLinux::FillDesktopFrameRegionWithColor(DesktopFrame* pDesktopFrame, Region rgn, uint32_t color) {
  XErrorTrap error_trap(GetDisplay());

  if (!pDesktopFrame) {
    return;
  }
  if (XEmptyRegion(rgn)) {
    return;
  }

  REGION * st_rgn = (REGION *)rgn;
  if(st_rgn && st_rgn->numRects > 0) {
    for (short i = 0; i < st_rgn->numRects; i++) {
      for (short j = st_rgn->rects[i].y1; j < st_rgn->rects[i].y2; j++) {
        uint32_t* dst_pos = reinterpret_cast<uint32_t*>(pDesktopFrame->data() + pDesktopFrame->stride() * j);
        for (short k = st_rgn->rects[i].x1; k < st_rgn->rects[i].x2; k++) {
          dst_pos[k] = color;
        }
      }
    }
  }
}

bool AppCapturerLinux::UpdateRegions() {
  XErrorTrap error_trap(GetDisplay());

  XSubtractRegion(rgn_visual_, rgn_visual_, rgn_visual_);
  XSubtractRegion(rgn_mask_, rgn_mask_, rgn_mask_);
  WindowUtilX11 window_util_x11(x_display_);
  int num_screens = XScreenCount(GetDisplay());
  for (int screen = 0; screen < num_screens; ++screen) {
    int nScreenCX = DisplayWidth(GetDisplay(), screen);
    int nScreenCY = DisplayHeight(GetDisplay(), screen);

    XRectangle  screen_rect;
    screen_rect.x = 0;
    screen_rect.y = 0;
    screen_rect.width = nScreenCX;
    screen_rect.height = nScreenCY;

    XUnionRectWithRegion(&screen_rect, rgn_background_, rgn_background_);
    XXorRegion(rgn_mask_, rgn_mask_, rgn_mask_);
    XXorRegion(rgn_visual_, rgn_visual_, rgn_visual_);

    ::Window root_window = XRootWindow(GetDisplay(), screen);
    ::Window parent;
    ::Window root_return;
    ::Window *children;
    unsigned int num_children;
    int status = XQueryTree(GetDisplay(), root_window, &root_return, &parent, &children, &num_children);
    if (status == 0) {
      LOG(LS_ERROR) << "Failed to query for child windows for screen " << screen;
      continue;
    }
    for (unsigned int i = 0; i < num_children; ++i) {
      ::Window app_window = window_util_x11.GetApplicationWindow(children[i]);
      if (!app_window) {
        continue;
      }

      // Get window region
      XRectangle  win_rect;
      window_util_x11.GetWindowRect(app_window, win_rect, true);
      if (win_rect.width <= 0 || win_rect.height <= 0) {
        continue;
      }

      Region win_rgn = XCreateRegion();
      XUnionRectWithRegion(&win_rect, win_rgn, win_rgn);
      // update rgn_visual_ , rgn_mask_,
      unsigned int processId = window_util_x11.GetWindowProcessID(app_window);
      if (processId != 0 && processId == selected_process_) {
        XUnionRegion(rgn_visual_, win_rgn, rgn_visual_);
        XSubtractRegion(rgn_mask_, win_rgn, rgn_mask_);
      } else {
        Region win_rgn_intersect = XCreateRegion();
        XIntersectRegion(rgn_visual_, win_rgn, win_rgn_intersect);

        XSubtractRegion(rgn_visual_, win_rgn_intersect, rgn_visual_);
        XUnionRegion(win_rgn_intersect, rgn_mask_, rgn_mask_);

        if (win_rgn_intersect) {
          XDestroyRegion(win_rgn_intersect);
        }
      }
      if (win_rgn) {
        XDestroyRegion(win_rgn);
      }
    }

    if (children) {
      XFree(children);
    }
  }

  XSubtractRegion(rgn_background_, rgn_visual_, rgn_background_);

  return true;
}

}  // namespace

// static
AppCapturer* AppCapturer::Create(const DesktopCaptureOptions& options) {
  return new AppCapturerLinux(options);
}

}  // namespace webrtc
