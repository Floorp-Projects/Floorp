/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterBase.h"
#include "nsPaperMargin.h"
#include <utility>
#include "nsPaper.h"
#include "nsIPrintSettings.h"
#include "nsPrintSettingsService.h"
#include "PrintBackgroundTask.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using mozilla::dom::Promise;
using mozilla::gfx::MarginDouble;

// The maximum error when considering a paper size equal, in points.
// There is some variance in the actual sizes returned by printer drivers and
// print servers for paper sizes. This is a best-guess based on initial
// telemetry which should catch most near-miss dimensions. This should let us
// get consistent paper size names even when the size isn't quite exactly the
// correct size.
static constexpr double kPaperSizePointsEpsilon = 4.0;

// Basic implementation of nsIPrinterInfo
class nsPrinterInfo : public nsIPrinterInfo {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterInfo)
  NS_DECL_NSIPRINTERINFO
  nsPrinterInfo() = delete;
  nsPrinterInfo(nsPrinterBase& aPrinter,
                const nsPrinterBase::PrinterInfo& aPrinterInfo)
      : mDefaultSettings(
            CreatePlatformPrintSettings(aPrinterInfo.mDefaultSettings)) {
    mPaperList.SetCapacity(aPrinterInfo.mPaperList.Length());
    for (const PaperInfo& info : aPrinterInfo.mPaperList) {
      mPaperList.AppendElement(MakeRefPtr<nsPaper>(aPrinter, info));
    }

    // Update the printer's default settings with the global settings.
    nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
        do_GetService("@mozilla.org/gfx/printsettings-service;1");
    if (printSettingsSvc) {
      // Passing false as the second parameter means we don't get the printer
      // specific settings.
      printSettingsSvc->InitPrintSettingsFromPrefs(
          mDefaultSettings, false, nsIPrintSettings::kInitSaveAll);
    }
  }

 private:
  virtual ~nsPrinterInfo() = default;

  nsTArray<RefPtr<nsIPaper>> mPaperList;
  RefPtr<nsIPrintSettings> mDefaultSettings;
};

NS_IMETHODIMP
nsPrinterInfo::GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) {
  aPaperList = mPaperList.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterInfo::GetDefaultSettings(nsIPrintSettings** aDefaultSettings) {
  NS_ENSURE_ARG_POINTER(aDefaultSettings);
  MOZ_ASSERT(mDefaultSettings);
  RefPtr<nsIPrintSettings> settings = mDefaultSettings;
  settings.forget(aDefaultSettings);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(nsPrinterInfo, mPaperList, mDefaultSettings)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPrinterInfo)
  NS_INTERFACE_MAP_ENTRY(nsIPrinterInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrinterInfo)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPrinterInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPrinterInfo)

template <typename Index, Index Size, typename Value>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    EnumeratedArray<Index, Size, Value>& aArray, const char* aName,
    uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  for (Value& element : aArray) {
    ImplCycleCollectionTraverse(aCallback, element, aName, aFlags);
  }
}

template <typename Index, Index Size, typename Value>
inline void ImplCycleCollectionUnlink(
    EnumeratedArray<Index, Size, Value>& aArray) {
  for (Value& element : aArray) {
    ImplCycleCollectionUnlink(element);
  }
}

namespace mozilla {

template <>
void ResolveOrReject(Promise& aPromise, nsPrinterBase&,
                     const MarginDouble& aResult) {
  auto margin = MakeRefPtr<nsPaperMargin>(aResult);
  aPromise.MaybeResolve(margin);
}

template <>
void ResolveOrReject(Promise& aPromise, nsPrinterBase& aPrinter,
                     const nsTArray<PaperInfo>& aResult) {
  nsTArray<RefPtr<nsPaper>> result;
  result.SetCapacity(aResult.Length());
  for (const PaperInfo& info : aResult) {
    result.AppendElement(MakeRefPtr<nsPaper>(aPrinter, info));
  }
  aPromise.MaybeResolve(result);
}

template <>
void ResolveOrReject(Promise& aPromise, nsPrinterBase& aPrinter,
                     const PrintSettingsInitializer& aResult) {
  aPromise.MaybeResolve(
      RefPtr<nsIPrintSettings>(CreatePlatformPrintSettings(aResult)));
}

template <>
void ResolveOrReject(Promise& aPromise, nsPrinterBase& aPrinter,
                     const nsPrinterBase::PrinterInfo& aResult) {
  aPromise.MaybeResolve(MakeRefPtr<nsPrinterInfo>(aPrinter, aResult));
}

}  // namespace mozilla

template <typename T, typename... Args>
nsresult nsPrinterBase::AsyncPromiseAttributeGetter(
    JSContext* aCx, Promise** aResultPromise, AsyncAttribute aAttribute,
    BackgroundTask<T, Args...> aBackgroundTask, Args... aArgs) {
  MOZ_ASSERT(NS_IsMainThread());

  static constexpr EnumeratedArray<AsyncAttribute, AsyncAttribute::Last,
                                   nsLiteralCString>
      attributeKeys{"SupportsDuplex"_ns, "SupportsColor"_ns,
                    "SupportsMonochrome"_ns, "SupportsCollation"_ns,
                    "PrinterInfo"_ns};
  return mozilla::AsyncPromiseAttributeGetter(
      *this, mAsyncAttributePromises[aAttribute], aCx, aResultPromise,
      attributeKeys[aAttribute], aBackgroundTask, std::forward<Args>(aArgs)...);
}

NS_IMETHODIMP nsPrinterBase::CopyFromWithValidation(
    nsIPrintSettings* aSettingsToCopyFrom, JSContext* aCx,
    Promise** aResultPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResultPromise);

  ErrorResult errorResult;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), errorResult);
  if (MOZ_UNLIKELY(errorResult.Failed())) {
    return errorResult.StealNSResult();
  }

  nsCOMPtr<nsIPrintSettings> settings;
  MOZ_ALWAYS_SUCCEEDS(aSettingsToCopyFrom->Clone(getter_AddRefs(settings)));
  nsString printerName;
  MOZ_ALWAYS_SUCCEEDS(GetName(printerName));
  MOZ_ALWAYS_SUCCEEDS(settings->SetPrinterName(printerName));
  promise->MaybeResolve(settings);
  promise.forget(aResultPromise);

  return NS_OK;
}

NS_IMETHODIMP nsPrinterBase::GetSupportsDuplex(JSContext* aCx,
                                               Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::SupportsDuplex,
                                     &nsPrinterBase::SupportsDuplex);
}

NS_IMETHODIMP nsPrinterBase::GetSupportsColor(JSContext* aCx,
                                              Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::SupportsColor,
                                     &nsPrinterBase::SupportsColor);
}

NS_IMETHODIMP nsPrinterBase::GetSupportsMonochrome(JSContext* aCx,
                                                   Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::SupportsMonochrome,
                                     &nsPrinterBase::SupportsMonochrome);
}

NS_IMETHODIMP nsPrinterBase::GetSupportsCollation(JSContext* aCx,
                                                  Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::SupportsCollation,
                                     &nsPrinterBase::SupportsCollation);
}

NS_IMETHODIMP nsPrinterBase::GetPrinterInfo(JSContext* aCx,
                                            Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::PrinterInfo,
                                     &nsPrinterBase::CreatePrinterInfo);
}

void nsPrinterBase::QueryMarginsForPaper(Promise& aPromise,
                                         const nsString& aPaperId) {
  return SpawnPrintBackgroundTask(*this, aPromise, "MarginsForPaper"_ns,
                                  &nsPrinterBase::GetMarginsForPaper, aPaperId);
}

NS_IMPL_CYCLE_COLLECTION(nsPrinterBase, mAsyncAttributePromises)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPrinterBase)
  NS_INTERFACE_MAP_ENTRY(nsIPrinter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrinter)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPrinterBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPrinterBase)

nsPrinterBase::nsPrinterBase(const CommonPaperInfoArray* aPaperInfoArray)
    : mCommonPaperInfo(aPaperInfoArray) {
  MOZ_DIAGNOSTIC_ASSERT(aPaperInfoArray, "Localized paper info was null");
}
nsPrinterBase::~nsPrinterBase() = default;

const PaperInfo* nsPrinterBase::FindCommonPaperSize(
    const gfx::SizeDouble& aSize) const {
  for (const PaperInfo& paper : *mCommonPaperInfo) {
    if (std::abs(paper.mSize.width - aSize.width) <= kPaperSizePointsEpsilon &&
        std::abs(paper.mSize.height - aSize.height) <=
            kPaperSizePointsEpsilon) {
      return &paper;
    }
  }
  return nullptr;
}
