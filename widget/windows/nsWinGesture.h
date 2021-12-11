/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinGesture_h__
#define WinGesture_h__

/*
 * nsWinGesture - Touch input handling for tablet displays.
 */

#include "nsdefs.h"
#include <winuser.h>
#include <tpcshrd.h>
#include "nsPoint.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TouchEvents.h"

// WM_TABLET_QUERYSYSTEMGESTURESTATUS return values
#define TABLET_ROTATE_GESTURE_ENABLE 0x02000000

class nsPointWin : public nsIntPoint {
 public:
  nsPointWin& operator=(const POINTS& aPoint) {
    x = aPoint.x;
    y = aPoint.y;
    return *this;
  }
  nsPointWin& operator=(const POINT& aPoint) {
    x = aPoint.x;
    y = aPoint.y;
    return *this;
  }
  nsPointWin& operator=(int val) {
    x = y = val;
    return *this;
  }
  void ScreenToClient(HWND hWnd) {
    POINT tmp;
    tmp.x = x;
    tmp.y = y;
    ::ScreenToClient(hWnd, &tmp);
    *this = tmp;
  }
};

class nsWinGesture {
 public:
  nsWinGesture();

 public:
  bool SetWinGestureSupport(
      HWND hWnd, mozilla::WidgetGestureNotifyEvent::PanDirection aDirection);
  bool ShutdownWinGestureSupport();

  // Simple gesture process
  bool ProcessGestureMessage(HWND hWnd, WPARAM wParam, LPARAM lParam,
                             mozilla::WidgetSimpleGestureEvent& evt);

  // Pan processing
  bool IsPanEvent(LPARAM lParam);
  bool ProcessPanMessage(HWND hWnd, WPARAM wParam, LPARAM lParam);
  bool PanDeltaToPixelScroll(mozilla::WidgetWheelEvent& aWheelEvent);
  void UpdatePanFeedbackX(HWND hWnd, int32_t scrollOverflow, bool& endFeedback);
  void UpdatePanFeedbackY(HWND hWnd, int32_t scrollOverflow, bool& endFeedback);
  void PanFeedbackFinalize(HWND hWnd, bool endFeedback);

 private:
  // Delay load info
  bool InitLibrary();

  // Pan and feedback state
  nsPointWin mPanIntermediate;
  nsPointWin mPanRefPoint;
  nsPointWin mPixelScrollDelta;
  bool mPanActive;
  bool mFeedbackActive;
  bool mXAxisFeedback;
  bool mYAxisFeedback;
  bool mPanInertiaActive;
  nsPointWin mPixelScrollOverflow;

  // Zoom state
  double mZoomIntermediate;

  // Rotate state
  double mRotateIntermediate;
};

#endif /* WinGesture_h__ */
