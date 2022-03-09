/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "MockWinWidget.h"
#include "mozilla/Preferences.h"
#include "mozilla/widget/WinEventObserver.h"
#include "mozilla/widget/WinWindowOcclusionTracker.h"
#include "nsThreadUtils.h"
#include "Units.h"
#include "WinUtils.h"

using namespace mozilla;
using namespace mozilla::widget;

#define PREF_DISPLAY_STATE \
  "widget.windows.window_occlusion_tracking_display_state.enabled"
#define PREF_SESSION_LOCK \
  "widget.windows.window_occlusion_tracking_session_lock.enabled"

class WinWindowOcclusionTrackerInteractiveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Shut down WinWindowOcclusionTracker if it exists.
    // This could happen when WinWindowOcclusionTracker was initialized by other
    // gtest
    if (WinWindowOcclusionTracker::Get()) {
      WinWindowOcclusionTracker::ShutDown();
    }
    EXPECT_EQ(nullptr, WinWindowOcclusionTracker::Get());

    WinWindowOcclusionTracker::Ensure();
    EXPECT_NE(nullptr, WinWindowOcclusionTracker::Get());

    WinWindowOcclusionTracker::Get()->EnsureDisplayStatusObserver();
    WinWindowOcclusionTracker::Get()->EnsureSessionChangeObserver();
  }

  void TearDown() override {
    WinWindowOcclusionTracker::ShutDown();
    EXPECT_EQ(nullptr, WinWindowOcclusionTracker::Get());
  }

  void SetNativeWindowBounds(HWND aHWnd, const LayoutDeviceIntRect aBounds) {
    RECT wr;
    wr.left = aBounds.X();
    wr.top = aBounds.Y();
    wr.right = aBounds.XMost();
    wr.bottom = aBounds.YMost();

    ::AdjustWindowRectEx(&wr, ::GetWindowLong(aHWnd, GWL_STYLE), FALSE,
                         ::GetWindowLong(aHWnd, GWL_EXSTYLE));

    // Make sure to keep the window onscreen, as AdjustWindowRectEx() may have
    // moved part of it offscreen. But, if the original requested bounds are
    // offscreen, don't adjust the position.
    LayoutDeviceIntRect windowBounds(wr.left, wr.top, wr.right - wr.left,
                                     wr.bottom - wr.top);

    if (aBounds.X() >= 0) {
      windowBounds.x = std::max(0, windowBounds.X());
    }
    if (aBounds.Y() >= 0) {
      windowBounds.y = std::max(0, windowBounds.Y());
    }
    ::SetWindowPos(aHWnd, HWND_TOP, windowBounds.X(), windowBounds.Y(),
                   windowBounds.Width(), windowBounds.Height(),
                   SWP_NOREPOSITION);
    EXPECT_TRUE(::UpdateWindow(aHWnd));
  }

  void CreateNativeWindowWithBounds(LayoutDeviceIntRect aBounds) {
    mMockWinWidget = MockWinWidget::Create(
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, /* aExStyle = */ 0, aBounds);
    EXPECT_NE(nullptr, mMockWinWidget.get());
    HWND hwnd = mMockWinWidget->GetWnd();
    SetNativeWindowBounds(hwnd, aBounds);
    HRGN region = ::CreateRectRgn(0, 0, 0, 0);
    EXPECT_NE(nullptr, region);
    if (::GetWindowRgn(hwnd, region) == COMPLEXREGION) {
      // On Windows 7, the newly created window has a complex region, which
      // means it will be ignored during the occlusion calculation. So, force
      // it to have a simple region so that we get test coverage on win 7.
      RECT boundingRect;
      EXPECT_TRUE(::GetWindowRect(hwnd, &boundingRect));
      HRGN rectangularRegion = ::CreateRectRgnIndirect(&boundingRect);
      EXPECT_NE(nullptr, rectangularRegion);
      ::SetWindowRgn(hwnd, rectangularRegion, /* bRedraw = */ TRUE);
      ::DeleteObject(rectangularRegion);
    }
    ::DeleteObject(region);

    ::ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
  }

  RefPtr<MockWinWidget> CreateTrackedWindowWithBounds(
      LayoutDeviceIntRect aBounds) {
    RefPtr<MockWinWidget> window = MockWinWidget::Create(
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, /* aExStyle */ 0, aBounds);
    EXPECT_NE(nullptr, window.get());
    HWND hwnd = window->GetWnd();
    ::ShowWindow(hwnd, SW_SHOWNORMAL);
    WinWindowOcclusionTracker::Get()->Enable(window, window->GetWnd());
    return window;
  }

  int GetNumVisibleRootWindows() {
    return WinWindowOcclusionTracker::Get()->mNumVisibleRootWindows;
  }

  void OnDisplayStateChanged(bool aDisplayOn) {
    WinWindowOcclusionTracker::Get()->OnDisplayStateChanged(aDisplayOn);
  }

  RefPtr<MockWinWidget> mMockWinWidget;
};

// Simple test completely covering a tracked window with a native window.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, SimpleOcclusion) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  window->SetExpectation(widget::OcclusionState::OCCLUDED);
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Simple test partially covering a tracked window with a native window.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, PartialOcclusion) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 200, 200));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(0, 0, 50, 50));
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Simple test that a partly off screen tracked window, with the on screen part
// occluded by a native window, is considered occluded.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, OffscreenOcclusion) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  // Move the tracked window 50 pixels offscreen to the left.
  int screenLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
  ::SetWindowPos(window->GetWnd(), HWND_TOP, screenLeft - 50, 0, 100, 100,
                 SWP_NOZORDER | SWP_NOSIZE);

  // Create a native window that covers the onscreen part of the tracked window.
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(screenLeft, 0, 50, 100));
  window->SetExpectation(widget::OcclusionState::OCCLUDED);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Simple test with a tracked window and native window that do not overlap.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, SimpleVisible) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(200, 0, 100, 100));
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Simple test with a minimized tracked window and native window.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, SimpleHidden) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(200, 0, 100, 100));
  // Iconify the tracked window and check that its occlusion state is HIDDEN.
  ::CloseWindow(window->GetWnd());
  window->SetExpectation(widget::OcclusionState::HIDDEN);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Test that minimizing and restoring a tracked window results in the occlusion
// tracker re-registering for win events and detecting that a native window
// occludes the tracked window.
TEST_F(WinWindowOcclusionTrackerInteractiveTest,
       OcclusionAfterVisibilityToggle) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  window->SetExpectation(widget::OcclusionState::HIDDEN);
  WinWindowOcclusionTracker::Get()->OnWindowVisibilityChanged(
      window, /* aVisible = */ false);

  // This makes the window iconic.
  ::CloseWindow(window->GetWnd());

  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  // HIDDEN state is set synchronously by OnWindowVsiblityChanged notification,
  // before occlusion is calculated, so the above expectation will be met w/o an
  // occlusion calculation.
  // Loop until an occlusion calculation has run with no non-hidden windows.
  while (GetNumVisibleRootWindows() != 0) {
    // Need to pump events in order for UpdateOcclusionState to get called, and
    // update the number of non hidden windows. When that number is 0,
    // occlusion has been calculated with no visible tracked windows.
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  window->SetExpectation(widget::OcclusionState::VISIBLE);
  WinWindowOcclusionTracker::Get()->OnWindowVisibilityChanged(
      window, /* aVisible = */ true);
  // This opens the window made iconic above.
  ::OpenIcon(window->GetWnd());

  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  // Open a native window that occludes the visible tracked window.
  window->SetExpectation(widget::OcclusionState::OCCLUDED);
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Test that locking the screen causes visible windows to become occluded.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, LockScreenVisibleOcclusion) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  window->SetExpectation(widget::OcclusionState::OCCLUDED);
  // Unfortunately, this relies on knowing that NativeWindowOcclusionTracker
  // uses SessionChangeObserver to listen for WM_WTSSESSION_CHANGE messages, but
  // actually locking the screen isn't feasible.
  DWORD currentSessionId = 0;
  ::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId);
  ::PostMessage(WinEventHub::Get()->GetWnd(), WM_WTSSESSION_CHANGE,
                WTS_SESSION_LOCK, currentSessionId);

  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    MSG msg;
    bool gotMessage =
        ::PeekMessageW(&msg, WinEventHub::Get()->GetWnd(), 0, 0, PM_REMOVE);
    if (gotMessage) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ false);
    PR_Sleep(PR_INTERVAL_NO_WAIT);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Test that locking the screen leaves hidden windows as hidden.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, LockScreenHiddenOcclusion) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  // Iconify the tracked window and check that its occlusion state is HIDDEN.
  ::CloseWindow(window->GetWnd());
  window->SetExpectation(widget::OcclusionState::HIDDEN);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  // Force the state to VISIBLE.
  window->NotifyOcclusionState(widget::OcclusionState::VISIBLE);

  window->SetExpectation(widget::OcclusionState::HIDDEN);

  // Unfortunately, this relies on knowing that NativeWindowOcclusionTracker
  // uses SessionChangeObserver to listen for WM_WTSSESSION_CHANGE messages, but
  // actually locking the screen isn't feasible.
  DWORD currentSessionId = 0;
  ::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId);
  PostMessage(WinEventHub::Get()->GetWnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, currentSessionId);

  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    MSG msg;
    bool gotMessage =
        ::PeekMessageW(&msg, WinEventHub::Get()->GetWnd(), 0, 0, PM_REMOVE);
    if (gotMessage) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ false);
    PR_Sleep(PR_INTERVAL_NO_WAIT);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Test that locking the screen from a different session doesn't mark window
// as occluded.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, LockScreenDifferentSession) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 200, 200));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  // Force the state to OCCLUDED.
  window->NotifyOcclusionState(widget::OcclusionState::OCCLUDED);

  // Generate a session change lock screen with a session id that's not
  // currentSessionId.
  DWORD currentSessionId = 0;
  ::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId);
  ::PostMessage(WinEventHub::Get()->GetWnd(), WM_WTSSESSION_CHANGE,
                WTS_SESSION_LOCK, currentSessionId + 1);

  window->SetExpectation(widget::OcclusionState::VISIBLE);
  // Create a native window to trigger occlusion calculation.
  CreateNativeWindowWithBounds(LayoutDeviceIntRect(0, 0, 50, 50));
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    MSG msg;
    bool gotMessage =
        ::PeekMessageW(&msg, WinEventHub::Get()->GetWnd(), 0, 0, PM_REMOVE);
    if (gotMessage) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
    NS_ProcessNextEvent(nullptr, /* aMayWait = */ false);
    PR_Sleep(PR_INTERVAL_NO_WAIT);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}

// Test that display off & on power state notification causes visible windows to
// become occluded, then visible.
TEST_F(WinWindowOcclusionTrackerInteractiveTest, DisplayOnOffHandling) {
  RefPtr<MockWinWidget> window =
      CreateTrackedWindowWithBounds(LayoutDeviceIntRect(0, 0, 100, 100));
  window->SetExpectation(widget::OcclusionState::VISIBLE);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  window->SetExpectation(widget::OcclusionState::OCCLUDED);

  // Turning display off and on isn't feasible, so send a notification.
  OnDisplayStateChanged(/* aDisplayOn */ false);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }

  window->SetExpectation(widget::OcclusionState::VISIBLE);
  OnDisplayStateChanged(/* aDisplayOn */ true);
  while (window->IsExpectingCall()) {
    WinWindowOcclusionTracker::Get()->TriggerCalculation();

    NS_ProcessNextEvent(nullptr, /* aMayWait = */ true);
  }
  EXPECT_FALSE(window->IsExpectingCall());
}
