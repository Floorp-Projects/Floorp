/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterBase.h"
#include "nsPaperMargin.h"
#include <utility>
#include "nsPrinter.h"
#include "nsPaper.h"
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

template <typename T>
void ResolveOrReject(Promise& aPromise, nsPrinterBase&, T& aResult) {
  aPromise.MaybeResolve(std::forward<T>(aResult));
}

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

template <typename T, typename... Args>
void nsPrinterBase::SpawnBackgroundTask(
    Promise& aPromise, BackgroundTask<T, Args...> aBackgroundTask,
    Args... aArgs) {
  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<Promise>>(
      "nsPrinterBase::SpawnBackgroundTaskPromise", &aPromise);
  // We actually want to allow to access the printer data from the callback, so
  // disable strict checking. They should of course only access immutable
  // members.
  auto holder = MakeRefPtr<nsMainThreadPtrHolder<nsPrinterBase>>(
      "nsPrinterBase::SpawnBackgroundTaskPrinter", this, /* strict = */ false);
  // See
  // https://stackoverflow.com/questions/47496358/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
  // about the tuple shenanigans. It could be improved with C++20
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "nsPrinterBase::SpawnBackgroundTask",
      [holder = std::move(holder), promiseHolder = std::move(promiseHolder),
       aBackgroundTask, aArgs = std::make_tuple(std::forward<Args>(aArgs)...)] {
        T result = std::apply(
            [&](auto&&... args) {
              return (holder->get()->*aBackgroundTask)(args...);
            },
            std::move(aArgs));
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "nsPrinterBase::SpawnBackgroundTaskResolution",
            [holder = std::move(holder),
             promiseHolder = std::move(promiseHolder),
             result = std::move(result)] {
              ResolveOrReject(*promiseHolder->get(), *holder->get(), result);
            }));
      }));
}

template <typename T, typename... Args>
nsresult nsPrinterBase::AsyncPromiseAttributeGetter(
    JSContext* aCx, Promise** aResultPromise, AsyncAttribute aAttribute,
    BackgroundTask<T, Args...> aBackgroundTask, Args... aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  if (RefPtr<Promise> existing = mAsyncAttributePromises[aAttribute]) {
    existing.forget(aResultPromise);
    return NS_OK;
  }
  ErrorResult rv;
  mAsyncAttributePromises[aAttribute] =
      Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<Promise> promise = mAsyncAttributePromises[aAttribute];
  SpawnBackgroundTask(*promise, aBackgroundTask, aArgs...);

  promise.forget(aResultPromise);
  return NS_OK;
}

NS_IMETHODIMP nsPrinterBase::GetSupportsDuplex(JSContext* aCx,
                                               Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter<bool>(aCx, aResultPromise,
                                           AsyncAttribute::SupportsDuplex,
                                           &nsPrinterBase::SupportsDuplex);
}

NS_IMETHODIMP nsPrinterBase::GetSupportsColor(JSContext* aCx,
                                              Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter<bool>(aCx, aResultPromise,
                                           AsyncAttribute::SupportsColor,
                                           &nsPrinterBase::SupportsColor);
}

NS_IMETHODIMP nsPrinterBase::GetPaperList(JSContext* aCx,
                                          Promise** aResultPromise) {
  return AsyncPromiseAttributeGetter<nsTArray<PaperInfo>>(
      aCx, aResultPromise, AsyncAttribute::PaperList,
      &nsPrinterBase::PaperList);
}

void nsPrinterBase::QueryMarginsForPaper(Promise& aPromise, uint64_t aPaperId) {
  return SpawnBackgroundTask<MarginDouble, uint64_t>(
      aPromise, &nsPrinterBase::GetMarginsForPaper, aPaperId);
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
