/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <dwmapi.h>
#include <windows.h>

#include "MockWinWidget.h"
#include "mozilla/widget/WinWindowOcclusionTracker.h"
#include "mozilla/WindowsVersion.h"

using namespace mozilla;
using namespace mozilla::widget;

class WinWindowOcclusionTrackerTest : public ::testing::Test {
 protected:
  HWND CreateNativeWindow(DWORD aStyle, DWORD aExStyle) {
    mMockWinWidget =
        MockWinWidget::Create(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | aStyle,
                              aExStyle, LayoutDeviceIntRect(0, 0, 100, 100));
    EXPECT_NE(nullptr, mMockWinWidget.get());
    HWND hwnd = mMockWinWidget->GetWnd();
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
    return hwnd;
  }

  // Wrapper around IsWindowVisibleAndFullyOpaque so only the test class
  // needs to be a friend of NativeWindowOcclusionTrackerWin.
  bool CheckWindowVisibleAndFullyOpaque(HWND aHWnd,
                                        LayoutDeviceIntRect* aWinRect) {
    bool ret = WinWindowOcclusionTracker::IsWindowVisibleAndFullyOpaque(
        aHWnd, aWinRect);
    // In general, if IsWindowVisibleAndFullyOpaque returns false, the
    // returned rect should not be altered.
    if (!ret) {
      EXPECT_EQ(*aWinRect, LayoutDeviceIntRect(0, 0, 0, 0));
    }
    return ret;
  }

  RefPtr<MockWinWidget> mMockWinWidget;
};

TEST_F(WinWindowOcclusionTrackerTest, VisibleOpaqueWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  LayoutDeviceIntRect returnedRect;
  // Normal windows should be visible.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &returnedRect));

  // Check that the returned rect == the actual window rect of the hwnd.
  RECT winRect;
  ASSERT_TRUE(::GetWindowRect(hwnd, &winRect));
  EXPECT_EQ(returnedRect, LayoutDeviceIntRect(winRect.left, winRect.top,
                                              winRect.right - winRect.left,
                                              winRect.bottom - winRect.top));
}

TEST_F(WinWindowOcclusionTrackerTest, MinimizedWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  LayoutDeviceIntRect winRect;
  ::ShowWindow(hwnd, SW_MINIMIZE);
  // Minimized windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, TransparentWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, WS_EX_TRANSPARENT);
  LayoutDeviceIntRect winRect;
  // Transparent windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, ToolWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, WS_EX_TOOLWINDOW);
  LayoutDeviceIntRect winRect;
  // Tool windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, LayeredAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, WS_EX_LAYERED);
  LayoutDeviceIntRect winRect;
  BYTE alpha = 1;
  DWORD flags = LWA_ALPHA;
  COLORREF colorRef = RGB(1, 1, 1);
  SetLayeredWindowAttributes(hwnd, colorRef, alpha, flags);
  // Layered windows with alpha < 255 are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, UpdatedLayeredAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, WS_EX_LAYERED);
  LayoutDeviceIntRect winRect;
  HDC hdc = ::CreateCompatibleDC(nullptr);
  EXPECT_NE(nullptr, hdc);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0x00, 0xFF, AC_SRC_ALPHA};

  ::UpdateLayeredWindow(hwnd, hdc, nullptr, nullptr, nullptr, nullptr,
                        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_OPAQUE);
  // Layered windows set up with UpdateLayeredWindow instead of
  // SetLayeredWindowAttributes should not be considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
  ::DeleteDC(hdc);
}

TEST_F(WinWindowOcclusionTrackerTest, LayeredNonAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, WS_EX_LAYERED);
  LayoutDeviceIntRect winRect;
  BYTE alpha = 1;
  DWORD flags = 0;
  COLORREF colorRef = RGB(1, 1, 1);
  ::SetLayeredWindowAttributes(hwnd, colorRef, alpha, flags);
  // Layered non alpha windows are considered visible and opaque.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, ComplexRegionWindow) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  LayoutDeviceIntRect winRect;
  // Create a region with rounded corners, which should be a complex region.
  HRGN region = CreateRoundRectRgn(1, 1, 100, 100, 5, 5);
  EXPECT_NE(nullptr, region);
  ::SetWindowRgn(hwnd, region, /* bRedraw = */ TRUE);
  // Windows with complex regions are not considered visible and fully opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
  DeleteObject(region);
}

TEST_F(WinWindowOcclusionTrackerTest, PopupWindow) {
  HWND hwnd = CreateNativeWindow(WS_POPUP, /* aExStyle = */ 0);
  LayoutDeviceIntRect winRect;
  // Normal Popup Windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}

TEST_F(WinWindowOcclusionTrackerTest, CloakedWindow) {
  // Cloaking is only supported in Windows 8 and above.
  if (!IsWin8OrLater()) {
    return;
  }
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  LayoutDeviceIntRect winRect;
  BOOL cloak = TRUE;
  ::DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
  // Cloaked Windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &winRect));
}
