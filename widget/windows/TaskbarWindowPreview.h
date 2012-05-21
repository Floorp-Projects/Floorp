/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_TaskbarWindowPreview_h__
#define __mozilla_widget_TaskbarWindowPreview_h__

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

#endif /* __mozilla_widget_TaskbarWindowPreview_h__ */

