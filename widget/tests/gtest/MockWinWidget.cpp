/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MockWinWidget.h"

#include "mozilla/gfx/Logging.h"

NS_IMPL_ISUPPORTS_INHERITED0(MockWinWidget, nsBaseWidget)

// static
RefPtr<MockWinWidget> MockWinWidget::Create(DWORD aStyle, DWORD aExStyle,
                                            const LayoutDeviceIntRect& aRect) {
  RefPtr<MockWinWidget> window = new MockWinWidget;
  if (!window->Initialize(aStyle, aExStyle, aRect)) {
    return nullptr;
  }

  return window;
}

MockWinWidget::MockWinWidget() {}

MockWinWidget::~MockWinWidget() {
  if (mWnd) {
    ::DestroyWindow(mWnd);
    mWnd = 0;
  }
}

bool MockWinWidget::Initialize(DWORD aStyle, DWORD aExStyle,
                               const LayoutDeviceIntRect& aRect) {
  WNDCLASSW wc;
  const wchar_t className[] = L"MozillaMockWinWidget";
  HMODULE hSelf = ::GetModuleHandle(nullptr);

  if (!GetClassInfoW(hSelf, className, &wc)) {
    ZeroMemory(&wc, sizeof(WNDCLASSW));
    wc.hInstance = hSelf;
    wc.lpfnWndProc = ::DefWindowProc;
    wc.lpszClassName = className;
    RegisterClassW(&wc);
  }

  mWnd = ::CreateWindowExW(aExStyle, className, className, aStyle, aRect.X(),
                           aRect.Y(), aRect.Width(), aRect.Height(), nullptr,
                           nullptr, hSelf, nullptr);
  if (!mWnd) {
    gfxCriticalNoteOnce << "GetClientRect failed " << ::GetLastError();
    return false;
  }

  // First nccalcszie (during CreateWindow) for captioned windows is
  // deliberately ignored so force a second one here to get the right
  // non-client set up.
  if (mWnd && (aStyle & WS_CAPTION)) {
    ::SetWindowPos(mWnd, NULL, 0, 0, 0, 0,
                   SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                       SWP_NOACTIVATE | SWP_NOREDRAW);
  }

  mBounds = aRect;
  return true;
}

void MockWinWidget::NotifyOcclusionState(
    mozilla::widget::OcclusionState aState) {
  mCurrentState = aState;
}

nsSizeMode MockWinWidget::SizeMode() {
  if (::IsIconic(mWnd)) {
    return nsSizeMode_Minimized;
  }
  return nsSizeMode_Normal;
}
