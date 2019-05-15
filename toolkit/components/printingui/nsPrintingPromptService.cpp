/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIPrintDialogService.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"

// Printing Progress Includes
#if !defined(XP_MACOSX)
#  include "nsPrintProgress.h"
#  include "nsPrintProgressParams.h"

static const char* kPrintProgressDialogURL =
    "chrome://global/content/printProgress.xul";
static const char* kPrtPrvProgressDialogURL =
    "chrome://global/content/printPreviewProgress.xul";
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsPrintingPromptService, nsIPrintingPromptService,
                  nsIWebProgressListener)

StaticRefPtr<nsPrintingPromptService> sSingleton;

/* static */
already_AddRefed<nsPrintingPromptService>
nsPrintingPromptService::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "The content process must use nsPrintingProxy");

  if (!sSingleton) {
    sSingleton = new nsPrintingPromptService();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

nsPrintingPromptService::nsPrintingPromptService() = default;

nsPrintingPromptService::~nsPrintingPromptService() = default;

nsresult nsPrintingPromptService::Init() {
#if !defined(XP_MACOSX)
  nsresult rv;
  mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  return rv;
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPrintDialog(mozIDOMWindowProxy* parent,
                                         nsIWebBrowserPrint* webBrowserPrint,
                                         nsIPrintSettings* printSettings) {
  NS_ENSURE_ARG(webBrowserPrint);
  NS_ENSURE_ARG(printSettings);

  nsCOMPtr<nsIPrintDialogService> dlgPrint(
      do_GetService(NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint)
    return dlgPrint->Show(nsPIDOMWindowOuter::From(parent), printSettings,
                          webBrowserPrint);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPrintProgressDialog(
    mozIDOMWindowProxy* parent,
    nsIWebBrowserPrint* webBrowserPrint,  // ok to be null
    nsIPrintSettings* printSettings,      // ok to be null
    nsIObserver* openDialogObserver,      // ok to be null
    bool isForPrinting, nsIWebProgressListener** webProgressListener,
    nsIPrintProgressParams** printProgressParams, bool* notifyOnOpen) {
#if !defined(XP_MACOSX)
  NS_ENSURE_ARG(webProgressListener);
  NS_ENSURE_ARG(printProgressParams);
  NS_ENSURE_ARG(notifyOnOpen);

  *notifyOnOpen = false;
  if (mPrintProgress) {
    *webProgressListener = nullptr;
    *printProgressParams = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsPrintProgress* prtProgress = new nsPrintProgress(printSettings);
  mPrintProgress = prtProgress;
  mWebProgressListener = prtProgress;

  nsCOMPtr<nsIPrintProgressParams> prtProgressParams =
      new nsPrintProgressParams();

  nsCOMPtr<mozIDOMWindowProxy> parentWindow = parent;

  if (mWatcher && !parentWindow) {
    mWatcher->GetActiveWindow(getter_AddRefs(parentWindow));
  }

  if (parentWindow) {
    mPrintProgress->OpenProgressDialog(
        parentWindow,
        isForPrinting ? kPrintProgressDialogURL : kPrtPrvProgressDialogURL,
        prtProgressParams, openDialogObserver, notifyOnOpen);
  }

  prtProgressParams.forget(printProgressParams);
  NS_ADDREF(*webProgressListener = this);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPageSetupDialog(mozIDOMWindowProxy* parent,
                                             nsIPrintSettings* printSettings) {
  NS_ENSURE_ARG(printSettings);

  nsCOMPtr<nsIPrintDialogService> dlgPrint(
      do_GetService(NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint)
    return dlgPrint->ShowPageSetup(nsPIDOMWindowOuter::From(parent),
                                   printSettings);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPrintingPromptService::OnStateChange(nsIWebProgress* aWebProgress,
                                       nsIRequest* aRequest,
                                       uint32_t aStateFlags, nsresult aStatus) {
#if !defined(XP_MACOSX)
  if ((aStateFlags & STATE_STOP) && mWebProgressListener) {
    mWebProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags,
                                        aStatus);
    if (mPrintProgress) {
      mPrintProgress->CloseProgressDialog(true);
    }
    mPrintProgress = nullptr;
    mWebProgressListener = nullptr;
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnProgressChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          int32_t aCurSelfProgress,
                                          int32_t aMaxSelfProgress,
                                          int32_t aCurTotalProgress,
                                          int32_t aMaxTotalProgress) {
#if !defined(XP_MACOSX)
  if (mWebProgressListener) {
    return mWebProgressListener->OnProgressChange(
        aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
        aCurTotalProgress, aMaxTotalProgress);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnLocationChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          nsIURI* location, uint32_t aFlags) {
#if !defined(XP_MACOSX)
  if (mWebProgressListener) {
    return mWebProgressListener->OnLocationChange(aWebProgress, aRequest,
                                                  location, aFlags);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnStatusChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest, nsresult aStatus,
                                        const char16_t* aMessage) {
#if !defined(XP_MACOSX)
  if (mWebProgressListener) {
    return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus,
                                                aMessage);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnSecurityChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aState) {
#if !defined(XP_MACOSX)
  if (mWebProgressListener) {
    return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest,
                                                  aState);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                                nsIRequest* aRequest,
                                                uint32_t aEvent) {
#if !defined(XP_MACOSX)
  if (mWebProgressListener) {
    return mWebProgressListener->OnContentBlockingEvent(aWebProgress, aRequest,
                                                        aEvent);
  }
#endif
  return NS_OK;
}
