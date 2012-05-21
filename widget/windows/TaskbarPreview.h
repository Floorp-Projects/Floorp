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

#include <nsITaskbarPreview.h>
#include <nsAutoPtr.h>
#include <nsString.h>
#include <nsWeakPtr.h>
#include <nsIDocShell.h>
#include "WindowHook.h"

namespace mozilla {
namespace widget {

class TaskbarPreview : public nsITaskbarPreview
{
public:
  TaskbarPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell);
  virtual ~TaskbarPreview();

  NS_DECL_NSITASKBARPREVIEW

protected:
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
  WindowHook &GetWindowHook();

  // Enables/disables custom drawing for the given window
  static void EnableCustomDrawing(HWND aHWND, bool aEnable);

  // MSCOM Taskbar interface
  nsRefPtr<ITaskbarList4> mTaskbar;
  // Controller for this preview
  nsCOMPtr<nsITaskbarPreviewController> mController;
  // The HWND to the nsWindow that this object previews
  HWND                    mWnd;
  // Whether or not this preview is visible
  bool                    mVisible;

private:
  // Called when the tooltip should be updated
  nsresult UpdateTooltip();

  // Requests the controller to draw into a canvas of the given width and
  // height. The resulting bitmap is sent to the DWM to display.
  void DrawBitmap(PRUint32 width, PRUint32 height, bool isPreview);

  // WindowHook procedure for hooking mWnd
  static bool MainWindowHook(void *aContext,
                               HWND hWnd, UINT nMsg,
                               WPARAM wParam, LPARAM lParam,
                               LRESULT *aResult);

  // Docshell corresponding to the <window> the nsWindow contains
  nsWeakPtr               mDocShell;
  nsString                mTooltip;

  // The preview currently marked as active in the taskbar. nsnull if no
  // preview is active (some other window is).
  static TaskbarPreview  *sActivePreview;
};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_TaskbarPreview_h__ */

