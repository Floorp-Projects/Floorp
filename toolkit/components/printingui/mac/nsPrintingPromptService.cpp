/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"

#include "nsIPrintingPromptService.h"
#include "nsIFactory.h"
#include "nsIPrintDialogService.h"
#include "nsPIDOMWindow.h"

//*****************************************************************************
// nsPrintingPromptService
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService()
{
}

nsPrintingPromptService::~nsPrintingPromptService() = default;

nsresult nsPrintingPromptService::Init()
{
    return NS_OK;
}

//*****************************************************************************
// nsPrintingPromptService::nsIPrintingPromptService
//*****************************************************************************

NS_IMETHODIMP
nsPrintingPromptService::ShowPrintDialog(mozIDOMWindowProxy *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
  nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                           NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint) {
    return dlgPrint->Show(nsPIDOMWindowOuter::From(parent), printSettings,
                          webBrowserPrint);
  }

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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPageSetup(mozIDOMWindowProxy *parent, nsIPrintSettings *printSettings, nsIObserver *aObs)
{
  nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                           NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint) {
    return dlgPrint->ShowPageSetup(nsPIDOMWindowOuter::From(parent), printSettings);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPrintingPromptService::ShowPrinterProperties(mozIDOMWindowProxy *parent, const char16_t *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// nsPrintingPromptService::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t aStateFlags, nsresult aStatus)
{
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, int32_t aCurSelfProgress, int32_t aMaxSelfProgress, int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location, in unsigned long aFlags); */
NS_IMETHODIMP
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const char16_t *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t state)
{
    return NS_OK;
}
