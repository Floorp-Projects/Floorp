/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintingProxy_h
#define __nsPrintingProxy_h

#include "nsIPrintingPromptService.h"
#include "mozilla/embedding/PPrintingChild.h"

namespace mozilla {
namespace layout {
class PRemotePrintJobChild;
}
}  // namespace mozilla

class nsPrintingProxy final : public nsIPrintingPromptService,
                              public mozilla::embedding::PPrintingChild {
  friend class mozilla::embedding::PPrintingChild;

 public:
  static already_AddRefed<nsPrintingProxy> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTINGPROMPTSERVICE

  /**
   * Used to proxy nsIPrintSettings.savePrintSettingsToPrefs calls to the
   * parent process.
   *
   * @param aFlags - kInitSave* flags from nsIPrintSettings's to specify
   *          which settings to save.
   */
  nsresult SavePrintSettings(nsIPrintSettings* aPS, bool aUsePrinterNamePrefix,
                             uint32_t aFlags);

 protected:
  PPrintProgressDialogChild* AllocPPrintProgressDialogChild() final;

  bool DeallocPPrintProgressDialogChild(
      PPrintProgressDialogChild* aActor) final;

  PPrintSettingsDialogChild* AllocPPrintSettingsDialogChild() final;

  bool DeallocPPrintSettingsDialogChild(
      PPrintSettingsDialogChild* aActor) final;

  PRemotePrintJobChild* AllocPRemotePrintJobChild() final;

  bool DeallocPRemotePrintJobChild(PRemotePrintJobChild* aActor) final;

 private:
  nsPrintingProxy();

  ~nsPrintingProxy() final;

  nsresult Init();
};

#endif
