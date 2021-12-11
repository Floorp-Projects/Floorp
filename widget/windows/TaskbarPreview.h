/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_TaskbarPreview_h__
#define __mozilla_widget_TaskbarPreview_h__

#include <windows.h>
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD

#include "mozilla/RefPtr.h"
#include <nsITaskbarPreview.h>
#include <nsITaskbarPreviewController.h>
#include <nsString.h>
#include <nsIWeakReferenceUtils.h>
#include <nsIDocShell.h>
#include "WindowHook.h"

namespace mozilla {
namespace widget {

class TaskbarPreviewCallback;

class TaskbarPreview : public nsITaskbarPreview {
 public:
  TaskbarPreview(ITaskbarList4* aTaskbar,
                 nsITaskbarPreviewController* aController, HWND aHWND,
                 nsIDocShell* aShell);
  virtual nsresult Init();

  friend class TaskbarPreviewCallback;

  NS_DECL_NSITASKBARPREVIEW

 protected:
  virtual ~TaskbarPreview();

  // Called to update ITaskbarList4 dependent properties
  virtual nsresult UpdateTaskbarProperties();

  // Invoked when the preview is made visible
  virtual nsresult Enable();
  // Invoked when the preview is made invisible
  virtual nsresult Disable();

  // Detaches this preview from the nsWindow instance it's tied to
  virtual void DetachFromNSWindow();

  // Determines if the window is available and a destroy has not yet started
  bool IsWindowAvailable() const;

  // Marks this preview as being active
  virtual nsresult ShowActive(bool active) = 0;
  // Gets a reference to the window used to handle the preview messages
  virtual HWND& PreviewWindow() = 0;

  // Window procedure for the PreviewWindow (hooked for window previews)
  virtual LRESULT WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam);

  // Returns whether or not the taskbar icon has been created for mWnd The
  // ITaskbarList4 API requires that we wait until the icon has been created
  // before we can call its methods.
  bool CanMakeTaskbarCalls();

  // Gets the WindowHook for the nsWindow
  WindowHook* GetWindowHook();

  // Enables/disables custom drawing for the given window
  static void EnableCustomDrawing(HWND aHWND, bool aEnable);

  // MSCOM Taskbar interface
  RefPtr<ITaskbarList4> mTaskbar;
  // Controller for this preview
  nsCOMPtr<nsITaskbarPreviewController> mController;
  // The HWND to the nsWindow that this object previews
  HWND mWnd;
  // Whether or not this preview is visible
  bool mVisible;

 private:
  // Called when the tooltip should be updated
  nsresult UpdateTooltip();

  // Requests the controller to draw into a canvas of the given width and
  // height. The resulting bitmap is sent to the DWM to display.
  void DrawBitmap(uint32_t width, uint32_t height, bool isPreview);

  // WindowHook procedure for hooking mWnd
  static bool MainWindowHook(void* aContext, HWND hWnd, UINT nMsg,
                             WPARAM wParam, LPARAM lParam, LRESULT* aResult);

  // Docshell corresponding to the <window> the nsWindow contains
  nsWeakPtr mDocShell;
  nsString mTooltip;

  // The preview currently marked as active in the taskbar. nullptr if no
  // preview is active (some other window is).
  static TaskbarPreview* sActivePreview;
};

/*
 * Callback object TaskbarPreview hands to preview controllers when we
 * request async thumbnail or live preview images. Controllers invoke
 * this interface once they have aquired the requested image.
 */
class TaskbarPreviewCallback : public nsITaskbarPreviewCallback {
 public:
  TaskbarPreviewCallback() : mIsThumbnail(true) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARPREVIEWCALLBACK

  void SetPreview(TaskbarPreview* aPreview) { mPreview = aPreview; }

  void SetIsPreview() { mIsThumbnail = false; }

 protected:
  virtual ~TaskbarPreviewCallback() {}

 private:
  RefPtr<TaskbarPreview> mPreview;
  bool mIsThumbnail;
};

}  // namespace widget
}  // namespace mozilla

#endif /* __mozilla_widget_TaskbarPreview_h__ */
