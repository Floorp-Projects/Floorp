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
 *   Siddharth Agarwal <sid.bugzilla@gmail.com>
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

#ifndef __mozilla_widget_TaskbarWindowPreview_h__
#define __mozilla_widget_TaskbarWindowPreview_h__

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#include "nsITaskbarWindowPreview.h"
#include "nsITaskbarProgress.h"
#include "nsITaskbarOverlayIconController.h"
#include "TaskbarPreview.h"
#include <nsWeakReference.h>

namespace mozilla {
namespace widget {

class TaskbarPreviewButton;
class TaskbarWindowPreview : public TaskbarPreview,
                             public nsITaskbarWindowPreview,
                             public nsITaskbarProgress,
                             public nsITaskbarOverlayIconController,
                             public nsSupportsWeakReference
{
public:
  TaskbarWindowPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell);
  virtual ~TaskbarWindowPreview();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARWINDOWPREVIEW
  NS_DECL_NSITASKBARPROGRESS
  NS_DECL_NSITASKBAROVERLAYICONCONTROLLER
  NS_FORWARD_NSITASKBARPREVIEW(TaskbarPreview::)

  virtual LRESULT WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam);
private:
  virtual nsresult ShowActive(bool active);
  virtual HWND &PreviewWindow();

  virtual nsresult UpdateTaskbarProperties();
  virtual nsresult Enable();
  virtual nsresult Disable();
  virtual void DetachFromNSWindow();
  nsresult UpdateButton(PRUint32 index);
  nsresult UpdateButtons();

  // Is custom drawing enabled?
  bool                    mCustomDrawing;
  // Have we made any buttons?
  bool                    mHaveButtons;
  // Windows button format
  THUMBBUTTON             mThumbButtons[nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS];
  // Pointers to our button class (cached instances)
  nsWeakPtr               mWeakButtons[nsITaskbarWindowPreview::NUM_TOOLBAR_BUTTONS];

  // Called to update ITaskbarList4 dependent properties
  nsresult UpdateTaskbarProgress();
  nsresult UpdateOverlayIcon();

  // The taskbar progress
  TBPFLAG                 mState;
  ULONGLONG               mCurrentValue;
  ULONGLONG               mMaxValue;

  // Taskbar overlay icon
  HICON                   mOverlayIcon;
  nsString                mIconDescription;

  // WindowHook procedure for hooking mWnd for taskbar progress and icon stuff
  static bool TaskbarWindowHook(void *aContext,
                                  HWND hWnd, UINT nMsg,
                                  WPARAM wParam, LPARAM lParam,
                                  LRESULT *aResult);

  friend class TaskbarPreviewButton;
};

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#endif /* __mozilla_widget_TaskbarWindowPreview_h__ */

