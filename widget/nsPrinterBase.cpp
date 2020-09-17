/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterBase.h"
#include "nsPaperMargin.h"
#include <utility>
#include "nsPaper.h"
#include "nsIPrintSettings.h"
#include "PrintBackgroundTask.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using mozilla::dom::Promise;
using mozilla::gfx::MarginDouble;

void nsPrinterBase::CachePrintSettingsInitializer(
    const PrintSettingsInitializer& aInitializer) {
  if (mPrintSettingsInitializer.isNothing()) {
    mPrintSettingsInitializer.emplace(aInitializer);
  }
}

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
  aPrinter.CachePrintSettingsInitializer(aResult);
  aPromise.MaybeResolve(
      RefPtr<nsIPrintSettings>(CreatePlatformPrintSettings(aResult)));
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
                    "PaperList"_ns};
  return mozilla::AsyncPromiseAttributeGetter(
      *this, mAsyncAttributePromises[aAttribute], aCx, aResultPromise,
      attributeKeys[aAttribute], aBackgroundTask, std::forward<Args>(aArgs)...);
}

NS_IMETHODIMP nsPrinterBase::CreateDefaultSettings(JSContext* aCx,
                                                   Promise** aResultPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aResultPromise);

  if (mPrintSettingsInitializer.isSome()) {
    ErrorResult rv;
    RefPtr<dom::Promise> promise =
        dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);

    if (MOZ_UNLIKELY(rv.Failed())) {
      *aResultPromise = nullptr;
      return rv.StealNSResult();
    }

    RefPtr<nsIPrintSettings> settings(
        CreatePlatformPrintSettings(mPrintSettingsInitializer.ref()));
    promise->MaybeResolve(settings);

    promise.forget(aResultPromise);
    return NS_OK;
  }

  return PrintBackgroundTaskPromise(*this, aCx, aResultPromise,
                                    "DefaultSettings"_ns,
                                    &nsPrinterBase::DefaultSettings);
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

NS_IMETHODIMP nsPrinterBase::GetPaperList(JSContext* aCx,
                                          Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter(aCx, aResultPromise,
                                     AsyncAttribute::PaperList,
                                     &nsPrinterBase::PaperList);
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

nsPrinterBase::nsPrinterBase() = default;
nsPrinterBase::~nsPrinterBase() = default;
