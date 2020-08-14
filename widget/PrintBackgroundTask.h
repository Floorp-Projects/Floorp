/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrintBackgroundTask_h_
#define mozilla_PrintBackgroundTask_h_

#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include <utility>
#include <tuple>

// A helper to resolve a DOM Promise with the result of a const method, executed
// in another thread.
//
// Once in the main thread, the caller can turn the result of the method into a
// JSValue by specializing ResolveOrReject.
namespace mozilla {

template <typename T, typename Result>
void ResolveOrReject(dom::Promise& aPromise, T&, Result& aResult) {
  aPromise.MaybeResolve(std::forward<Result>(aResult));
}

template <typename T, typename Result, typename... Args>
using PrintBackgroundTask = Result (T::*)(Args...) const;

template <typename T, typename Result, typename... Args>
void SpawnPrintBackgroundTask(
    T& aReceiver, dom::Promise& aPromise,
    PrintBackgroundTask<T, Result, Args...> aBackgroundTask, Args... aArgs) {
  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<dom::Promise>>(
      "nsPrinterBase::SpawnBackgroundTaskPromise", &aPromise);
  // We actually want to allow to access the printer data from the callback, so
  // disable strict checking. They should of course only access immutable
  // members.
  auto holder = MakeRefPtr<nsMainThreadPtrHolder<T>>(
      "nsPrinterBase::SpawnBackgroundTaskPrinter", &aReceiver,
      /* strict = */ false);
  // See
  // https://stackoverflow.com/questions/47496358/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
  // about the tuple shenanigans. It could be improved with C++20
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "SpawnPrintBackgroundTask",
      [holder = std::move(holder), promiseHolder = std::move(promiseHolder),
       aBackgroundTask, aArgs = std::make_tuple(std::forward<Args>(aArgs)...)] {
        Result result = std::apply(
            [&](auto&&... args) {
              return (holder->get()->*aBackgroundTask)(args...);
            },
            std::move(aArgs));
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "SpawnPrintBackgroundTaskResolution",
            [holder = std::move(holder),
             promiseHolder = std::move(promiseHolder),
             result = std::move(result)] {
              ResolveOrReject(*promiseHolder->get(), *holder->get(), result);
            }));
      }));
}

// Gets a fresh promise into aResultPromise, that resolves whenever the print
// background task finishes.
template <typename T, typename Result, typename... Args>
nsresult PrintBackgroundTaskPromise(
    T& aReceiver, JSContext* aCx, dom::Promise** aResultPromise,
    PrintBackgroundTask<T, Result, Args...> aTask, Args... aArgs) {
  ErrorResult rv;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  SpawnPrintBackgroundTask(aReceiver, *promise, aTask,
                           std::forward<Args>(aArgs)...);

  promise.forget(aResultPromise);
  return NS_OK;
}

// Resolves an async attribute via a background task, creating and storing a
// promise as needed in aPromiseSlot.
template <typename T, typename Result, typename... Args>
nsresult AsyncPromiseAttributeGetter(
    T& aReceiver, RefPtr<dom::Promise>& aPromiseSlot, JSContext* aCx,
    dom::Promise** aResultPromise,
    PrintBackgroundTask<T, Result, Args...> aTask, Args... aArgs) {
  if (RefPtr<dom::Promise> existing = aPromiseSlot) {
    existing.forget(aResultPromise);
    return NS_OK;
  }

  nsresult rv = PrintBackgroundTaskPromise(aReceiver, aCx, aResultPromise,
                                           aTask, std::forward<Args>(aArgs)...);
  NS_ENSURE_SUCCESS(rv, rv);

  aPromiseSlot = *aResultPromise;
  return NS_OK;
}

}  // namespace mozilla

#endif
