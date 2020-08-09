/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterListBase_h__
#define nsPrinterListBase_h__

#include "nsIPrinterList.h"

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPrinterListBase : public nsIPrinterList {
 public:
  using Promise = mozilla::dom::Promise;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterListBase)
  NS_IMETHOD GetPrinters(JSContext*, Promise**) final;

  struct PrinterInfo {
    // Both windows and CUPS: The name of the printer.
    nsString mName;
    // CUPS only: Two handles to owned cups_dest_t / cups_dinfo_t objects.
    std::array<void*, 2> mCupsHandles{};
  };

  // Called off the main thread, collect information to create an appropriate
  // list of printers.
  virtual nsTArray<PrinterInfo> GetPrinters() const = 0;

  // Create an nsIPrinter object given the information we obtained from the
  // background thread.
  virtual RefPtr<nsIPrinter> CreatePrinter(PrinterInfo) const = 0;

  // No copy or move, we're an identity.
  nsPrinterListBase(const nsPrinterListBase&) = delete;
  nsPrinterListBase(nsPrinterListBase&&) = delete;

 protected:
  nsPrinterListBase();
  virtual ~nsPrinterListBase();

  RefPtr<Promise> mPrintersPromise;
};

#endif
