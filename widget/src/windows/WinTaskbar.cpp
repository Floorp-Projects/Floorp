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
#include "nsIXULAppInfo.h"
#include "nsIJumpListBuilder.h"
#include "nsUXThemeData.h"
#include "nsWindow.h"
#include "TaskbarTabPreview.h"
#include "TaskbarWindowPreview.h"
#include "JumpListBuilder.h"
#include "nsWidgetsCID.h"
#include "nsPIDOMWindow.h"
#include <io.h>
#include <propvarutil.h>
#include <propkey.h>
#include <shellapi.h>

const PRUnichar kShellLibraryName[] =  L"shell32.dll";

static NS_DEFINE_CID(kJumpListBuilderCID, NS_WIN_JUMPLISTBUILDER_CID);

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

HWND
GetHWNDFromDOMWindow(nsIDOMWindow *dw) {
  nsCOMPtr<nsIWidget> widget;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(dw);
  if (!window) 
    return NULL;

  return GetHWNDFromDocShell(window->GetDocShell());
}

nsresult
SetWindowAppUserModelProp(nsIDOMWindow *aParent,
                          const nsString & aIdentifier) {
  NS_ENSURE_ARG_POINTER(aParent);

  if (aIdentifier.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDOMWindow(aParent), GA_ROOT);

  if (!toplevelHWND)
    return NS_ERROR_INVALID_ARG;

  typedef HRESULT (WINAPI * SHGetPropertyStoreForWindowPtr)
                    (HWND hwnd, REFIID riid, void** ppv);
  SHGetPropertyStoreForWindowPtr funcGetProStore = nsnull;

  HMODULE hDLL = ::LoadLibraryW(kShellLibraryName);
  funcGetProStore = (SHGetPropertyStoreForWindowPtr)
    GetProcAddress(hDLL, "SHGetPropertyStoreForWindow");

  if (!funcGetProStore) {
    FreeLibrary(hDLL);
    return NS_ERROR_NO_INTERFACE;
  }

  IPropertyStore* pPropStore;
  if (FAILED(funcGetProStore(toplevelHWND,
                             IID_PPV_ARGS(&pPropStore)))) {
    FreeLibrary(hDLL);
    return NS_ERROR_INVALID_ARG;
  }

  PROPVARIANT pv;
  if (FAILED(InitPropVariantFromString(aIdentifier.get(), &pv))) {
    pPropStore->Release();
    FreeLibrary(hDLL);
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;
  if (FAILED(pPropStore->SetValue(PKEY_AppUserModel_ID, pv)) ||
      FAILED(pPropStore->Commit())) {
    rv = NS_ERROR_FAILURE;
  }

  PropVariantClear(&pv);
  pPropStore->Release();
  FreeLibrary(hDLL);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// default nsITaskbarPreviewController

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
DefaultController::DrawPreview(nsIDOMCanvasRenderingContext2D *ctx, bool *rDrawFrame) {
  *rDrawFrame = true;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::DrawThumbnail(nsIDOMCanvasRenderingContext2D *ctx, PRUint32 width, PRUint32 height, bool *rDrawFrame) {
  *rDrawFrame = false;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnClose(void) {
  NS_NOTREACHED("OnClose should not be called for TaskbarWindowPreviews");
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnActivate(bool *rAcceptActivation) {
  *rAcceptActivation = true;
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

///////////////////////////////////////////////////////////////////////////////
// nsIWinTaskbar

NS_IMPL_THREADSAFE_ISUPPORTS1(WinTaskbar, nsIWinTaskbar)

bool
WinTaskbar::Initialize() {
  if (mTaskbar)
    return true;

  ::CoInitialize(NULL);
  HRESULT hr = ::CoCreateInstance(CLSID_TaskbarList,
                                  NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ITaskbarList4,
                                  (void**)&mTaskbar);
  if (FAILED(hr))
    return false;

  hr = mTaskbar->HrInit();
  if (FAILED(hr)) {
    NS_WARNING("Unable to initialize taskbar");
    NS_RELEASE(mTaskbar);
    return false;
  }
  return true;
}

WinTaskbar::WinTaskbar() 
  : mTaskbar(nsnull) {
}

WinTaskbar::~WinTaskbar() {
  if (mTaskbar) { // match successful Initialize() call
    NS_RELEASE(mTaskbar);
    ::CoUninitialize();
  }
}

// static
bool
WinTaskbar::GetAppUserModelID(nsAString & aDefaultGroupId) {
  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1");
  if (!appInfo)
    return false;

  // The default, pulled from application.ini:
  // 'vendor.application.version'
  nsCString val;
  if (NS_SUCCEEDED(appInfo->GetVendor(val))) {
    AppendASCIItoUTF16(val, aDefaultGroupId);
    aDefaultGroupId.Append(PRUnichar('.'));
  }
  if (NS_SUCCEEDED(appInfo->GetName(val))) {
    AppendASCIItoUTF16(val, aDefaultGroupId);
    aDefaultGroupId.Append(PRUnichar('.'));
  }
  if (NS_SUCCEEDED(appInfo->GetVersion(val))) {
    AppendASCIItoUTF16(val, aDefaultGroupId);
  }

  if (aDefaultGroupId.IsEmpty())
    return false;

  // Differentiate 64-bit builds
#if defined(_WIN64)
  aDefaultGroupId.AppendLiteral(".Win64");
#endif

  return true;
}

/* readonly attribute AString defaultGroupId; */
NS_IMETHODIMP
WinTaskbar::GetDefaultGroupId(nsAString & aDefaultGroupId) {
  if (!GetAppUserModelID(aDefaultGroupId))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

// (static) Called from AppShell
bool
WinTaskbar::RegisterAppUserModelID() {
  if (nsWindow::GetWindowsVersion() < WIN7_VERSION)
    return false;

  SetCurrentProcessExplicitAppUserModelIDPtr funcAppUserModelID = nsnull;
  bool retVal = false;

  nsAutoString uid;
  if (!GetAppUserModelID(uid))
    return false;

  HMODULE hDLL = ::LoadLibraryW(kShellLibraryName);

  funcAppUserModelID = (SetCurrentProcessExplicitAppUserModelIDPtr)
                        GetProcAddress(hDLL, "SetCurrentProcessExplicitAppUserModelID");

  if (!funcAppUserModelID) {
    ::FreeLibrary(hDLL);
    return false;
  }

  if (SUCCEEDED(funcAppUserModelID(uid.get())))
    retVal = true;

  if (hDLL)
    ::FreeLibrary(hDLL);

  return retVal;
}

NS_IMETHODIMP
WinTaskbar::GetAvailable(bool *aAvailable) {
  *aAvailable = 
    nsWindow::GetWindowsVersion() < WIN7_VERSION ?
    false : true;

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::CreateTaskbarTabPreview(nsIDocShell *shell, nsITaskbarPreviewController *controller, nsITaskbarTabPreview **_retval) {
  if (!Initialize())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(shell);
  NS_ENSURE_ARG_POINTER(controller);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  if (!toplevelHWND)
    return NS_ERROR_INVALID_ARG;

  nsRefPtr<TaskbarTabPreview> preview(new TaskbarTabPreview(mTaskbar, controller, toplevelHWND, shell));
  if (!preview)
    return NS_ERROR_OUT_OF_MEMORY;

  preview.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetTaskbarWindowPreview(nsIDocShell *shell, nsITaskbarWindowPreview **_retval) {
  if (!Initialize())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(shell);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  if (!toplevelHWND)
    return NS_ERROR_INVALID_ARG;

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

NS_IMETHODIMP
WinTaskbar::GetOverlayIconController(nsIDocShell *shell,
                                     nsITaskbarOverlayIconController **_retval) {
  nsCOMPtr<nsITaskbarWindowPreview> preview;
  nsresult rv = GetTaskbarWindowPreview(shell, getter_AddRefs(preview));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(preview, _retval);
}

/* nsIJumpListBuilder createJumpListBuilder(); */
NS_IMETHODIMP
WinTaskbar::CreateJumpListBuilder(nsIJumpListBuilder * *aJumpListBuilder) {
  nsresult rv;

  if (JumpListBuilder::sBuildingList)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsCOMPtr<nsIJumpListBuilder> builder = 
    do_CreateInstance(kJumpListBuilderCID, &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_UNEXPECTED;

  NS_IF_ADDREF(*aJumpListBuilder = builder);

  return NS_OK;
}

/* void setGroupIdForWindow (in nsIDOMWindow aParent, in AString aIdentifier); */
NS_IMETHODIMP
WinTaskbar::SetGroupIdForWindow(nsIDOMWindow *aParent,
                                const nsAString & aIdentifier) {
  return SetWindowAppUserModelProp(aParent, nsString(aIdentifier));
}

/* void prepareFullScreen(in nsIDOMWindow aWindow, in boolean aFullScreen); */
NS_IMETHODIMP
WinTaskbar::PrepareFullScreen(nsIDOMWindow *aWindow, bool aFullScreen) {
  NS_ENSURE_ARG_POINTER(aWindow);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDOMWindow(aWindow), GA_ROOT);
  if (!toplevelHWND)
    return NS_ERROR_INVALID_ARG;
  
  return PrepareFullScreenHWND(toplevelHWND, aFullScreen);
}

/* void prepareFullScreen(in voidPtr aWindow, in boolean aFullScreen); */
NS_IMETHODIMP
WinTaskbar::PrepareFullScreenHWND(void *aHWND, bool aFullScreen) {
  if (!Initialize())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(aHWND);

  if (!::IsWindow((HWND)aHWND)) 
    return NS_ERROR_INVALID_ARG;

  HRESULT hr = mTaskbar->MarkFullscreenWindow((HWND)aHWND, aFullScreen);
  if (FAILED(hr)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

} // namespace widget
} // namespace mozilla

#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
