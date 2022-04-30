/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capturer_win.h"

#include <string>
#include <utility>
#include <vector>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/test_support/test_window.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/win/scoped_com_initializer.h"
#include "rtc_base/win/windows_version.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const char kWindowThreadName[] = "wgc_capturer_test_window_thread";
const WCHAR kWindowTitle[] = L"WGC Capturer Test Window";

const int kSmallWindowWidth = 200;
const int kSmallWindowHeight = 100;
const int kWindowWidth = 300;
const int kWindowHeight = 200;
const int kLargeWindowWidth = 400;
const int kLargeWindowHeight = 300;

// The size of the image we capture is slightly smaller than the actual size of
// the window.
const int kWindowWidthSubtrahend = 14;
const int kWindowHeightSubtrahend = 7;

// Custom message constants so we can direct our thread to close windows
// and quit running.
const UINT kNoOp = WM_APP;
const UINT kDestroyWindow = WM_APP + 1;
const UINT kQuitRunning = WM_APP + 2;

enum CaptureType { kWindowCapture = 0, kScreenCapture = 1 };

}  // namespace

class WgcCapturerWinTest : public ::testing::TestWithParam<CaptureType>,
                           public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
      RTC_LOG(LS_INFO)
          << "Skipping WgcWindowCaptureTests on Windows versions < RS5.";
      GTEST_SKIP();
    }

    com_initializer_ =
        std::make_unique<ScopedCOMInitializer>(ScopedCOMInitializer::kMTA);
    EXPECT_TRUE(com_initializer_->Succeeded());
  }

  void SetUpForWindowCapture() {
    capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    CreateWindowOnSeparateThread();
    StartWindowThreadMessageLoop();
    source_id_ = GetTestWindowIdFromSourceList();
  }

  void SetUpForScreenCapture() {
    capturer_ = WgcCapturerWin::CreateRawScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = GetScreenIdFromSourceList();
  }

  void TearDown() override {
    if (window_open_) {
      CloseTestWindow();
    }
  }

  // The window must live on a separate thread so that we can run a message pump
  // without blocking the test thread. This is necessary if we are interested in
  // having GraphicsCaptureItem events (i.e. the Closed event) fire, and it more
  // closely resembles how capture works in the wild.
  void CreateWindowOnSeparateThread() {
    window_thread_ = rtc::Thread::Create();
    window_thread_->SetName(kWindowThreadName, nullptr);
    window_thread_->Start();
    window_thread_->Invoke<void>(RTC_FROM_HERE, [this]() {
      window_thread_id_ = GetCurrentThreadId();
      window_info_ =
          CreateTestWindow(kWindowTitle, kWindowHeight, kWindowWidth);
      window_open_ = true;

      while (!IsWindowResponding(window_info_.hwnd)) {
        RTC_LOG(LS_INFO) << "Waiting for test window to become responsive in "
                            "WgcWindowCaptureTest.";
      }

      while (!IsWindowValidAndVisible(window_info_.hwnd)) {
        RTC_LOG(LS_INFO) << "Waiting for test window to be visible in "
                            "WgcWindowCaptureTest.";
      }
    });

    ASSERT_TRUE(window_thread_->RunningForTest());
    ASSERT_FALSE(window_thread_->IsCurrent());
  }

  void StartWindowThreadMessageLoop() {
    window_thread_->PostTask(RTC_FROM_HERE, [this]() {
      MSG msg;
      BOOL gm;
      while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
        ::DispatchMessage(&msg);
        if (msg.message == kDestroyWindow) {
          DestroyTestWindow(window_info_);
        }
        if (msg.message == kQuitRunning) {
          PostQuitMessage(0);
        }
      }
    });
  }

  void CloseTestWindow() {
    ::PostThreadMessage(window_thread_id_, kDestroyWindow, 0, 0);
    ::PostThreadMessage(window_thread_id_, kQuitRunning, 0, 0);
    window_thread_->Stop();
    window_open_ = false;
  }

  DesktopCapturer::SourceId GetTestWindowIdFromSourceList() {
    // Frequently, the test window will not show up in GetSourceList because it
    // was created too recently. Since we are confident the window will be found
    // eventually we loop here until we find it.
    intptr_t src_id;
    do {
      DesktopCapturer::SourceList sources;
      EXPECT_TRUE(capturer_->GetSourceList(&sources));

      auto it = std::find_if(
          sources.begin(), sources.end(),
          [&](const DesktopCapturer::Source& src) {
            return src.id == reinterpret_cast<intptr_t>(window_info_.hwnd);
          });

      src_id = it->id;
    } while (src_id != reinterpret_cast<intptr_t>(window_info_.hwnd));

    return src_id;
  }

  DesktopCapturer::SourceId GetScreenIdFromSourceList() {
    DesktopCapturer::SourceList sources;
    EXPECT_TRUE(capturer_->GetSourceList(&sources));
    EXPECT_GT(sources.size(), 0ULL);
    return sources[0].id;
  }

  void DoCapture() {
    // Sometimes the first few frames are empty becaues the capture engine is
    // still starting up. We also may drop a few frames when the window is
    // resized or un-minimized.
    do {
      capturer_->CaptureFrame();
    } while (result_ == DesktopCapturer::Result::ERROR_TEMPORARY);

    EXPECT_EQ(result_, DesktopCapturer::Result::SUCCESS);
    EXPECT_TRUE(frame_);
  }

  // DesktopCapturer::Callback interface
  // The capturer synchronously invokes this method before |CaptureFrame()|
  // returns.
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    result_ = result;
    frame_ = std::move(frame);
  }

 protected:
  std::unique_ptr<ScopedCOMInitializer> com_initializer_;
  DWORD window_thread_id_;
  std::unique_ptr<rtc::Thread> window_thread_;
  WindowInfo window_info_;
  intptr_t source_id_;
  bool window_open_ = false;
  DesktopCapturer::Result result_;
  std::unique_ptr<DesktopFrame> frame_;
  std::unique_ptr<DesktopCapturer> capturer_;
};

TEST_P(WgcCapturerWinTest, SelectValidSource) {
  if (GetParam() == CaptureType::kWindowCapture) {
    SetUpForWindowCapture();
  } else {
    SetUpForScreenCapture();
  }

  EXPECT_TRUE(capturer_->SelectSource(source_id_));
}

TEST_P(WgcCapturerWinTest, SelectInvalidSource) {
  if (GetParam() == CaptureType::kWindowCapture) {
    capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = kNullWindowId;
  } else {
    capturer_ = WgcCapturerWin::CreateRawScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = kInvalidScreenId;
  }

  EXPECT_FALSE(capturer_->SelectSource(source_id_));
}

TEST_P(WgcCapturerWinTest, Capture) {
  if (GetParam() == CaptureType::kWindowCapture) {
    SetUpForWindowCapture();
  } else {
    SetUpForScreenCapture();
  }

  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  EXPECT_GT(frame_->size().width(), 0);
  EXPECT_GT(frame_->size().height(), 0);
}

INSTANTIATE_TEST_SUITE_P(SourceAgnostic,
                         WgcCapturerWinTest,
                         ::testing::Values(CaptureType::kWindowCapture,
                                           CaptureType::kScreenCapture));

// Monitor specific tests.
TEST_F(WgcCapturerWinTest, CaptureAllMonitors) {
  SetUpForScreenCapture();
  // 0 (or a NULL HMONITOR) leads to WGC capturing all displays.
  EXPECT_TRUE(capturer_->SelectSource(0));

  capturer_->Start(this);
  DoCapture();
  EXPECT_GT(frame_->size().width(), 0);
  EXPECT_GT(frame_->size().height(), 0);
}

// Window specific tests.
TEST_F(WgcCapturerWinTest, SelectMinimizedWindow) {
  SetUpForWindowCapture();
  MinimizeTestWindow(reinterpret_cast<HWND>(source_id_));
  EXPECT_FALSE(capturer_->SelectSource(source_id_));

  UnminimizeTestWindow(reinterpret_cast<HWND>(source_id_));
  EXPECT_TRUE(capturer_->SelectSource(source_id_));
}

TEST_F(WgcCapturerWinTest, SelectClosedWindow) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  CloseTestWindow();
  EXPECT_FALSE(capturer_->SelectSource(source_id_));
}

TEST_F(WgcCapturerWinTest, ResizeWindowMidCapture) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(window_info_.hwnd, kLargeWindowWidth, kLargeWindowHeight);
  DoCapture();
  // We don't expect to see the new size until the next capture, as the frame
  // pool hadn't had a chance to resize yet.
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kLargeWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kLargeWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(window_info_.hwnd, kSmallWindowWidth, kSmallWindowHeight);
  DoCapture();
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kSmallWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kSmallWindowHeight - kWindowHeightSubtrahend);
}

TEST_F(WgcCapturerWinTest, MinimizeWindowMidCapture) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);

  // Minmize the window and capture should continue but return temporary errors.
  MinimizeTestWindow(window_info_.hwnd);
  for (int i = 0; i < 10; ++i) {
    capturer_->CaptureFrame();
    EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_TEMPORARY);
  }

  // Reopen the window and the capture should continue normally.
  UnminimizeTestWindow(window_info_.hwnd);
  DoCapture();
  // We can't verify the window size here because the test window does not
  // repaint itself after it is unminimized, but capturing successfully is still
  // a good test.
}

TEST_F(WgcCapturerWinTest, CloseWindowMidCapture) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  CloseTestWindow();

  // We need to call GetMessage to trigger the Closed event and the capturer's
  // event handler for it. If we are too early and the Closed event hasn't
  // arrived yet we should keep trying until the capturer receives it and stops.
  auto* wgc_capturer = static_cast<WgcCapturerWin*>(capturer_.get());
  while (wgc_capturer->IsSourceBeingCaptured(source_id_)) {
    // Since the capturer handles the Closed message, there will be no message
    // for us and GetMessage will hang, unless we send ourselves a message
    // first.
    ::PostThreadMessage(GetCurrentThreadId(), kNoOp, 0, 0);
    MSG msg;
    ::GetMessage(&msg, NULL, 0, 0);
    ::DispatchMessage(&msg);
  }

  // Occasionally, one last frame will have made it into the frame pool before
  // the window closed. The first call will consume it, and in that case we need
  // to make one more call to CaptureFrame.
  capturer_->CaptureFrame();
  if (result_ == DesktopCapturer::Result::SUCCESS)
    capturer_->CaptureFrame();

  EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_PERMANENT);
}

}  // namespace webrtc
