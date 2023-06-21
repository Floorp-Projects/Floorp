/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef TESTING_GTEST_MOZILLA_WAITFOR_H_
#define TESTING_GTEST_MOZILLA_WAITFOR_H_

#include "MediaEventSource.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/SpinEventLoopUntil.h"

namespace mozilla {

/**
 * Waits for an occurrence of aEvent on the current thread (by blocking it,
 * except tasks added to the event loop may run) and returns the event's
 * templated value, if it's non-void.
 *
 * The caller must be wary of eventloop issues, in
 * particular cases where we rely on a stable state runnable, but there is never
 * a task to trigger stable state. In such cases it is the responsibility of the
 * caller to create the needed tasks, as JS would. A noteworthy API that relies
 * on stable state is MediaTrackGraph::GetInstance.
 */
template <typename T>
T WaitFor(MediaEventSource<T>& aEvent) {
  Maybe<T> value;
  MediaEventListener listener = aEvent.Connect(
      AbstractThread::GetCurrent(), [&](T aValue) { value = Some(aValue); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitFor(MediaEventSource<T>& aEvent)"_ns,
      [&] { return value.isSome(); });
  listener.Disconnect();
  return value.value();
}

/**
 * Specialization of WaitFor<T> for void.
 */
void WaitFor(MediaEventSource<void>& aEvent);

/**
 * Variant of WaitFor that blocks the caller until a MozPromise has either been
 * resolved or rejected.
 */
template <typename R, typename E, bool Exc>
Result<R, E> WaitFor(const RefPtr<MozPromise<R, E, Exc>>& aPromise) {
  Maybe<R> success;
  Maybe<E> error;
  aPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](R aResult) { success = Some(aResult); },
      [&](E aError) { error = Some(aError); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitFor(const RefPtr<MozPromise<R, E, Exc>>& aPromise)"_ns,
      [&] { return success.isSome() || error.isSome(); });
  if (success.isSome()) {
    return success.extract();
  }
  return Err(error.extract());
}

/**
 * A variation of WaitFor that takes a callback to be called each time aEvent is
 * raised. Blocks the caller until the callback function returns true.
 */
template <typename... Args, typename CallbackFunction>
void WaitUntil(MediaEventSource<Args...>& aEvent, CallbackFunction&& aF) {
  bool done = false;
  MediaEventListener listener =
      aEvent.Connect(AbstractThread::GetCurrent(), [&](Args... aValue) {
        if (!done) {
          done = aF(std::forward<Args>(aValue)...);
        }
      });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "WaitUntil(MediaEventSource<Args...>& aEvent, CallbackFunction&& aF)"_ns,
      [&] { return done; });
  listener.Disconnect();
}

template <typename... Args>
using TakeNPromise = MozPromise<std::vector<std::tuple<Args...>>, bool, true>;

template <ListenerPolicy Lp, typename... Args>
auto TakeN(MediaEventSourceImpl<Lp, Args...>& aEvent, size_t aN)
    -> RefPtr<TakeNPromise<Args...>> {
  using Storage = std::vector<std::tuple<Args...>>;
  using Promise = TakeNPromise<Args...>;
  using Values = media::Refcountable<Storage>;
  using Listener = media::Refcountable<MediaEventListener>;
  RefPtr<Values> values = MakeRefPtr<Values>();
  values->reserve(aN);
  RefPtr<Listener> listener = MakeRefPtr<Listener>();
  auto promise = InvokeAsync(
      AbstractThread::GetCurrent(), __func__, [values, aN]() mutable {
        SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
            "TakeN(MediaEventSourceImpl<Lp, Args...>& aEvent, size_t aN)"_ns,
            [&] { return values->size() == aN; });
        return Promise::CreateAndResolve(std::move(*values), __func__);
      });
  *listener = aEvent.Connect(AbstractThread::GetCurrent(),
                             [values, listener, aN](Args... aValue) {
                               values->push_back({aValue...});
                               if (values->size() == aN) {
                                 listener->Disconnect();
                               }
                             });
  return promise;
}

/**
 * Helper that, given that canonicals have just been updated on the current
 * thread, will block its execution until mirrors and their watchers have
 * executed on aTarget.
 */
inline void WaitForMirrors(const RefPtr<nsISerialEventTarget>& aTarget) {
  Unused << WaitFor(InvokeAsync(aTarget, __func__, [] {
    return GenericPromise::CreateAndResolve(true, "WaitForMirrors resolver");
  }));
}

/**
 * Short form of WaitForMirrors that assumes mirrors are on the current thread
 * (like canonicals).
 */
inline void WaitForMirrors() { WaitForMirrors(GetCurrentSerialEventTarget()); }

}  // namespace mozilla

#endif  // TESTING_GTEST_MOZILLA_WAITFOR_H_
