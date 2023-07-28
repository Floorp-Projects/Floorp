/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_WINDOWS_NSWINDOWTASKBARCONCEALER_H_
#define WIDGET_WINDOWS_NSWINDOWTASKBARCONCEALER_H_

#include "nsWindow.h"
#include "mozilla/Maybe.h"

/**
 * nsWindow::TaskbarConcealer
 *
 * Fullscreen-state (and, thus, taskbar-occlusion) manager.
 */
class nsWindow::TaskbarConcealer {
 public:
  // To be called when a window acquires focus. (Note that no action need be
  // taken when focus is lost.)
  static void OnFocusAcquired(nsWindow* aWin);

  // To be called during or after a window's destruction. The corresponding
  // nsWindow pointer is not needed, and will not be acquired or accessed.
  static void OnWindowDestroyed(HWND aWnd);

  // To be called when the Gecko-fullscreen state of a window changes.
  static void OnFullscreenChanged(nsWindow* aWin, bool enteredFullscreen);

  // To be called when the position of a window changes. (Performs its own
  // batching; irrelevant movements will be cheap.)
  static void OnWindowPosChanged(nsWindow* aWin);

  // To be called when the cloaking state of any window changes. (Expects that
  // all windows' internal cloaking-state mirror variables are up-to-date.)
  static void OnCloakChanged();

  // To be called upon receipt of MOZ_WM_FULLSCREEN_STATE_UPDATE.
  static void OnAsyncStateUpdateRequest(HWND);

 private:
  static void UpdateAllState(HWND destroyedHwnd = nullptr);

  struct WindowState {
    HMONITOR monitor;
    bool isGkFullscreen;
  };
  static mozilla::Maybe<WindowState> GetWindowState(HWND);

  static nsTHashMap<HWND, HMONITOR> sKnownWindows;
};

#endif  // WIDGET_WINDOWS_NSWINDOWTASKBARCONCEALER_H_
