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

NS_IMETHODIMP nsPrinterListBase::GetFallbackPaperList(JSContext* aCx,
                                                      Promise** aResult) {
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    *aResult = nullptr;
    return rv.StealNSResult();
  }
  promise->MaybeResolve(FallbackPaperList());
  promise.forget(aResult);
  return NS_OK;
}

nsTArray<RefPtr<nsPaper>> nsPrinterListBase::FallbackPaperList() const {
#define mm *72.0 / 25.4
#define in *72.0
  static const mozilla::PaperInfo kPapers[] = {
      {u"A5"_ns, {148 mm, 210 mm}, Some(MarginDouble{})},
      {u"A4"_ns, {210 mm, 297 mm}, Some(MarginDouble{})},
      {u"A3"_ns, {297 mm, 420 mm}, Some(MarginDouble{})},
      {u"B5"_ns, {176 mm, 250 mm}, Some(MarginDouble{})},
      {u"B4"_ns, {250 mm, 353 mm}, Some(MarginDouble{})},
      {u"JIS-B5"_ns, {182 mm, 257 mm}, Some(MarginDouble{})},
      {u"JIS-B4"_ns, {257 mm, 364 mm}, Some(MarginDouble{})},
      {u"US Letter"_ns, {8.5 in, 11 in}, Some(MarginDouble{})},
      {u"US Legal"_ns, {8.5 in, 14 in}, Some(MarginDouble{})},
      {u"Tabloid"_ns, {11 in, 17 in}, Some(MarginDouble{})},
  };
#undef mm
#undef in

  // TODO:
  // Replace the en-US strings above with lowercased, "US"-stripped versions
  // as found in printUI.ftl, and call Fluent to get localized paper names.
  // Consider whether any more sizes should be included (A0-A2?).

  nsTArray<RefPtr<nsPaper>> result;
  result.SetCapacity(mozilla::ArrayLength(kPapers));
  for (const auto& info : kPapers) {
    result.AppendElement(MakeRefPtr<nsPaper>(info));
  }

  return result;
}
