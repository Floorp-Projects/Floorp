/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterBase.h"
#include "nsPrinter.h"
#include "nsPaper.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;

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
nsresult nsPrinterBase::AsyncPromiseAttributeGetter(
    JSContext* aCx, Promise** aResultPromise, AsyncAttribute aAttribute,
    AsyncAttributeBackgroundTask<T> aBackgroundTask) {
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

  // We actually want to allow to access the printer data from the callback, so
  // disable strict checking. They should of course only access immutable
  // members.
  auto holder = MakeRefPtr<nsMainThreadPtrHolder<nsPrinterBase>>(
      "nsPrinterBase::AsyncPromiseAttributeGetter", this, /* strict = */ false);
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "nsPrinterBase::AsyncPromiseAttributeGetter",
      [holder = std::move(holder), aAttribute, aBackgroundTask] {
        T result = (holder->get()->*aBackgroundTask)();
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "nsPrinterBase::AsyncPromiseAttributeGetterResult",
            [holder = std::move(holder), result = std::move(result),
             aAttribute] {
              nsPrinterBase* printer = holder->get();
              // There could be no promise, could've been CC'd or what not.
              if (Promise* p = printer->mAsyncAttributePromises[aAttribute]) {
                p->MaybeResolve(result);
              }
            }));
      }));

  RefPtr<Promise> existing = mAsyncAttributePromises[aAttribute];
  existing.forget(aResultPromise);
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

NS_IMPL_CYCLE_COLLECTION(nsPrinterBase, mAsyncAttributePromises)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPrinterBase)
  NS_INTERFACE_MAP_ENTRY(nsIPrinter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrinter)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPrinterBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPrinterBase)

nsPrinterBase::nsPrinterBase() = default;
nsPrinterBase::~nsPrinterBase() = default;
