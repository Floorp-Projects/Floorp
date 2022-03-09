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
#include "mozilla/layout/RemotePrintJobParent.h"
#include "mozilla/StaticPrefs_print.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

namespace mozilla {
namespace embedding {
void PrintingParent::ActorDestroy(ActorDestroyReason aWhy) {}

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
