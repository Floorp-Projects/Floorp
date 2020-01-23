/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef runnable_utils_h__
#define runnable_utils_h__

#include <utility>

#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include <functional>
#include <tuple>

// Abstract base class for all of our templates
namespace mozilla {

namespace detail {

enum RunnableResult { NoResult, ReturnsResult };

static inline nsresult RunOnThreadInternal(nsIEventTarget* thread,
                                           nsIRunnable* runnable,
                                           uint32_t flags) {
  return thread->Dispatch(runnable, flags);
}

template <RunnableResult result>
class runnable_args_base : public Runnable {
 public:
  runnable_args_base() : Runnable("media-runnable_args_base") {}

  NS_IMETHOD Run() override = 0;
};

}  // namespace detail

template <typename FunType, typename... Args>
class runnable_args_func : public detail::runnable_args_base<detail::NoResult> {
 public:
  // |explicit| to pacify static analysis when there are no |args|.
  template <typename... Arguments>
  explicit runnable_args_func(FunType f, Arguments&&... args)
      : mFunc(f), mArgs(std::forward<Arguments>(args)...) {}

  NS_IMETHOD Run() override {
    std::apply(mFunc, std::move(mArgs));
    return NS_OK;
  }

 private:
  FunType mFunc;
  std::tuple<Args...> mArgs;
};

template <typename FunType, typename... Args>
runnable_args_func<FunType, typename mozilla::Decay<Args>::Type...>*
WrapRunnableNM(FunType f, Args&&... args) {
  return new runnable_args_func<FunType,
                                typename mozilla::Decay<Args>::Type...>(
      f, std::forward<Args>(args)...);
}

template <typename Ret, typename FunType, typename... Args>
class runnable_args_func_ret
    : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  template <typename... Arguments>
  runnable_args_func_ret(Ret* ret, FunType f, Arguments&&... args)
      : mReturn(ret), mFunc(f), mArgs(std::forward<Arguments>(args)...) {}

  NS_IMETHOD Run() override {
    *mReturn = std::apply(mFunc, std::move(mArgs));
    return NS_OK;
  }

 private:
  Ret* mReturn;
  FunType mFunc;
  std::tuple<Args...> mArgs;
};

template <typename R, typename FunType, typename... Args>
runnable_args_func_ret<R, FunType, typename mozilla::Decay<Args>::Type...>*
WrapRunnableNMRet(R* ret, FunType f, Args&&... args) {
  return new runnable_args_func_ret<R, FunType,
                                    typename mozilla::Decay<Args>::Type...>(
      ret, f, std::forward<Args>(args)...);
}

template <typename Class, typename M, typename... Args>
class runnable_args_memfn
    : public detail::runnable_args_base<detail::NoResult> {
 public:
  template <typename... Arguments>
  runnable_args_memfn(Class&& obj, M method, Arguments&&... args)
      : mObj(std::forward<Class>(obj)),
        mMethod(method),
        mArgs(std::forward<Arguments>(args)...) {}

  NS_IMETHOD Run() override {
    // Capturing `this` is okay here, the call is synchronous `mObj` and
    // `mMethod` are not passed to the callback and will not be modified.
    std::apply(
        [this](Args&&... args) {
          ((*mObj).*mMethod)(std::forward<Args>(args)...);
        },
        std::move(mArgs));
    return NS_OK;
  }

 private:
  // For holders such as RefPtr and UniquePtr make sure concrete copy is held
  // rather than a potential dangling reference.
  typename mozilla::Decay<Class>::Type mObj;
  M mMethod;
  std::tuple<Args...> mArgs;
};

template <typename Class, typename M, typename... Args>
runnable_args_memfn<Class, M, typename mozilla::Decay<Args>::Type...>*
WrapRunnable(Class&& obj, M method, Args&&... args) {
  return new runnable_args_memfn<Class, M,
                                 typename mozilla::Decay<Args>::Type...>(
      std::forward<Class>(obj), method, std::forward<Args>(args)...);
}

template <typename Ret, typename Class, typename M, typename... Args>
class runnable_args_memfn_ret
    : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  template <typename... Arguments>
  runnable_args_memfn_ret(Ret* ret, Class&& obj, M method, Arguments... args)
      : mReturn(ret),
        mObj(std::forward<Class>(obj)),
        mMethod(method),
        mArgs(std::forward<Arguments>(args)...) {}

  NS_IMETHOD Run() override {
    // Capturing `this` is okay here, the call is synchronous `mObj`,
    // `mMethod`, and `mReturn` are not passed to the callback and will not be
    // modified.
    std::apply(
        [this](Args&&... args) {
          *mReturn = ((*mObj).*mMethod)(std::forward<Args>(args)...);
        },
        std::move(mArgs));
    return NS_OK;
  }

 private:
  Ret* mReturn;
  // For holders such as RefPtr and UniquePtr make sure concrete copy is held
  // rather than a potential dangling reference.
  typename mozilla::Decay<Class>::Type mObj;
  M mMethod;
  std::tuple<Args...> mArgs;
};

template <typename R, typename Class, typename M, typename... Args>
runnable_args_memfn_ret<R, Class, M, typename mozilla::Decay<Args>::Type...>*
WrapRunnableRet(R* ret, Class&& obj, M method, Args&&... args) {
  return new runnable_args_memfn_ret<R, Class, M,
                                     typename mozilla::Decay<Args>::Type...>(
      ret, std::forward<Class>(obj), method, std::forward<Args>(args)...);
}

static inline nsresult RUN_ON_THREAD(
    nsIEventTarget* thread,
    detail::runnable_args_base<detail::NoResult>* runnable, uint32_t flags) {
  return detail::RunOnThreadInternal(
      thread, static_cast<nsIRunnable*>(runnable), flags);
}

static inline nsresult RUN_ON_THREAD(
    nsIEventTarget* thread,
    detail::runnable_args_base<detail::ReturnsResult>* runnable) {
  return detail::RunOnThreadInternal(
      thread, static_cast<nsIRunnable*>(runnable), NS_DISPATCH_SYNC);
}

#ifdef DEBUG
#  define ASSERT_ON_THREAD(t)           \
    do {                                \
      if (t) {                          \
        bool on;                        \
        nsresult rv;                    \
        rv = t->IsOnCurrentThread(&on); \
        MOZ_ASSERT(NS_SUCCEEDED(rv));   \
        MOZ_ASSERT(on);                 \
      }                                 \
    } while (0)
#else
#  define ASSERT_ON_THREAD(t)
#endif

template <class T>
class DispatchedRelease : public detail::runnable_args_base<detail::NoResult> {
 public:
  explicit DispatchedRelease(already_AddRefed<T>& ref) : ref_(ref) {}

  NS_IMETHOD Run() override {
    ref_ = nullptr;
    return NS_OK;
  }

 private:
  RefPtr<T> ref_;
};

template <typename T>
DispatchedRelease<T>* WrapRelease(already_AddRefed<T>&& ref) {
  return new DispatchedRelease<T>(ref);
}

} /* namespace mozilla */

#endif
