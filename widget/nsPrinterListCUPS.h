/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterListCUPS_h__
#define nsPrinterListCUPS_h__

#include "nsPrinterListBase.h"
#include "nsStringFwd.h"

class nsPrinterListCUPS final : public nsPrinterListBase {
  NS_IMETHOD InitPrintSettingsFromPrinter(const nsAString&,
                                          nsIPrintSettings*) final;

  nsTArray<PrinterInfo> Printers() const final;
  RefPtr<nsIPrinter> CreatePrinter(PrinterInfo) const final;
  Maybe<PrinterInfo> PrinterByName(nsString aPrinterName) const final;
  Maybe<PrinterInfo> PrinterBySystemName(nsString aPrinterName) const final;
  nsresult SystemDefaultPrinterName(nsAString&) const final;

 private:
  ~nsPrinterListCUPS() override = default;
};

#endif
