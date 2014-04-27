/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskbarTabPreview.h"
#include "nsWindowGfx.h"
#include "nsUXThemeData.h"
#include "WinUtils.h"
#include <nsITaskbarPreviewController.h>

#define TASKBARPREVIEW_HWNDID L"TaskbarTabPreviewHwnd"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(TaskbarTabPreview, nsITaskbarTabPreview)

const wchar_t *const kWindowClass = L"MozillaTaskbarPreviewClass";

TaskbarTabPreview::TaskbarTabPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell)
  : TaskbarPreview(aTaskbar, aController, aHWND, aShell),
    mProxyWindow(nullptr),
    mIcon(nullptr),
    mRegistered(false)
{
  WindowHook &hook = GetWindowHook();
  hook.AddMonitor(WM_WINDOWPOSCHANGED, MainWindowHook, this);
}

TaskbarTabPreview::~TaskbarTabPreview() {
  if (mIcon) {
    ::DestroyIcon(mIcon);
    mIcon = nullptr;
  }

  // We need to ensure that proxy window disappears or else Bad Things happen.
  if (mProxyWindow)
    Disable();

  NS_ASSERTION(!mProxyWindow, "Taskbar proxy window was not destroyed!");

  if (IsWindowAvailable()) {
    DetachFromNSWindow();
  } else {
    mWnd = nullptr;
  }
}

nsresult
TaskbarTabPreview::ShowActive(bool active) {
  NS_ASSERTION(mVisible && CanMakeTaskbarCalls(), "ShowActive called on invisible window or before taskbar calls can be made for this window");
  return FAILED(mTaskbar->SetTabActive(active ? mProxyWindow : nullptr,
                                       mWnd, 0)) ? NS_ERROR_FAILURE : NS_OK;
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
  HICON hIcon = nullptr;
  if (icon) {
    nsresult rv;
    rv = nsWindowGfx::CreateIcon(icon, false, 0, 0,
                                 nsWindowGfx::GetIconMetrics(nsWindowGfx::kSmallIcon),
                                 &hIcon);
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
  mRegistered = true;
  return rv;
}

LRESULT
TaskbarTabPreview::WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam) {
  nsRefPtr<TaskbarTabPreview> kungFuDeathGrip(this);
  switch (nMsg) {
    case WM_CREATE:
      TaskbarPreview::EnableCustomDrawing(mProxyWindow, true);
      return 0;
    case WM_CLOSE:
      mController->OnClose();
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_ACTIVE) {
        // Activate the tab the user selected then restore the main window,
        // keeping normal/max window state intact.
        bool activateWindow;
        nsresult rv = mController->OnActivate(&activateWindow);
        if (NS_SUCCEEDED(rv) && activateWindow) {
          nsWindow* win = WinUtils::GetNSWindowPtr(mWnd);
          if (win) {
            nsWindow * parent = win->GetTopLevelWindow(true);
            if (parent) {
              parent->Show(true);
            }
          }
        }
      }
      return 0;
    case WM_GETICON:
      return (LRESULT)mIcon;
    case WM_SYSCOMMAND:
      // Send activation events to the top level window and select the proper
      // tab through the controller.
      if (wParam == SC_RESTORE || wParam == SC_MAXIMIZE) {
        bool activateWindow;
        nsresult rv = mController->OnActivate(&activateWindow);
        if (NS_SUCCEEDED(rv) && activateWindow) {
          // Note, restoring an iconic, maximized window here will only
          // activate the maximized window. This is not a bug, it's default
          // windows behavior.
          ::SendMessageW(mWnd, WM_SYSCOMMAND, wParam, lParam);
        }
        return 0;
      }
      // Forward everything else to the top level window. Do not forward
      // close since that's intended for the tab. When the preview proxy
      // closes, we'll close the tab above.
      return wParam == SC_CLOSE
           ? ::DefWindowProcW(mProxyWindow, WM_SYSCOMMAND, wParam, lParam)
           : ::SendMessageW(mWnd, WM_SYSCOMMAND, wParam, lParam);
      return 0;
  }
  return TaskbarPreview::WndProc(nMsg, wParam, lParam);
}

/* static */
LRESULT CALLBACK
TaskbarTabPreview::GlobalWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam) {
  TaskbarTabPreview *preview(nullptr);
  if (nMsg == WM_CREATE) {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    preview = reinterpret_cast<TaskbarTabPreview*>(cs->lpCreateParams);
    if (!::SetPropW(hWnd, TASKBARPREVIEW_HWNDID, preview))
      NS_ERROR("Could not associate native window with tab preview");
    preview->mProxyWindow = hWnd;
  } else {
    preview = reinterpret_cast<TaskbarTabPreview*>(::GetPropW(hWnd, TASKBARPREVIEW_HWNDID));
    if (nMsg == WM_DESTROY)
      ::RemovePropW(hWnd, TASKBARPREVIEW_HWNDID);
  }

  if (preview)
    return preview->WndProc(nMsg, wParam, lParam);
  return ::DefWindowProcW(hWnd, nMsg, wParam, lParam);
}

nsresult
TaskbarTabPreview::Enable() {
  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(nullptr);

  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = GlobalWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = (HBRUSH) nullptr;
    wc.lpszMenuName  = (LPCWSTR) nullptr;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }
  ::CreateWindowW(kWindowClass, L"TaskbarPreviewWindow",
                  WS_CAPTION | WS_SYSMENU, 0, 0, 200, 60,
                  nullptr, nullptr, module, this);
  // GlobalWndProc will set mProxyWindow so that WM_CREATE can have a valid HWND
  if (!mProxyWindow)
    return NS_ERROR_INVALID_ARG;

  UpdateProxyWindowStyle();

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
  // TaskbarPreview::Disable assumes that mWnd is valid but this method can be
  // called when it is null iff the nsWindow has already been destroyed and we
  // are still visible for some reason during object destruction.
  if (mWnd)
    TaskbarPreview::Disable();

  if (FAILED(mTaskbar->UnregisterTab(mProxyWindow)))
    return NS_ERROR_FAILURE;
  mRegistered = false;

  // TaskbarPreview::WndProc will set mProxyWindow to null
  if (!DestroyWindow(mProxyWindow))
    return NS_ERROR_FAILURE;
  mProxyWindow = nullptr;
  return NS_OK;
}

void
TaskbarTabPreview::DetachFromNSWindow() {
  (void) SetVisible(false);
  WindowHook &hook = GetWindowHook();
  hook.RemoveMonitor(WM_WINDOWPOSCHANGED, MainWindowHook, this);

  TaskbarPreview::DetachFromNSWindow();
}

/* static */
bool
TaskbarTabPreview::MainWindowHook(void *aContext,
                                  HWND hWnd, UINT nMsg,
                                  WPARAM wParam, LPARAM lParam,
                                  LRESULT *aResult) {
  if (nMsg == WM_WINDOWPOSCHANGED) {
    TaskbarTabPreview *preview = reinterpret_cast<TaskbarTabPreview*>(aContext);
    WINDOWPOS *pos = reinterpret_cast<WINDOWPOS*>(lParam);
    if (SWP_FRAMECHANGED == (pos->flags & SWP_FRAMECHANGED))
      preview->UpdateProxyWindowStyle();
  } else {
    NS_NOTREACHED("Style changed hook fired on non-style changed message");
  }
  return false;
}

void
TaskbarTabPreview::UpdateProxyWindowStyle() {
  if (!mProxyWindow)
    return;

  DWORD minMaxMask = WS_MINIMIZE | WS_MAXIMIZE;
  DWORD windowStyle = GetWindowLongW(mWnd, GWL_STYLE);

  DWORD proxyStyle = GetWindowLongW(mProxyWindow, GWL_STYLE);
  proxyStyle &= ~minMaxMask;
  proxyStyle |= windowStyle & minMaxMask;
  SetWindowLongW(mProxyWindow, GWL_STYLE, proxyStyle);

  DWORD exStyle = (WS_MAXIMIZE == (windowStyle & WS_MAXIMIZE)) ? WS_EX_TOOLWINDOW : 0;
  SetWindowLongW(mProxyWindow, GWL_EXSTYLE, exStyle);
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
  HWND hNext = nullptr;
  if (mNext) {
    bool visible;
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

