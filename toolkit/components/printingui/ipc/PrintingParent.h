/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintingParent_h
#define mozilla_embedding_PrintingParent_h

#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/embedding/PPrintingParent.h"

class nsIPrintSettingsService;
class nsIWebProgressListener;
class nsPIDOMWindowOuter;
class PPrintProgressDialogParent;
class PPrintSettingsDialogParent;

namespace mozilla {
namespace layout {
class PRemotePrintJobParent;
class RemotePrintJobParent;
}  // namespace layout

namespace embedding {

class PrintingParent final : public PPrintingParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(PrintingParent)

  mozilla::ipc::IPCResult RecvShowProgress(
      PBrowserParent* parent, PPrintProgressDialogParent* printProgressDialog,
      PRemotePrintJobParent* remotePrintJob, const bool& isForPrinting) final;
  mozilla::ipc::IPCResult RecvShowPrintDialog(
      PPrintSettingsDialogParent* aDialog, PBrowserParent* aParent,
      const PrintData& aData) final;

  mozilla::ipc::IPCResult RecvSavePrintSettings(
      const PrintData& data, const bool& usePrinterNamePrefix,
      const uint32_t& flags, nsresult* rv) final;

  PPrintProgressDialogParent* AllocPPrintProgressDialogParent() final;

  bool DeallocPPrintProgressDialogParent(
      PPrintProgressDialogParent* aActor) final;

  PPrintSettingsDialogParent* AllocPPrintSettingsDialogParent() final;

  bool DeallocPPrintSettingsDialogParent(
      PPrintSettingsDialogParent* aActor) final;

  PRemotePrintJobParent* AllocPRemotePrintJobParent() final;

  bool DeallocPRemotePrintJobParent(PRemotePrintJobParent* aActor) final;

  void ActorDestroy(ActorDestroyReason aWhy) final;

  MOZ_IMPLICIT PrintingParent();

  /**
   * Serialize nsIPrintSettings to PrintData ready for sending to a child
   * process. A RemotePrintJob will be created and added to the PrintData.
   * An optional progress listener can be given, which will be registered
   * with the RemotePrintJob, so that progress can be tracked in the parent.
   *
   * @param aPrintSettings optional print settings to serialize, otherwise a
   *                       default print settings will be used.
   * @param aProgressListener optional print progress listener.
   * @param aRemotePrintJob optional remote print job, so that an existing
   *                        one can be used.
   * @param aPrintData PrintData to populate.
   */
  nsresult SerializeAndEnsureRemotePrintJob(
      nsIPrintSettings* aPrintSettings, nsIWebProgressListener* aListener,
      layout::RemotePrintJobParent* aRemotePrintJob, PrintData* aPrintData);

 private:
  ~PrintingParent() final;

  nsPIDOMWindowOuter* DOMWindowFromBrowserParent(PBrowserParent* parent);

  nsresult ShowPrintDialog(PBrowserParent* parent, const PrintData& data,
                           PrintData* result);

  nsCOMPtr<nsIPrintSettingsService> mPrintSettingsSvc;
};

}  // namespace embedding
}  // namespace mozilla

#endif
