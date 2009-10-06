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

#include "WinTaskbar.h"
#include "TaskbarPreview.h"
#include <nsITaskbarPreviewController.h>

#include <nsError.h>
#include <nsCOMPtr.h>
#include <nsIWidget.h>
#include <nsIBaseWindow.h>
#include <nsIObserverService.h>
#include <nsServiceManagerUtils.h>
#include <nsAutoPtr.h>
#include "nsUXThemeData.h"
#include "nsWindow.h"
#include "TaskbarTabPreview.h"
#include "TaskbarWindowPreview.h"
#include <io.h>

namespace {
HWND
GetHWNDFromDocShell(nsIDocShell *aShell) {
  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(reinterpret_cast<nsISupports*>(aShell)));

  if (!baseWindow)
    return NULL;

  nsCOMPtr<nsIWidget> widget;
  baseWindow->GetMainWidget(getter_AddRefs(widget));

  return widget ? (HWND)widget->GetNativeData(NS_NATIVE_WINDOW) : NULL;
}

// Default controller for TaskbarWindowPreviews
class DefaultController : public nsITaskbarPreviewController
{
  HWND mWnd;
public:
  DefaultController(HWND hWnd) 
    : mWnd(hWnd)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARPREVIEWCONTROLLER
};

NS_IMETHODIMP
DefaultController::GetWidth(PRUint32 *aWidth)
{
  RECT r;
  ::GetClientRect(mWnd, &r);
  *aWidth = r.right;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::GetHeight(PRUint32 *aHeight)
{
  RECT r;
  ::GetClientRect(mWnd, &r);
  *aHeight = r.bottom;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::GetThumbnailAspectRatio(float *aThumbnailAspectRatio) {
  PRUint32 width, height;
  GetWidth(&width);
  GetHeight(&height);
  if (!height)
    height = 1;

  *aThumbnailAspectRatio = width/float(height);
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::DrawPreview(nsIDOMCanvasRenderingContext2D *ctx, PRBool *rDrawFrame) {
  *rDrawFrame = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::DrawThumbnail(nsIDOMCanvasRenderingContext2D *ctx, PRUint32 width, PRUint32 height, PRBool *rDrawFrame) {
  *rDrawFrame = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnClose(void) {
  NS_NOTREACHED("OnClose should not be called for TaskbarWindowPreviews");
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnActivate(PRBool *rAcceptActivation) {
  *rAcceptActivation = PR_TRUE;
  NS_NOTREACHED("OnActivate should not be called for TaskbarWindowPreviews");
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnClick(nsITaskbarPreviewButton *button) {
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(DefaultController, nsITaskbarPreviewController);
}

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS1(WinTaskbar, nsIWinTaskbar)

WinTaskbar::WinTaskbar() 
  : mTaskbar(nsnull) {
  ::CoInitialize(NULL);
  HRESULT hr = ::CoCreateInstance(CLSID_TaskbarList,
                                  NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ITaskbarList4,
                                  (void**)&mTaskbar);
  if (FAILED(hr))
    return;

  hr = mTaskbar->HrInit();
  if (FAILED(hr)) {
    NS_WARNING("Unable to initialize taskbar");
    NS_RELEASE(mTaskbar);
  }
}

WinTaskbar::~WinTaskbar() {
  NS_IF_RELEASE(mTaskbar);
  ::CoUninitialize();
}

NS_IMETHODIMP
WinTaskbar::GetAvailable(PRBool *aAvailable) {
  *aAvailable = mTaskbar != nsnull;
  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::CreateTaskbarTabPreview(nsIDocShell *shell, nsITaskbarPreviewController *controller, nsITaskbarTabPreview **_retval) {
  if (!mTaskbar)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(shell);
  NS_ENSURE_ARG_POINTER(controller);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  nsRefPtr<TaskbarTabPreview> preview(new TaskbarTabPreview(mTaskbar, controller, toplevelHWND, shell));
  if (!preview)
    return NS_ERROR_OUT_OF_MEMORY;

  preview.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetTaskbarWindowPreview(nsIDocShell *shell, nsITaskbarWindowPreview **_retval) {
  if (!mTaskbar)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(shell);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  nsWindow *window = nsWindow::GetNSWindowPtr(toplevelHWND);

  if (!window)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITaskbarWindowPreview> preview = window->GetTaskbarPreview();
  if (!preview) {
    nsRefPtr<DefaultController> defaultController = new DefaultController(toplevelHWND);
    preview = new TaskbarWindowPreview(mTaskbar, defaultController, toplevelHWND, shell);
    if (!preview)
      return NS_ERROR_OUT_OF_MEMORY;
    window->SetTaskbarPreview(preview);
  }

  preview.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetTaskbarProgress(nsIDocShell *shell, nsITaskbarProgress **_retval) {
  nsCOMPtr<nsITaskbarWindowPreview> preview;
  nsresult rv = GetTaskbarWindowPreview(shell, getter_AddRefs(preview));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(preview, _retval);
}

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
