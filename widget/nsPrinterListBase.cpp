/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterListBase.h"
#include "PrintBackgroundTask.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/intl/Localization.h"
#include "xpcpublic.h"

using mozilla::ErrorResult;
using mozilla::intl::Localization;
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
  return mozilla::AsyncPromiseAttributeGetter(*this, mPrintersPromise, aCx,
                                              aResult, "Printers"_ns,
                                              &nsPrinterListBase::Printers);
}

NS_IMETHODIMP nsPrinterListBase::GetPrinterByName(const nsAString& aPrinterName,
                                                  JSContext* aCx,
                                                  Promise** aResult) {
  return PrintBackgroundTaskPromise(*this, aCx, aResult, "PrinterByName"_ns,
                                    &nsPrinterListBase::PrinterByName,
                                    nsString{aPrinterName});
}

NS_IMETHODIMP nsPrinterListBase::GetPrinterBySystemName(
    const nsAString& aPrinterName, JSContext* aCx, Promise** aResult) {
  return PrintBackgroundTaskPromise(
      *this, aCx, aResult, "PrinterBySystemName"_ns,
      &nsPrinterListBase::PrinterBySystemName, nsString{aPrinterName});
}

NS_IMETHODIMP nsPrinterListBase::GetNamedOrDefaultPrinter(
    const nsAString& aPrinterName, JSContext* aCx, Promise** aResult) {
  return PrintBackgroundTaskPromise(
      *this, aCx, aResult, "NamedOrDefaultPrinter"_ns,
      &nsPrinterListBase::NamedOrDefaultPrinter, nsString{aPrinterName});
}

Maybe<PrinterInfo> nsPrinterListBase::NamedOrDefaultPrinter(
    nsString aName) const {
  if (Maybe<PrinterInfo> value = PrinterByName(std::move(aName))) {
    return value;
  }

  // Since the name had to be passed by-value, we can re-use it to fetch the
  // default printer name, potentially avoiding an extra string allocation.
  if (NS_SUCCEEDED(SystemDefaultPrinterName(aName))) {
    return PrinterByName(std::move(aName));
  }

  return Nothing();
}

NS_IMETHODIMP nsPrinterListBase::GetFallbackPaperList(JSContext* aCx,
                                                      Promise** aResult) {
#define mm *72.0 / 25.4
#define in *72.0
  // As paper IDs, we use the PWG standardized names.
  // The names will be localized using strings in printUI.ftl.
  static const mozilla::PaperInfo kPapers[] = {
      {u"iso_a5"_ns, u"a5"_ns, {148 mm, 210 mm}, Some(MarginDouble{})},
      {u"iso_a4"_ns, u"a4"_ns, {210 mm, 297 mm}, Some(MarginDouble{})},
      {u"iso_a3"_ns, u"a3"_ns, {297 mm, 420 mm}, Some(MarginDouble{})},
      {u"iso_b5"_ns, u"b5"_ns, {176 mm, 250 mm}, Some(MarginDouble{})},
      {u"iso_b4"_ns, u"b4"_ns, {250 mm, 353 mm}, Some(MarginDouble{})},
      {u"jis_b5"_ns, u"jis-b5"_ns, {182 mm, 257 mm}, Some(MarginDouble{})},
      {u"jis_b4"_ns, u"jis-b4"_ns, {257 mm, 364 mm}, Some(MarginDouble{})},
      {u"na_letter"_ns, u"letter"_ns, {8.5 in, 11 in}, Some(MarginDouble{})},
      {u"na_legal"_ns, u"legal"_ns, {8.5 in, 14 in}, Some(MarginDouble{})},
      {u"na_ledger"_ns, u"tabloid"_ns, {11 in, 17 in}, Some(MarginDouble{})},
  };
#undef mm
#undef in

  ErrorResult rv;
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(aCx);
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    *aResult = nullptr;
    return rv.StealNSResult();
  }

  // Apply localization to the names while constructing the nsIPapers, if
  // available (otherwise leave them as the internal keys, which are at least
  // somewhat recognizable).
  RefPtr<Localization> l10n = Localization::Create(global, true, {});
  l10n->AddResourceId(u"toolkit/printing/printUI.ftl"_ns);

  nsTArray<RefPtr<nsPaper>> papers;
  papers.SetCapacity(mozilla::ArrayLength(kPapers));
  for (const auto& info : kPapers) {
    nsAutoCString key("printui-paper-");
    AppendUTF16toUTF8(info.mName, key);
    nsAutoCString name;
    l10n->FormatValueSync(aCx, key, {}, name, rv);
    if (rv.Failed() || name.IsEmpty()) {
      // No localized name found; use original key from the PaperInfo array.
      papers.AppendElement(MakeRefPtr<nsPaper>(info));
    } else {
      // Use the localized paper name.
      mozilla::PaperInfo locInfo = {info.mId, NS_ConvertUTF8toUTF16(name),
                                    info.mSize, info.mUnwriteableMargin};
      papers.AppendElement(MakeRefPtr<nsPaper>(locInfo));
    }
  }

  promise->MaybeResolve(papers);
  promise.forget(aResult);
  return NS_OK;
}
