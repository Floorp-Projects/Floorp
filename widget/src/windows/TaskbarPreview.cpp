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
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "TaskbarPreview.h"
#include <nsITaskbarPreviewController.h>
#include <windows.h>

#include <nsError.h>
#include <nsCOMPtr.h>
#include <nsIWidget.h>
#include <nsIBaseWindow.h>
#include <nsIObserverService.h>
#include <nsServiceManagerUtils.h>

#include "nsUXThemeData.h"
#include "nsWindow.h"
#include "nsAppShell.h"
#include "TaskbarPreviewButton.h"

#include <nsIBaseWindow.h>
#include <nsICanvasRenderingContextInternal.h>
#include <nsIDOMCanvasRenderingContext2D.h>
#include <imgIContainer.h>
#include <nsIDocShell.h>

// Defined in dwmapi in a header that needs a higher numbered _WINNT #define
#define DWM_SIT_DISPLAYFRAME 0x1

namespace mozilla {
namespace widget {

namespace {

// Shared by all TaskbarPreviews to avoid the expensive creation process.
// Manually refcounted (see gInstCount) by the ctor and dtor of TaskbarPreview.
// This is done because static constructors aren't allowed for perf reasons.
nsIDOMCanvasRenderingContext2D* gCtx = NULL;
// Used in tracking the number of previews. Used in freeing
// the static 2d rendering context on shutdown.
PRUint32 gInstCount = 0;

/* Helper method to lazily create a canvas rendering context and associate a given
 * surface with it.
 *
 * @param shell The docShell used by the canvas context for text settings and other
 *              misc things.
 * @param surface The gfxSurface backing the context
 * @param width The width of the given surface
 * @param height The height of the given surface
 */
nsresult
GetRenderingContext(nsIDocShell *shell, gfxASurface *surface,
                    PRUint32 width, PRUint32 height) {
  nsresult rv;
  nsCOMPtr<nsIDOMCanvasRenderingContext2D> ctx = gCtx;

  if (!ctx) {
    // create the canvas rendering context
    ctx = do_CreateInstance("@mozilla.org/content/2dthebes-canvas-rendering-context;1", &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Could not create nsICanvasRenderingContext2D for tab previews!");
      return rv;
    }
    gCtx = ctx;
    NS_ADDREF(gCtx);
  }

  nsCOMPtr<nsICanvasRenderingContextInternal> ctxI = do_QueryInterface(ctx, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Set the surface we'll use to render.
  return ctxI->InitializeWithSurface(shell, surface, width, height);
}

/* Helper method for freeing surface resources associated with the rendering context.
 */
void
ResetRenderingContext() {
  if (!gCtx)
    return;

  nsresult rv;
  nsCOMPtr<nsICanvasRenderingContextInternal> ctxI = do_QueryInterface(gCtx, &rv);
  if (NS_FAILED(rv))
    return;
  if (NS_FAILED(ctxI->Reset())) {
    NS_RELEASE(gCtx);
    gCtx = nsnull;
  }
}

}

TaskbarPreview::TaskbarPreview(ITaskbarList4 *aTaskbar, nsITaskbarPreviewController *aController, HWND aHWND, nsIDocShell *aShell)
  : mTaskbar(aTaskbar),
    mController(aController),
    mWnd(aHWND),
    mVisible(PR_FALSE),
    mDocShell(do_GetWeakReference(aShell))
{
  // TaskbarPreview may outlive the WinTaskbar that created it
  ::CoInitialize(NULL);

  gInstCount++;

  WindowHook &hook = GetWindowHook();
  hook.AddMonitor(WM_DESTROY, MainWindowHook, this);
}

TaskbarPreview::~TaskbarPreview() {
  // Avoid dangling pointer
  if (sActivePreview == this)
    sActivePreview = nsnull;

  // Our subclass should have invoked DetachFromNSWindow already.
  NS_ASSERTION(!mWnd, "TaskbarPreview::DetachFromNSWindow was not called before destruction");

  // Make sure to release before potentially uninitializing COM
  mTaskbar = NULL;

  if (--gInstCount == 0)
    NS_IF_RELEASE(gCtx);

  ::CoUninitialize();
}

NS_IMETHODIMP
TaskbarPreview::SetController(nsITaskbarPreviewController *aController) {
  NS_ENSURE_ARG(aController);

  mController = aController;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::GetController(nsITaskbarPreviewController **aController) {
  NS_ADDREF(*aController = mController);
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::GetTooltip(nsAString &aTooltip) {
  aTooltip = mTooltip;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::SetTooltip(const nsAString &aTooltip) {
  mTooltip = aTooltip;
  return CanMakeTaskbarCalls() ? UpdateTooltip() : NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::SetVisible(PRBool visible) {
  if (mVisible == visible) return NS_OK;
  mVisible = visible;

  // If the nsWindow has already been destroyed but the caller is still trying
  // to use it then just pretend that everything succeeded.  The caller doesn't
  // actually have a way to detect this since it's the same case as when we
  // CanMakeTaskbarCalls returns false.
  if (!mWnd)
    return NS_OK;

  return visible ? Enable() : Disable();
}

NS_IMETHODIMP
TaskbarPreview::GetVisible(PRBool *visible) {
  *visible = mVisible;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::SetActive(PRBool active) {
  if (active)
    sActivePreview = this;
  else if (sActivePreview == this)
    sActivePreview = NULL;

  return CanMakeTaskbarCalls() ? ShowActive(active) : NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::GetActive(PRBool *active) {
  *active = sActivePreview == this;
  return NS_OK;
}

NS_IMETHODIMP
TaskbarPreview::Invalidate() {
  if (!mVisible)
    return NS_ERROR_FAILURE;

  // DWM Composition is required for previews
  if (!nsUXThemeData::CheckForCompositor())
    return NS_OK;

  HWND previewWindow = PreviewWindow();
  return FAILED(nsUXThemeData::dwmInvalidateIconicBitmapsPtr(previewWindow))
       ? NS_ERROR_FAILURE
       : NS_OK;
}

nsresult
TaskbarPreview::UpdateTaskbarProperties() {
  nsresult rv = UpdateTooltip();

  // If we are the active preview and our window is the active window, restore
  // our active state - otherwise some other non-preview window is now active
  // and should be displayed as so.
  if (sActivePreview == this) {
    if (mWnd == ::GetActiveWindow()) {
      nsresult rvActive = ShowActive(PR_TRUE);
      if (NS_FAILED(rvActive))
        rv = rvActive;
    } else {
      sActivePreview = nsnull;
    }
  }
  return rv;
}

nsresult
TaskbarPreview::Enable() {
  nsresult rv = NS_OK;
  if (CanMakeTaskbarCalls()) {
    rv = UpdateTaskbarProperties();
  } else {
    WindowHook &hook = GetWindowHook();
    hook.AddMonitor(nsAppShell::GetTaskbarButtonCreatedMessage(), MainWindowHook, this);
  }
  return rv;
}

nsresult
TaskbarPreview::Disable() {
  WindowHook &hook = GetWindowHook();
  (void) hook.RemoveMonitor(nsAppShell::GetTaskbarButtonCreatedMessage(), MainWindowHook, this);

  return NS_OK;
}

void
TaskbarPreview::DetachFromNSWindow() {
  WindowHook &hook = GetWindowHook();
  hook.RemoveMonitor(WM_DESTROY, MainWindowHook, this);
  mWnd = NULL;
}

LRESULT
TaskbarPreview::WndProc(UINT nMsg, WPARAM wParam, LPARAM lParam) {
  switch (nMsg) {
    case WM_DWMSENDICONICTHUMBNAIL:
      {
        PRUint32 width = HIWORD(lParam);
        PRUint32 height = LOWORD(lParam);
        float aspectRatio = width/float(height);

        nsresult rv;
        float preferredAspectRatio;
        rv = mController->GetThumbnailAspectRatio(&preferredAspectRatio);
        if (NS_FAILED(rv))
          break;

        PRUint32 thumbnailWidth = width;
        PRUint32 thumbnailHeight = height;

        if (aspectRatio > preferredAspectRatio) {
          thumbnailWidth = PRUint32(thumbnailHeight * preferredAspectRatio);
        } else {
          thumbnailHeight = PRUint32(thumbnailWidth / preferredAspectRatio);
        }

        DrawBitmap(thumbnailWidth, thumbnailHeight, PR_FALSE);
      }
      break;
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
      {
        PRUint32 width, height;
        nsresult rv;
        rv = mController->GetWidth(&width);
        if (NS_FAILED(rv))
          break;
        rv = mController->GetHeight(&height);
        if (NS_FAILED(rv))
          break;

        DrawBitmap(width, height, PR_TRUE);
      }
      break;
  }
  return ::DefWindowProcW(PreviewWindow(), nMsg, wParam, lParam);
}

PRBool
TaskbarPreview::CanMakeTaskbarCalls() {
  // If the nsWindow has already been destroyed and we know it but our caller
  // clearly doesn't so we can't make any calls.
  if (!mWnd)
    return PR_FALSE;
  // Certain functions like SetTabOrder seem to require a visible window. During
  // window close, the window seems to be hidden before being destroyed.
  if (!::IsWindowVisible(mWnd))
    return PR_FALSE;
  if (mVisible) {
    nsWindow *window = nsWindow::GetNSWindowPtr(mWnd);
    NS_ASSERTION(window, "Could not get nsWindow from HWND");
    return window->HasTaskbarIconBeenCreated();
  }
  return PR_FALSE;
}

WindowHook&
TaskbarPreview::GetWindowHook() {
  nsWindow *window = nsWindow::GetNSWindowPtr(mWnd);
  NS_ASSERTION(window, "Cannot use taskbar previews in an embedded context!");

  return window->GetWindowHook();
}

void
TaskbarPreview::EnableCustomDrawing(HWND aHWND, PRBool aEnable) {
  nsUXThemeData::dwmSetWindowAttributePtr(
      aHWND,
      DWMWA_FORCE_ICONIC_REPRESENTATION,
      &aEnable,
      sizeof(aEnable));

  nsUXThemeData::dwmSetWindowAttributePtr(
      aHWND,
      DWMWA_HAS_ICONIC_BITMAP,
      &aEnable,
      sizeof(aEnable));
}


nsresult
TaskbarPreview::UpdateTooltip() {
  NS_ASSERTION(CanMakeTaskbarCalls() && mVisible, "UpdateTooltip called on invisible tab preview");

  if (FAILED(mTaskbar->SetThumbnailTooltip(PreviewWindow(), mTooltip.get())))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

void
TaskbarPreview::DrawBitmap(PRUint32 width, PRUint32 height, PRBool isPreview) {
  nsresult rv;
  nsRefPtr<gfxWindowsSurface> surface = new gfxWindowsSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);

  nsCOMPtr<nsIDocShell> shell = do_QueryReferent(mDocShell);

  if (!shell)
    return;

  rv = GetRenderingContext(shell, surface, width, height);
  if (NS_FAILED(rv))
    return;

  PRBool drawFrame = PR_FALSE;
  if (isPreview)
    rv = mController->DrawPreview(gCtx, &drawFrame);
  else
    rv = mController->DrawThumbnail(gCtx, width, height, &drawFrame);

  if (NS_FAILED(rv))
    return;

  HDC hDC = surface->GetDC();
  HBITMAP hBitmap = (HBITMAP)GetCurrentObject(hDC, OBJ_BITMAP);

  DWORD flags = drawFrame ? DWM_SIT_DISPLAYFRAME : 0;
  POINT pptClient = { 0, 0 };
  if (isPreview)
    nsUXThemeData::dwmSetIconicLivePreviewBitmapPtr(PreviewWindow(), hBitmap, &pptClient, flags);
  else
    nsUXThemeData::dwmSetIconicThumbnailPtr(PreviewWindow(), hBitmap, flags);

  ResetRenderingContext();
}

/* static */
PRBool
TaskbarPreview::MainWindowHook(void *aContext,
                               HWND hWnd, UINT nMsg,
                               WPARAM wParam, LPARAM lParam,
                               LRESULT *aResult)
{
  NS_ASSERTION(nMsg == nsAppShell::GetTaskbarButtonCreatedMessage() ||
               nMsg == WM_DESTROY,
               "Window hook proc called with wrong message");
  TaskbarPreview *preview = reinterpret_cast<TaskbarPreview*>(aContext);
  if (nMsg == WM_DESTROY) {
    // nsWindow is being destroyed
    // We can't really do anything at this point including removing hooks
    preview->mWnd = NULL;
  } else {
    nsWindow *window = nsWindow::GetNSWindowPtr(preview->mWnd);
    NS_ASSERTION(window, "Cannot use taskbar previews in an embedded context!");

    window->SetHasTaskbarIconBeenCreated();

    if (preview->mVisible)
      preview->UpdateTaskbarProperties();
  }
  return PR_FALSE;
}

TaskbarPreview *
TaskbarPreview::sActivePreview = nsnull;

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
