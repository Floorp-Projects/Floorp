/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingProxy.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/Unused.h"
#include "nsIDocShell.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintSession.h"
#include "nsPIDOMWindow.h"
#include "nsPrintSettingsService.h"
#include "nsServiceManagerUtils.h"
#include "PrintProgressDialogChild.h"
#include "PrintSettingsDialogChild.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::embedding;
using namespace mozilla::layout;

static StaticRefPtr<nsPrintingProxy> sPrintingProxyInstance;

NS_IMPL_ISUPPORTS(nsPrintingProxy, nsIPrintingPromptService)

nsPrintingProxy::nsPrintingProxy() = default;

nsPrintingProxy::~nsPrintingProxy() = default;

/* static */
already_AddRefed<nsPrintingProxy> nsPrintingProxy::GetInstance() {
  if (!sPrintingProxyInstance) {
    sPrintingProxyInstance = new nsPrintingProxy();
    if (!sPrintingProxyInstance) {
      return nullptr;
    }
    nsresult rv = sPrintingProxyInstance->Init();
    if (NS_FAILED(rv)) {
      sPrintingProxyInstance = nullptr;
      return nullptr;
    }
    ClearOnShutdown(&sPrintingProxyInstance);
  }

  RefPtr<nsPrintingProxy> inst = sPrintingProxyInstance.get();
  return inst.forget();
}

nsresult nsPrintingProxy::Init() {
  mozilla::Unused << ContentChild::GetSingleton()->SendPPrintingConstructor(
      this);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPrintDialog(mozIDOMWindowProxy* parent,
                                 nsIPrintSettings* printSettings) {
  NS_ENSURE_ARG(printSettings);

  // If parent is null we are just being called to retrieve the print settings
  // from the printer in the parent for print preview.
  BrowserChild* pBrowser = nullptr;
  if (parent) {
    // Get the BrowserChild for this nsIDOMWindow, which we can then pass up to
    // the parent.
    nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(parent);
    NS_ENSURE_STATE(pwin);
    nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
    NS_ENSURE_STATE(docShell);

    nsCOMPtr<nsIBrowserChild> tabchild = docShell->GetBrowserChild();
    NS_ENSURE_STATE(tabchild);

    pBrowser = static_cast<BrowserChild*>(tabchild.get());
  }

  // Next, serialize the nsIWebBrowserPrint and nsIPrintSettings we were given.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PrintData inSettings;
  rv = printSettingsSvc->SerializeToPrintData(printSettings, &inSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, the waiting game. The parent process should be showing
  // the printing dialog soon. In the meantime, we need to spin a
  // nested event loop while we wait for the results of the dialog
  // to be returned to us.

  RefPtr<PrintSettingsDialogChild> dialog = new PrintSettingsDialogChild();
  SendPPrintSettingsDialogConstructor(dialog);

  mozilla::Unused << SendShowPrintDialog(dialog, pBrowser, inSettings);

  SpinEventLoopUntil([&, dialog]() { return dialog->returned(); });

  rv = dialog->result();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = printSettingsSvc->DeserializeToPrintSettings(dialog->data(),
                                                    printSettings);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPrintProgressDialog(
    mozIDOMWindowProxy* parent,
    nsIPrintSettings* printSettings,  // ok to be null
    nsIObserver* openDialogObserver,  // ok to be null
    bool isForPrinting, nsIWebProgressListener** webProgressListener,
    nsIPrintProgressParams** printProgressParams, bool* notifyOnOpen) {
  NS_ENSURE_ARG(parent);
  NS_ENSURE_ARG(webProgressListener);
  NS_ENSURE_ARG(printProgressParams);
  NS_ENSURE_ARG(notifyOnOpen);

  // Get the BrowserChild for this nsIDOMWindow, which we can then pass up to
  // the parent.
  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(parent);
  NS_ENSURE_STATE(pwin);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  NS_ENSURE_STATE(docShell);
  nsCOMPtr<nsIBrowserChild> tabchild = docShell->GetBrowserChild();
  BrowserChild* pBrowser = static_cast<BrowserChild*>(tabchild.get());

  RefPtr<PrintProgressDialogChild> dialogChild =
      new PrintProgressDialogChild(openDialogObserver, printSettings);

  SendPPrintProgressDialogConstructor(dialogChild);

  // Get the RemotePrintJob if we have one available.
  RefPtr<RemotePrintJobChild> remotePrintJob;
  if (printSettings) {
    nsCOMPtr<nsIPrintSession> printSession;
    nsresult rv = printSettings->GetPrintSession(getter_AddRefs(printSession));
    if (NS_SUCCEEDED(rv) && printSession) {
      remotePrintJob = printSession->GetRemotePrintJob();
    }
  }

  // NOTE: We set notifyOnOpen to true unconditionally. If the parent process
  // would get `false` for notifyOnOpen, then it will synthesize a notification
  // which will be sent asynchronously down to the child.
  *notifyOnOpen = true;
  mozilla::Unused << SendShowProgress(pBrowser, dialogChild, remotePrintJob,
                                      isForPrinting);

  // If we have a RemotePrintJob that will be being used as a more general
  // forwarder for print progress listeners. Once we always have one we can
  // remove the interface from PrintProgressDialogChild.
  if (!remotePrintJob) {
    NS_ADDREF(*webProgressListener = dialogChild);
  }
  NS_ADDREF(*printProgressParams = dialogChild);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPageSetupDialog(mozIDOMWindowProxy* parent,
                                     nsIPrintSettings* printSettings) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsPrintingProxy::SavePrintSettings(nsIPrintSettings* aPS,
                                            bool aUsePrinterNamePrefix,
                                            uint32_t aFlags) {
  nsresult rv;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PrintData settings;
  rv = printSettingsSvc->SerializeToPrintData(aPS, &settings);
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << SendSavePrintSettings(settings, aUsePrinterNamePrefix, aFlags, &rv);
  return rv;
}

PPrintProgressDialogChild* nsPrintingProxy::AllocPPrintProgressDialogChild() {
  // The parent process will never initiate the PPrintProgressDialog
  // protocol connection, so no need to provide an allocator here.
  MOZ_ASSERT_UNREACHABLE(
      "Allocator for PPrintProgressDialogChild should not "
      "be called on nsPrintingProxy.");
  return nullptr;
}

bool nsPrintingProxy::DeallocPPrintProgressDialogChild(
    PPrintProgressDialogChild* aActor) {
  // The PrintProgressDialogChild implements refcounting, and
  // will take itself out.
  return true;
}

PPrintSettingsDialogChild* nsPrintingProxy::AllocPPrintSettingsDialogChild() {
  // The parent process will never initiate the PPrintSettingsDialog
  // protocol connection, so no need to provide an allocator here.
  MOZ_ASSERT_UNREACHABLE(
      "Allocator for PPrintSettingsDialogChild should not "
      "be called on nsPrintingProxy.");
  return nullptr;
}

bool nsPrintingProxy::DeallocPPrintSettingsDialogChild(
    PPrintSettingsDialogChild* aActor) {
  // The PrintSettingsDialogChild implements refcounting, and
  // will take itself out.
  return true;
}

PRemotePrintJobChild* nsPrintingProxy::AllocPRemotePrintJobChild() {
  RefPtr<RemotePrintJobChild> remotePrintJob = new RemotePrintJobChild();
  return remotePrintJob.forget().take();
}

bool nsPrintingProxy::DeallocPRemotePrintJobChild(
    PRemotePrintJobChild* aDoomed) {
  RemotePrintJobChild* remotePrintJob =
      static_cast<RemotePrintJobChild*>(aDoomed);
  NS_RELEASE(remotePrintJob);
  return true;
}
