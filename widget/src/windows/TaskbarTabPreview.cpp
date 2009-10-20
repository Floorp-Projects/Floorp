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

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7

#include "TaskbarTabPreview.h"
#include "nsWindowGFX.h"
#include "nsUXThemeData.h"
#include <nsITaskbarPreviewController.h>

#define TASKBARPREVIEW_HWNDID L"TaskbarTabPreviewHwnd"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS1(TaskbarTabPreview, nsITaskbarTabPreview)

const PRUnichar *const kWindowClass = L"MozillaTaskbarPreviewClass";

TaskbarTabPreview::TaskbarTabPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell)
  : TaskbarPreview(aTaskbar, aController, aHWND, aShell),
    mProxyWindow(NULL),
    mIcon(NULL),
    mRegistered(PR_FALSE)
{
}

TaskbarTabPreview::~TaskbarTabPreview() {
  if (mIcon) {
    ::DestroyIcon(mIcon);
    mIcon = NULL;
  }
  // Do this here because this is our last chance to execute methods in this class
  (void) SetVisible(PR_FALSE);
}

nsresult
TaskbarTabPreview::ShowActive(PRBool active) {
  NS_ASSERTION(mVisible && CanMakeTaskbarCalls(), "ShowActive called on invisible window or before taskbar calls can be made for this window");
  return FAILED(mTaskbar->SetTabActive(active ? mProxyWindow : NULL, mWnd, 0))
       ? NS_ERROR_FAILURE
       : NS_OK;
}

HWND &
TaskbarTabPreview::PreviewWindow() {
  return mProxyWindow;
}

nativeWindow
TaskbarTabPreview::GetHWND() {
  return mProxyWindow;
}

void
TaskbarTabPreview::EnsureRegistration() {
  NS_ASSERTION(mVisible && CanMakeTaskbarCalls(), "EnsureRegistration called when it is not safe to do so");

  (void) UpdateTaskbarProperties();
}

NS_IMETHODIMP
TaskbarTabPreview::GetTitle(nsAString &aTitle) {
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarTabPreview::SetTitle(const nsAString &aTitle) {
  mTitle = aTitle;
  return mVisible ? UpdateTitle() : NS_OK;
}

NS_IMETHODIMP
TaskbarTabPreview::SetIcon(imgIContainer *icon) {
  HICON hIcon = NULL;
  if (icon) {
    nsresult rv;
    rv = nsWindowGfx::CreateIcon(icon, PR_FALSE, 0, 0, &hIcon);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mIcon)
    ::DestroyIcon(mIcon);
  mIcon = hIcon;
  mIconImage = icon;
  return mVisible ? UpdateIcon() : NS_OK;
}

NS_IMETHODIMP
TaskbarTabPreview::GetIcon(imgIContainer **icon) {
  NS_IF_ADDREF(*icon = mIconImage);
  return NS_OK;
}

NS_IMETHODIMP
TaskbarTabPreview::Move(nsITaskbarTabPreview *aNext) {
  if (aNext == this)
    return NS_ERROR_INVALID_ARG;
  mNext = aNext;
  return CanMakeTaskbarCalls() ? UpdateNext() : NS_OK;
}

nsresult
TaskbarTabPreview::UpdateTaskbarProperties() {
  if (mRegistered)
    return NS_OK;

  if (FAILED(mTaskbar->RegisterTab(mProxyWindow, mWnd)))
    return NS_ERROR_FAILURE;

  nsresult rv = UpdateNext();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = TaskbarPreview::UpdateTaskbarProperties();
  mRegistered = PR_TRUE;
  return rv;
}

LRESULT
TaskbarTabPreview::WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam) {
  nsRefPtr<TaskbarTabPreview> kungFuDeathGrip(this);
  switch (nMsg) {
    case WM_CREATE:
      TaskbarPreview::EnableCustomDrawing(mProxyWindow, PR_TRUE);
      return 0;
    case WM_CLOSE:
      mController->OnClose();
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wParam) != WA_INACTIVE) {
        PRBool activateWindow;
        nsresult rv = mController->OnActivate(&activateWindow);
        if (NS_SUCCEEDED(rv) && activateWindow) {
          ::SetActiveWindow(mWnd);
          if (::IsIconic(mWnd))
            ::ShowWindow(mWnd, SW_RESTORE);
          else
            ::BringWindowToTop(mWnd);
        }
      }
      return 0;
    case WM_GETICON:
      return (LRESULT)mIcon;
  }
  return TaskbarPreview::WndProc(nMsg, wParam, lParam);
}

/* static */
LRESULT CALLBACK
TaskbarTabPreview::GlobalWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam) {
  TaskbarTabPreview *preview(nsnull);
  if (nMsg == WM_CREATE) {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    preview = reinterpret_cast<TaskbarTabPreview*>(cs->lpCreateParams);
    if (!::SetPropW(hWnd, TASKBARPREVIEW_HWNDID, preview))
      NS_ERROR("Could not associate native window with tab preview");
    preview->mProxyWindow = hWnd;
  } else {
    preview = reinterpret_cast<TaskbarTabPreview*>(::GetPropW(hWnd, TASKBARPREVIEW_HWNDID));
  }

  if (preview)
    return preview->WndProc(nMsg, wParam, lParam);
  return ::DefWindowProcW(hWnd, nMsg, wParam, lParam);
}

nsresult
TaskbarTabPreview::Enable() {
  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(NULL);

  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = GlobalWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH) NULL;
    wc.lpszMenuName  = (LPCWSTR) NULL;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }
  ::CreateWindowW(kWindowClass, L"TaskbarPreviewWindow",
                  WS_CAPTION, 0, 0, 200, 60, NULL, NULL, module, this);
  // GlobalWndProc will set mProxyWindow so that WM_CREATE can have a valid HWND
  if (!mProxyWindow)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = TaskbarPreview::Enable();
  nsresult rvUpdate;
  rvUpdate = UpdateTitle();
  if (NS_FAILED(rvUpdate))
    rv = rvUpdate;

  rvUpdate = UpdateIcon();
  if (NS_FAILED(rvUpdate))
    rv = rvUpdate;

  return rv;
}

nsresult
TaskbarTabPreview::Disable() {
  TaskbarPreview::Disable();

  if (FAILED(mTaskbar->UnregisterTab(mProxyWindow)))
    return NS_ERROR_FAILURE;
  mRegistered = PR_FALSE;

  // TaskbarPreview::WndProc will set mProxyWindow to null
  if (!DestroyWindow(mProxyWindow))
    return NS_ERROR_FAILURE;
  mProxyWindow = NULL;
  return NS_OK;
}

void
TaskbarTabPreview::DetachFromNSWindow(PRBool windowIsAlive) {
  (void) SetVisible(PR_FALSE);

  TaskbarPreview::DetachFromNSWindow(windowIsAlive);
}

nsresult
TaskbarTabPreview::UpdateTitle() {
  NS_ASSERTION(mVisible, "UpdateTitle called on invisible preview");

  if (!::SetWindowTextW(mProxyWindow, mTitle.get()))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult
TaskbarTabPreview::UpdateIcon() {
  NS_ASSERTION(mVisible, "UpdateIcon called on invisible preview");

  ::SendMessageW(mProxyWindow, WM_SETICON, ICON_SMALL, (LPARAM)mIcon);

  return NS_OK;
}

nsresult
TaskbarTabPreview::UpdateNext() {
  NS_ASSERTION(CanMakeTaskbarCalls() && mVisible, "UpdateNext called on invisible tab preview");
  HWND hNext = NULL;
  if (mNext) {
    PRBool visible;
    nsresult rv = mNext->GetVisible(&visible);

    NS_ENSURE_SUCCESS(rv, rv);

    // Can only move next to enabled previews
    if (!visible)
      return NS_ERROR_FAILURE;

    hNext = (HWND)mNext->GetHWND();

    // hNext must be registered with the taskbar if the call is to succeed
    mNext->EnsureRegistration();
  }
  if (FAILED(mTaskbar->SetTabOrder(mProxyWindow, hNext)))
    return NS_ERROR_FAILURE;
  return NS_OK;
}


} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
