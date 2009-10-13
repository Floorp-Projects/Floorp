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

#ifndef __mozilla_widget_TaskbarTabPreview_h__
#define __mozilla_widget_TaskbarTabPreview_h__

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#include "nsITaskbarTabPreview.h"
#include "TaskbarPreview.h"

namespace mozilla {
namespace widget {

class TaskbarTabPreview : public nsITaskbarTabPreview,
                          public TaskbarPreview
{
public:
  TaskbarTabPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell);
  virtual ~TaskbarTabPreview();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARTABPREVIEW
  NS_FORWARD_NSITASKBARPREVIEW(TaskbarPreview::)

private:
  virtual nsresult ShowActive(PRBool active);
  virtual HWND &PreviewWindow();
  virtual LRESULT WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK GlobalWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

  virtual nsresult UpdateTaskbarProperties();
  virtual nsresult Enable();
  virtual nsresult Disable();
  virtual void DetachFromNSWindow(PRBool windowIsAlive);
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
  PRBool                  mRegistered;
};

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#endif /* __mozilla_widget_TaskbarTabPreview_h__ */
