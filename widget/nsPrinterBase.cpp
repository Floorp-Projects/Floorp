/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterBase.h"
#include "nsPaperMargin.h"
#include <utility>
#include "nsPaper.h"
#include "PrintBackgroundTask.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using mozilla::dom::Promise;
using mozilla::gfx::MarginDouble;

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

}  // namespace mozilla

template <typename T, typename... Args>
nsresult nsPrinterBase::AsyncPromiseAttributeGetter(
    JSContext* aCx, Promise** aResultPromise, AsyncAttribute aAttribute,
    BackgroundTask<T, Args...> aBackgroundTask, Args... aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  return mozilla::AsyncPromiseAttributeGetter(
      *this, mAsyncAttributePromises[aAttribute], aCx, aResultPromise,
      aBackgroundTask, std::forward<Args>(aArgs)...);
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

void nsPrinterBase::QueryMarginsForPaper(Promise& aPromise, uint64_t aPaperId) {
  return SpawnPrintBackgroundTask(*this, aPromise,
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
