/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIPrintDialogService.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsPrintingPromptService, nsIPrintingPromptService)

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
                                         nsIPrintSettings* printSettings) {
  NS_ENSURE_ARG(printSettings);

  nsCOMPtr<nsIPrintDialogService> dlgPrint(
      do_GetService(NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint)
    return dlgPrint->Show(nsPIDOMWindowOuter::From(parent), printSettings);

  return NS_ERROR_FAILURE;
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
