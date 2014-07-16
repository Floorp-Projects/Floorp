/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_TaskbarTabPreview_h__
#define __mozilla_widget_TaskbarTabPreview_h__

#include "nsITaskbarTabPreview.h"
#include "TaskbarPreview.h"

namespace mozilla {
namespace widget {

class TaskbarTabPreview : public nsITaskbarTabPreview,
                          public TaskbarPreview
{
  virtual ~TaskbarTabPreview();

public:
  TaskbarTabPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARTABPREVIEW
  NS_FORWARD_NSITASKBARPREVIEW(TaskbarPreview::)

private:
  virtual nsresult ShowActive(bool active);
  virtual HWND &PreviewWindow();
  virtual LRESULT WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK GlobalWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

  virtual nsresult UpdateTaskbarProperties();
  virtual nsresult Enable();
  virtual nsresult Disable();
  virtual void DetachFromNSWindow();

  // WindowHook procedure for hooking mWnd
  static bool MainWindowHook(void *aContext,
                               HWND hWnd, UINT nMsg,
                               WPARAM wParam, LPARAM lParam,
                               LRESULT *aResult);

  // Bug 520807 - we need to update the proxy window style based on the main
  // window's style to workaround a bug with the way the DWM displays the
  // previews.
  void UpdateProxyWindowStyle();

  nsresult UpdateTitle();
  nsresult UpdateIcon();
  nsresult UpdateNext();

  // Handle to the toplevel proxy window
  HWND                    mProxyWindow;
  nsString                mTitle;
  nsCOMPtr<imgIContainer> mIconImage;
  // Cached Windows icon of mIconImage
  HICON                   mIcon;
  // Preview that follows this preview in the taskbar (left-to-right order)
  nsCOMPtr<nsITaskbarTabPreview> mNext;
  // True if this preview has been registered with the taskbar
  bool                    mRegistered;
};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_TaskbarTabPreview_h__ */
