/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintSettingsService.h"
#include "nsServiceManagerUtils.h"
#include "PrintingParent.h"
#include "PrintSettingsDialogParent.h"
#include "mozilla/layout/RemotePrintJobParent.h"
#include "mozilla/StaticPrefs_print.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

namespace mozilla {
namespace embedding {
nsresult PrintingParent::ShowPrintDialog(PBrowserParent* aParent,
                                         const PrintData& aData,
                                         PrintData* aResult) {
  // If aParent is null this call is just being used to get print settings from
  // the printer for print preview.
  bool isPrintPreview = !aParent;
  nsCOMPtr<nsPIDOMWindowOuter> parentWin;
  if (aParent) {
    parentWin = DOMWindowFromBrowserParent(aParent);
    if (!parentWin) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIPrintingPromptService> pps(
      do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));
  if (!pps) {
    return NS_ERROR_FAILURE;
  }

  // Use the existing RemotePrintJob and its settings, if we have one, to make
  // sure they stay current.
  RemotePrintJobParent* remotePrintJob =
      static_cast<RemotePrintJobParent*>(aData.remotePrintJobParent());
  nsCOMPtr<nsIPrintSettings> settings;
  nsresult rv;
  if (remotePrintJob) {
    settings = remotePrintJob->GetPrintSettings();
  } else {
    rv = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(settings));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We only want to use the print silently setting from the parent.
  bool printSilently;
  rv = settings->GetPrintSilent(&printSilently);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrintSettingsSvc->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = settings->SetPrintSilent(printSilently);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString printerName;
  settings->GetPrinterName(printerName);
#ifdef MOZ_X11
  // Requesting the last-used printer name on Linux has been removed in the
  // child, because it was causing a sandbox violation (see Bug 1329216). If no
  // printer name is set at this point, use the print settings service to get
  // the last-used printer name, unless we're printing to file.
  bool printToFile = false;
  MOZ_ALWAYS_SUCCEEDS(settings->GetPrintToFile(&printToFile));
  if (!printToFile && printerName.IsEmpty()) {
    mPrintSettingsSvc->GetLastUsedPrinterName(printerName);
    settings->SetPrinterName(printerName);
  }
  mPrintSettingsSvc->InitPrintSettingsFromPrinter(printerName, settings);
#endif

  // If this is for print preview or we are printing silently then we just need
  // to initialize the print settings with anything specific from the printer.
  if (isPrintPreview || printSilently ||
      StaticPrefs::print_always_print_silent()) {
    settings->SetIsInitializedFromPrinter(false);
    mPrintSettingsSvc->InitPrintSettingsFromPrinter(printerName, settings);
  } else {
    rv = pps->ShowPrintDialog(parentWin, settings);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (isPrintPreview) {
    // For print preview we don't want a RemotePrintJob just the settings.
    rv = mPrintSettingsSvc->SerializeToPrintData(settings, aResult);
  } else {
    rv = SerializeAndEnsureRemotePrintJob(settings, nullptr, remotePrintJob,
                                          aResult);
  }

  return rv;
}

mozilla::ipc::IPCResult PrintingParent::RecvShowPrintDialog(
    PPrintSettingsDialogParent* aDialog, PBrowserParent* aParent,
    const PrintData& aData) {
  PrintData resultData;
  nsresult rv = ShowPrintDialog(aParent, aData, &resultData);

  // The child has been spinning an event loop while waiting
  // to hear about the print settings. We return the results
  // with an async message which frees the child process from
  // its nested event loop.
  if (NS_FAILED(rv)) {
    mozilla::Unused
        << PPrintingParent::PPrintSettingsDialogParent::Send__delete__(aDialog,
                                                                       rv);
  } else {
    mozilla::Unused
        << PPrintingParent::PPrintSettingsDialogParent::Send__delete__(
               aDialog, resultData);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult PrintingParent::RecvSavePrintSettings(
    const PrintData& aData, const bool& aUsePrinterNamePrefix,
    const uint32_t& aFlags, nsresult* aResult) {
  nsCOMPtr<nsIPrintSettings> settings;
  *aResult = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(settings));
  NS_ENSURE_SUCCESS(*aResult, IPC_OK());

  *aResult = mPrintSettingsSvc->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(*aResult, IPC_OK());

  *aResult = mPrintSettingsSvc->SavePrintSettingsToPrefs(
      settings, aUsePrinterNamePrefix, aFlags);

  return IPC_OK();
}

PPrintSettingsDialogParent* PrintingParent::AllocPPrintSettingsDialogParent() {
  return new PrintSettingsDialogParent();
}

bool PrintingParent::DeallocPPrintSettingsDialogParent(
    PPrintSettingsDialogParent* aDoomed) {
  delete aDoomed;
  return true;
}

void PrintingParent::ActorDestroy(ActorDestroyReason aWhy) {}

nsPIDOMWindowOuter* PrintingParent::DOMWindowFromBrowserParent(
    PBrowserParent* parent) {
  if (!parent) {
    return nullptr;
  }

  BrowserParent* browserParent = BrowserParent::GetFrom(parent);
  if (!browserParent) {
    return nullptr;
  }

  nsCOMPtr<Element> frameElement = browserParent->GetOwnerElement();
  if (!frameElement) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> frame(frameElement);
  if (!frame) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = frame->OwnerDoc()->GetWindow();
  if (!parentWin) {
    return nullptr;
  }

  return parentWin;
}

nsresult PrintingParent::SerializeAndEnsureRemotePrintJob(
    nsIPrintSettings* aPrintSettings, nsIWebProgressListener* aListener,
    layout::RemotePrintJobParent* aRemotePrintJob, PrintData* aPrintData) {
  MOZ_ASSERT(aPrintData);

  nsresult rv;
  nsCOMPtr<nsIPrintSettings> printSettings;
  if (aPrintSettings) {
    printSettings = aPrintSettings;
  } else {
    rv = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mPrintSettingsSvc->SerializeToPrintData(printSettings, aPrintData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RemotePrintJobParent* remotePrintJob;
  if (aRemotePrintJob) {
    remotePrintJob = aRemotePrintJob;
    aPrintData->remotePrintJobParent() = remotePrintJob;
  } else {
    remotePrintJob = new RemotePrintJobParent(aPrintSettings);
    aPrintData->remotePrintJobParent() =
        SendPRemotePrintJobConstructor(remotePrintJob);
  }
  if (aListener) {
    remotePrintJob->RegisterListener(aListener);
  }

  return NS_OK;
}

PrintingParent::PrintingParent() {
  mPrintSettingsSvc = do_GetService("@mozilla.org/gfx/printsettings-service;1");
  MOZ_ASSERT(mPrintSettingsSvc);
}

PrintingParent::~PrintingParent() = default;

}  // namespace embedding
}  // namespace mozilla
