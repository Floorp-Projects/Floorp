/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterListBase.h"
#include "PrintBackgroundTask.h"
#include "mozilla/ErrorResult.h"
#include "xpcpublic.h"

using mozilla::ErrorResult;
using PrinterInfo = nsPrinterListBase::PrinterInfo;

nsPrinterListBase::nsPrinterListBase() = default;
nsPrinterListBase::~nsPrinterListBase() = default;

NS_IMPL_CYCLE_COLLECTION(nsPrinterListBase, mPrintersPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPrinterListBase)
  NS_INTERFACE_MAP_ENTRY(nsIPrinterList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrinterList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPrinterListBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPrinterListBase)

namespace mozilla {

template <>
void ResolveOrReject(dom::Promise& aPromise, nsPrinterListBase& aList,
                     const nsTArray<PrinterInfo>& aInfo) {
  nsTArray<RefPtr<nsIPrinter>> printers;
  printers.SetCapacity(aInfo.Length());
  for (auto& info : aInfo) {
    printers.AppendElement(aList.CreatePrinter(info));
  }
  aPromise.MaybeResolve(printers);
}

template <>
void ResolveOrReject(dom::Promise& aPromise, nsPrinterListBase& aList,
                     const Maybe<PrinterInfo>& aInfo) {
  if (aInfo) {
    aPromise.MaybeResolve(aList.CreatePrinter(aInfo.value()));
  } else {
    aPromise.MaybeRejectWithNotFoundError("Printer not found");
  }
}

}  // namespace mozilla

NS_IMETHODIMP nsPrinterListBase::GetPrinters(JSContext* aCx,
                                             Promise** aResult) {
  return mozilla::AsyncPromiseAttributeGetter(
      *this, mPrintersPromise, aCx, aResult, &nsPrinterListBase::Printers);
}

NS_IMETHODIMP nsPrinterListBase::GetNamedPrinter(const nsAString& aPrinterName,
                                                 JSContext* aCx,
                                                 Promise** aResult) {
  return PrintBackgroundTaskPromise(*this, aCx, aResult,
                                    &nsPrinterListBase::NamedPrinter,
                                    nsString{aPrinterName});
}

NS_IMETHODIMP nsPrinterListBase::GetNamedOrDefaultPrinter(
    const nsAString& aPrinterName, JSContext* aCx, Promise** aResult) {
  return PrintBackgroundTaskPromise(*this, aCx, aResult,
                                    &nsPrinterListBase::NamedOrDefaultPrinter,
                                    nsString{aPrinterName});
}

Maybe<PrinterInfo> nsPrinterListBase::NamedOrDefaultPrinter(
    nsString aName) const {
  if (Maybe<PrinterInfo> value = NamedPrinter(std::move(aName))) {
    return value;
  }

  // Since the name had to be passed by-value, we can re-use it to fetch the
  // default printer name, potentially avoiding an extra string allocation.
  if (NS_SUCCEEDED(SystemDefaultPrinterName(aName))) {
    return NamedPrinter(std::move(aName));
  }

  return Nothing();
}
