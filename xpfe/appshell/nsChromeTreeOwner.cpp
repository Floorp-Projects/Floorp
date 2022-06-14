/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsChromeTreeOwner.h"
#include "AppWindow.h"

// Helper Classes
#include "nsString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIDocShellTreeItem.h"

// Interfaces needed to include
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIWebProgress.h"
#include "nsIWidget.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

//*****************************************************************************
// nsChromeTreeOwner string literals
//*****************************************************************************

const nsLiteralString kPersist(u"persist");
const nsLiteralString kScreenX(u"screenX");
const nsLiteralString kScreenY(u"screenY");
const nsLiteralString kWidth(u"width");
const nsLiteralString kHeight(u"height");
const nsLiteralString kSizemode(u"sizemode");
const nsLiteralString kSpace(u" ");

//*****************************************************************************
//***    nsChromeTreeOwner: Object Management
//*****************************************************************************

nsChromeTreeOwner::nsChromeTreeOwner() : mAppWindow(nullptr) {}

nsChromeTreeOwner::~nsChromeTreeOwner() {}

//*****************************************************************************
// nsChromeTreeOwner::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsChromeTreeOwner)
NS_IMPL_RELEASE(nsChromeTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsChromeTreeOwner)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
  NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsChromeTreeOwner::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP nsChromeTreeOwner::GetInterface(const nsIID& aIID, void** aSink) {
  NS_ENSURE_ARG_POINTER(aSink);

  if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
    NS_ENSURE_STATE(mAppWindow);
    return mAppWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_STATE(mAppWindow);
    return mAppWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome))) {
    NS_ENSURE_STATE(mAppWindow);
    return mAppWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow))) {
    NS_ENSURE_STATE(mAppWindow);
    return mAppWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIAppWindow))) {
    NS_ENSURE_STATE(mAppWindow);
    return mAppWindow->QueryInterface(aIID, aSink);
  }

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                     bool aPrimary) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->ContentShellAdded(aContentShell, aPrimary);
}

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->ContentShellRemoved(aContentShell);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPrimaryContentShell(
    nsIDocShellTreeItem** aShell) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP
nsChromeTreeOwner::RemoteTabAdded(nsIRemoteTab* aTab, bool aPrimary) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->RemoteTabAdded(aTab, aPrimary);
}

NS_IMETHODIMP
nsChromeTreeOwner::RemoteTabRemoved(nsIRemoteTab* aTab) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->RemoteTabRemoved(aTab);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPrimaryRemoteTab(nsIRemoteTab** aTab) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPrimaryRemoteTab(aTab);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPrimaryContentBrowsingContext(
    mozilla::dom::BrowsingContext** aBc) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPrimaryContentBrowsingContext(aBc);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPrimaryContentSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetPrimaryContentSize(int32_t aWidth, int32_t aHeight) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetRootShellSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetRootShellSize(int32_t aWidth, int32_t aHeight) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP nsChromeTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
                                             int32_t aCX, int32_t aCY) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetPersistence(bool aPersistPosition, bool aPersistSize,
                                  bool aPersistSizeMode) {
  NS_ENSURE_STATE(mAppWindow);
  nsCOMPtr<dom::Element> docShellElement = mAppWindow->GetWindowDOMElement();
  if (!docShellElement) return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(kPersist, persistString);

  bool saveString = false;
  int32_t index;

#define FIND_PERSIST_STRING(aString, aCond)     \
  index = persistString.Find(aString);          \
  if (!aCond && index > kNotFound) {            \
    persistString.Cut(index, aString.Length()); \
    saveString = true;                          \
  } else if (aCond && index == kNotFound) {     \
    persistString.Append(kSpace + aString);     \
    saveString = true;                          \
  }
  FIND_PERSIST_STRING(kScreenX, aPersistPosition);
  FIND_PERSIST_STRING(kScreenY, aPersistPosition);
  FIND_PERSIST_STRING(kWidth, aPersistSize);
  FIND_PERSIST_STRING(kHeight, aPersistSize);
  FIND_PERSIST_STRING(kSizemode, aPersistSizeMode);

  ErrorResult rv;
  if (saveString) {
    docShellElement->SetAttribute(kPersist, persistString, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPersistence(bool* aPersistPosition, bool* aPersistSize,
                                  bool* aPersistSizeMode) {
  NS_ENSURE_STATE(mAppWindow);
  nsCOMPtr<dom::Element> docShellElement = mAppWindow->GetWindowDOMElement();
  if (!docShellElement) return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(kPersist, persistString);

  // data structure doesn't quite match the question, but it's close enough
  // for what we want (since this method is never actually called...)
  if (aPersistPosition)
    *aPersistPosition = persistString.Find(kScreenX) > kNotFound ||
                        persistString.Find(kScreenY) > kNotFound;
  if (aPersistSize)
    *aPersistSize = persistString.Find(kWidth) > kNotFound ||
                    persistString.Find(kHeight) > kNotFound;
  if (aPersistSizeMode)
    *aPersistSizeMode = persistString.Find(kSizemode) > kNotFound;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetTabCount(uint32_t* aResult) {
  if (mAppWindow) {
    return mAppWindow->GetTabCount(aResult);
  }

  *aResult = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetHasPrimaryContent(bool* aResult) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetHasPrimaryContent(aResult);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsChromeTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
                                            nsIWidget* parentWidget, int32_t x,
                                            int32_t y, int32_t cx, int32_t cy) {
  // Ignore widget parents for now.  Don't think those are a vaild thing to
  // call.
  NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, 0), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Destroy() {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->Destroy();
}

double nsChromeTreeOwner::GetWidgetCSSToDeviceScale() {
  return mAppWindow ? mAppWindow->GetWidgetCSSToDeviceScale() : 1.0;
}

NS_IMETHODIMP nsChromeTreeOwner::GetDevicePixelsPerDesktopPixel(
    double* aScale) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetDevicePixelsPerDesktopPixel(aScale);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionDesktopPix(int32_t x, int32_t y) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetPositionDesktopPix(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPosition(int32_t x, int32_t y) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPosition(int32_t* x, int32_t* y) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetSize(int32_t cx, int32_t cy,
                                         bool fRepaint) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetSize(int32_t* cx, int32_t* cy) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetSize(cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionAndSize(int32_t x, int32_t y,
                                                    int32_t cx, int32_t cy,
                                                    uint32_t aFlags) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetPositionAndSize(x, y, cx, cy, aFlags);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPositionAndSize(int32_t* x, int32_t* y,
                                                    int32_t* cx, int32_t* cy) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::Repaint(bool aForce) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->Repaint(aForce);
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentWidget(nsIWidget** aParentWidget) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentWidget(nsIWidget* aParentWidget) {
  NS_ASSERTION(false, "You can't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentNativeWindow(
    nativeWindow* aParentNativeWindow) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentNativeWindow(
    nativeWindow aParentNativeWindow) {
  NS_ASSERTION(false, "You can't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetNativeHandle(nsAString& aNativeHandle) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetNativeHandle(aNativeHandle);
}

NS_IMETHODIMP nsChromeTreeOwner::GetVisibility(bool* aVisibility) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::SetVisibility(bool aVisibility) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::GetEnabled(bool* aEnabled) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsChromeTreeOwner::SetEnabled(bool aEnable) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsChromeTreeOwner::GetMainWidget(nsIWidget** aMainWidget) {
  NS_ENSURE_ARG_POINTER(aMainWidget);
  NS_ENSURE_STATE(mAppWindow);

  *aMainWidget = mAppWindow->mWindow;
  NS_IF_ADDREF(*aMainWidget);

  return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::GetTitle(nsAString& aTitle) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsChromeTreeOwner::SetTitle(const nsAString& aTitle) {
  NS_ENSURE_STATE(mAppWindow);
  return mAppWindow->SetTitle(aTitle);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsChromeTreeOwner::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    int32_t aCurSelfProgress,
                                    int32_t aMaxSelfProgress,
                                    int32_t aCurTotalProgress,
                                    int32_t aMaxTotalProgress) {
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 uint32_t aProgressStateFlags,
                                 nsresult aStatus) {
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, nsIURI* aLocation,
                                    uint32_t aFlags) {
  NS_ENSURE_STATE(mAppWindow);

  // If loading a new root .xul document, then redo chrome.
  if (aWebProgress) {
    nsCOMPtr<nsIDocShell> docshell;
    mAppWindow->GetDocShell(getter_AddRefs(docshell));

    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(docshell));
    if (webProgress != aWebProgress) {
      return NS_OK;
    }
  }

  mAppWindow->mChromeLoaded = false;
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest, nsresult aStatus,
                                  const char16_t* aMessage) {
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, uint32_t aState) {
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aEvent) {
  return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner: Helpers
//*****************************************************************************

//*****************************************************************************
// nsChromeTreeOwner: Accessors
//*****************************************************************************

void nsChromeTreeOwner::AppWindow(mozilla::AppWindow* aAppWindow) {
  mAppWindow = aAppWindow;
}

mozilla::AppWindow* nsChromeTreeOwner::AppWindow() { return mAppWindow; }
