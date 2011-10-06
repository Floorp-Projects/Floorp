/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Arnold <tellrob@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __mozilla_widget_TaskbarPreview_h__
#define __mozilla_widget_TaskbarPreview_h__

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

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

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#endif /* __mozilla_widget_TaskbarPreview_h__ */


