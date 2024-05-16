/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <windows.h>

#include "MockWinWidget.h"
#include "mozilla/widget/OSKInputPaneManager.h"

using namespace mozilla;
using namespace mozilla::widget;

class WinInputPaneTest : public ::testing::Test {
 protected:
  HWND CreateNativeWindow(DWORD aStyle, DWORD aExStyle) {
    mMockWinWidget =
        MockWinWidget::Create(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | aStyle,
                              aExStyle, LayoutDeviceIntRect(0, 0, 100, 100));
    EXPECT_NE(nullptr, mMockWinWidget.get());
    HWND hwnd = mMockWinWidget->GetWnd();

    ::ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
    return hwnd;
  }

  void ExpectHasEventHandler(HWND aHwnd, bool aValue) {
    Maybe<bool> hasEventHandler =
        OSKInputPaneManager::HasInputPaneEventHandlerService(aHwnd);
    EXPECT_TRUE(hasEventHandler.isSome() && hasEventHandler.value() == aValue);
  }

  RefPtr<MockWinWidget> mMockWinWidget;
};

TEST_F(WinInputPaneTest, HasNoEventHandlerAtCreation) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  ExpectHasEventHandler(hwnd, false);
}

TEST_F(WinInputPaneTest, HasEventHandlerAfterShowOnScreenKeyboard) {
  HWND hwnd = CreateNativeWindow(/* aStyle = */ 0, /* aExStyle = */ 0);
  ExpectHasEventHandler(hwnd, false);

  OSKInputPaneManager::ShowOnScreenKeyboard(hwnd);
  ExpectHasEventHandler(hwnd, true);
}
