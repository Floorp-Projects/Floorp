/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIPrintDialogService.h"

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"

static const char *kPrintProgressDialogURL = "chrome://global/content/printProgress.xul";
static const char *kPrtPrvProgressDialogURL = "chrome://global/content/printPreviewProgress.xul";

NS_IMPL_ISUPPORTS(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService()
{
}

nsPrintingPromptService::~nsPrintingPromptService()
{
}

nsresult
nsPrintingPromptService::Init()
{
    nsresult rv;
    mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    return rv;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPrintDialog(mozIDOMWindowProxy *parent,
                                         nsIWebBrowserPrint *webBrowserPrint,
                                         nsIPrintSettings *printSettings)
{
    NS_ENSURE_ARG(webBrowserPrint);
    NS_ENSURE_ARG(printSettings);

    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->Show(nsPIDOMWindowOuter::From(parent),
                            printSettings, webBrowserPrint);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowProgress(mozIDOMWindowProxy*      parent,
                                      nsIWebBrowserPrint*      webBrowserPrint,    // ok to be null
                                      nsIPrintSettings*        printSettings,      // ok to be null
                                      nsIObserver*             openDialogObserver, // ok to be null
                                      bool                     isForPrinting,
                                      nsIWebProgressListener** webProgressListener,
                                      nsIPrintProgressParams** printProgressParams,
                                      bool*                  notifyOnOpen)
{
    NS_ENSURE_ARG(webProgressListener);
    NS_ENSURE_ARG(printProgressParams);
    NS_ENSURE_ARG(notifyOnOpen);

    *notifyOnOpen = false;

    nsPrintProgress* prtProgress = new nsPrintProgress(printSettings);
    mPrintProgress = prtProgress;
    mWebProgressListener = prtProgress;

    nsCOMPtr<nsIPrintProgressParams> prtProgressParams = new nsPrintProgressParams();

    nsCOMPtr<mozIDOMWindowProxy> parentWindow = parent;

    if (mWatcher && !parentWindow) {
        mWatcher->GetActiveWindow(getter_AddRefs(parentWindow));
    }

    if (parentWindow) {
        mPrintProgress->OpenProgressDialog(parentWindow,
                                           isForPrinting ? kPrintProgressDialogURL : kPrtPrvProgressDialogURL,
                                           prtProgressParams, openDialogObserver, notifyOnOpen);
    }

    prtProgressParams.forget(printProgressParams);
    NS_ADDREF(*webProgressListener = this);

    return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPageSetup(mozIDOMWindowProxy *parent,
                                       nsIPrintSettings *printSettings,
                                       nsIObserver *aObs)
{
    NS_ENSURE_ARG(printSettings);

    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->ShowPageSetup(nsPIDOMWindowOuter::From(parent),
                                     printSettings);

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPrinterProperties(mozIDOMWindowProxy *parent,
                                               const char16_t *printerName,
                                               nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;

}

NS_IMETHODIMP
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t aStateFlags, nsresult aStatus)
{
  if ((aStateFlags & STATE_STOP) && mWebProgressListener) {
    mWebProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    if (mPrintProgress) {
      mPrintProgress->CloseProgressDialog(true);
    }
    mPrintProgress       = nullptr;
    mWebProgressListener = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, int32_t aCurSelfProgress, int32_t aMaxSelfProgress, int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location, aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const char16_t *aMessage)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t state)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
  }
  return NS_OK;
}
