/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsXULWindow.h"

// Helper Classes
#include "nsString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIServiceManager.h"
#include "nsIDocShellTreeItem.h"

// Interfaces needed to include
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULElement.h"
#include "nsIXULBrowserWindow.h"
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

nsChromeTreeOwner::nsChromeTreeOwner() : mXULWindow(nullptr)
{
}

nsChromeTreeOwner::~nsChromeTreeOwner()
{
}

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

NS_IMETHODIMP nsChromeTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);

  if(aIID.Equals(NS_GET_IID(nsIPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if(aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if(aIID.Equals(NS_GET_IID(nsIWebBrowserChrome))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIXULWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->QueryInterface(aIID, aSink);
  }

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                     bool aPrimary)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellAdded(aContentShell, aPrimary);
}

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellRemoved(aContentShell);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP
nsChromeTreeOwner::TabParentAdded(nsITabParent* aTab, bool aPrimary)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->TabParentAdded(aTab, aPrimary);
}

NS_IMETHODIMP
nsChromeTreeOwner::TabParentRemoved(nsITabParent* aTab)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->TabParentRemoved(aTab);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPrimaryTabParent(nsITabParent** aTab)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPrimaryTabParent(aTab);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPrimaryContentSize(int32_t* aWidth,
                                         int32_t* aHeight)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetPrimaryContentSize(int32_t aWidth,
                                         int32_t aHeight)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::GetRootShellSize(int32_t* aWidth,
                                    int32_t* aHeight)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetRootShellSize(int32_t aWidth,
                                    int32_t aHeight)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP nsChromeTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   int32_t aCX, int32_t aCY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetPersistence(bool aPersistPosition,
                                  bool aPersistSize,
                                  bool aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(kPersist, persistString);

  bool saveString = false;
  int32_t index;

#define FIND_PERSIST_STRING(aString, aCond)            \
  index = persistString.Find(aString);                 \
  if (!aCond && index > kNotFound) {                   \
    persistString.Cut(index, aString.Length());        \
    saveString = true;                              \
  } else if (aCond && index == kNotFound) {            \
    persistString.Append(kSpace + aString); \
    saveString = true;                              \
  }
  FIND_PERSIST_STRING(kScreenX,  aPersistPosition);
  FIND_PERSIST_STRING(kScreenY,  aPersistPosition);
  FIND_PERSIST_STRING(kWidth,    aPersistSize);
  FIND_PERSIST_STRING(kHeight,   aPersistSize);
  FIND_PERSIST_STRING(kSizemode, aPersistSizeMode);

  ErrorResult rv;
  if (saveString) {
    docShellElement->SetAttribute(kPersist, persistString, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPersistence(bool* aPersistPosition,
                                  bool* aPersistSize,
                                  bool* aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement)
    return NS_ERROR_FAILURE;

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
nsChromeTreeOwner::GetTabCount(uint32_t* aResult)
{
  if (mXULWindow) {
    return mXULWindow->GetTabCount(aResult);
  }

  *aResult = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetHasPrimaryContent(bool* aResult)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetHasPrimaryContent(aResult);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsChromeTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
   // Ignore widget parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, 0), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Create()
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsChromeTreeOwner::Destroy()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Destroy();
}

NS_IMETHODIMP nsChromeTreeOwner::GetUnscaledDevicePixelsPerCSSPixel(double *aScale)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetUnscaledDevicePixelsPerCSSPixel(aScale);
}

NS_IMETHODIMP nsChromeTreeOwner::GetDevicePixelsPerDesktopPixel(double *aScale)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetDevicePixelsPerDesktopPixel(aScale);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionDesktopPix(int32_t x, int32_t y)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPositionDesktopPix(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPosition(int32_t x, int32_t y)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPosition(int32_t* x, int32_t* y)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetSize(int32_t cx, int32_t cy, bool fRepaint)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetSize(int32_t* cx, int32_t* cy)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetSize(cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionAndSize(int32_t x, int32_t y, int32_t cx,
   int32_t cy, uint32_t aFlags)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPositionAndSize(x, y, cx, cy, aFlags);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPositionAndSize(int32_t* x, int32_t* y, int32_t* cx,
   int32_t* cy)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::Repaint(bool aForce)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetNativeHandle(nsAString& aNativeHandle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetNativeHandle(aNativeHandle);
}

NS_IMETHODIMP nsChromeTreeOwner::GetVisibility(bool* aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::SetVisibility(bool aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::GetEnabled(bool *aEnabled)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsChromeTreeOwner::SetEnabled(bool aEnable)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsChromeTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);
   NS_ENSURE_STATE(mXULWindow);

   *aMainWidget = mXULWindow->mWindow;
   NS_IF_ADDREF(*aMainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetFocus()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsChromeTreeOwner::GetTitle(char16_t** aTitle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsChromeTreeOwner::SetTitle(const char16_t* aTitle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetTitle(aTitle);
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
                                    int32_t aMaxTotalProgress)
{
   return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 uint32_t aProgressStateFlags,
                                 nsresult aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  nsIURI* aLocation,
                                                  uint32_t aFlags)
{
  bool itsForYou = true;

  if (aWebProgress) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<mozIDOMWindowProxy> progressWin;
    aWebProgress->GetDOMWindow(getter_AddRefs(progressWin));

    nsCOMPtr<nsIDocShell> docshell;
    mXULWindow->GetDocShell(getter_AddRefs(docshell));
    // XXXkhuey this is totally wrong, bug 1223303.
    nsCOMPtr<mozIDOMWindowProxy> ourWin(do_QueryInterface(docshell));

    if (ourWin != progressWin)
      itsForYou = false;
  }

   // If loading a new root .xul document, then redo chrome.
  if (itsForYou) {
    NS_ENSURE_STATE(mXULWindow);
    mXULWindow->mChromeLoaded = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const char16_t* aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP
nsChromeTreeOwner::OnSecurityChange(nsIWebProgress *aWebProgress,
                                    nsIRequest *aRequest,
                                    uint32_t state)
{
    return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner: Helpers
//*****************************************************************************

//*****************************************************************************
// nsChromeTreeOwner: Accessors
//*****************************************************************************

void nsChromeTreeOwner::XULWindow(nsXULWindow* aXULWindow)
{
   mXULWindow = aXULWindow;
}

nsXULWindow* nsChromeTreeOwner::XULWindow()
{
   return mXULWindow;
}
