/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterListBase_h__
#define nsPrinterListBase_h__

#include "nsIPrinterList.h"

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsPaper.h"
#include "nsString.h"

class nsPrinterListBase : public nsIPrinterList {
 public:
  using Promise = mozilla::dom::Promise;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterListBase)
  NS_IMETHOD GetSystemDefaultPrinterName(nsAString& aName) final {
    return SystemDefaultPrinterName(aName);
  }
  NS_IMETHOD GetPrinters(JSContext*, Promise**) final;
  NS_IMETHOD GetPrinterByName(const nsAString& aPrinterName, JSContext* aCx,
                              Promise** aResult) final;
  NS_IMETHOD GetPrinterBySystemName(const nsAString& aPrinterName,
                                    JSContext* aCx, Promise** aResult) final;
  NS_IMETHOD GetNamedOrDefaultPrinter(const nsAString& aPrinterName,
                                      JSContext* aCx, Promise** aResult) final;
  NS_IMETHOD GetFallbackPaperList(JSContext*, Promise**) final;

  struct PrinterInfo {
    // Both windows and CUPS: The name of the printer.
    nsString mName;
    // CUPS only: Handle to owned cups_dest_t.
    void* mCupsHandle = nullptr;
  };

  // Called off the main thread, collect information to create an appropriate
  // list of printers.
  virtual nsTArray<PrinterInfo> Printers() const = 0;

  // Create an nsIPrinter object given the information we obtained from the
  // background thread.
  virtual RefPtr<nsIPrinter> CreatePrinter(PrinterInfo) const = 0;

  Maybe<PrinterInfo> NamedOrDefaultPrinter(nsString aName) const;

  // No copy or move, we're an identity.
  nsPrinterListBase(const nsPrinterListBase&) = delete;
  nsPrinterListBase(nsPrinterListBase&&) = delete;

 protected:
  nsPrinterListBase();
  virtual ~nsPrinterListBase();

  // This could be implemented in terms of Printers() and then searching the
  // returned printer info for a printer of the given name, but we expect
  // backends to have more efficient methods of implementing this.
  virtual Maybe<PrinterInfo> PrinterByName(nsString aName) const = 0;

  // Same as NamedPrinter, but uses the system name.
  // Depending on whether or not there is a more efficient way to address the
  // printer for a given backend, this may or may not be equivalent to
  // NamedPrinter.
  virtual Maybe<PrinterInfo> PrinterBySystemName(nsString aName) const = 0;

  // This is implemented separately from the IDL interface version so that it
  // can be made const, which allows it to be used while resolving promises.
  virtual nsresult SystemDefaultPrinterName(nsAString&) const = 0;

  // Return "paper" sizes to be supported by the Save to PDF destination;
  // for actual printer drivers the list is retrieved from nsIPrinter.
  nsTArray<RefPtr<nsPaper>> FallbackPaperList() const;

  RefPtr<Promise> mPrintersPromise;
};

#endif
