/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef runnable_utils_h__
#define runnable_utils_h__

#include "nsThreadUtils.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"

#include <utility>

// Abstract base class for all of our templates
namespace mozilla {

namespace detail {

enum RunnableResult {
  NoResult,
  ReturnsResult
};

static inline nsresult
RunOnThreadInternal(nsIEventTarget *thread, nsIRunnable *runnable, uint32_t flags)
{
  return thread->Dispatch(runnable, flags);
}

template<RunnableResult result>
class runnable_args_base : public Runnable {
 public:
  runnable_args_base() : Runnable("media-runnable_args_base") {}

  NS_IMETHOD Run() override = 0;
};


template<typename R>
struct RunnableFunctionCallHelper
{
  template<typename FunType, typename... Args, size_t... Indices>
  static R apply(FunType func, Tuple<Args...>& args, std::index_sequence<Indices...>)
  {
    return func(Get<Indices>(args)...);
  }
};

// A void specialization is needed in the case where the template instantiator
// knows we don't want to return a value, but we don't know whether the called
// function returns void or something else.
template<>
struct RunnableFunctionCallHelper<void>
{
  template<typename FunType, typename... Args, size_t... Indices>
  static void apply(FunType func, Tuple<Args...>& args, std::index_sequence<Indices...>)
  {
    func(Get<Indices>(args)...);
  }
};

template<typename R>
struct RunnableMethodCallHelper
{
  template<typename Class, typename M, typename... Args, size_t... Indices>
  static R apply(Class obj, M method, Tuple<Args...>& args, std::index_sequence<Indices...>)
  {
    return ((*obj).*method)(Get<Indices>(args)...);
  }
};

// A void specialization is needed in the case where the template instantiator
// knows we don't want to return a value, but we don't know whether the called
// method returns void or something else.
template<>
struct RunnableMethodCallHelper<void>
{
  template<typename Class, typename M, typename... Args, size_t... Indices>
  static void apply(Class obj, M method, Tuple<Args...>& args, std::index_sequence<Indices...>)
  {
    ((*obj).*method)(Get<Indices>(args)...);
  }
};

}

template<typename FunType, typename... Args>
class runnable_args_func : public detail::runnable_args_base<detail::NoResult>
{
public:
  // |explicit| to pacify static analysis when there are no |args|.
  template<typename... Arguments>
  explicit runnable_args_func(FunType f, Arguments&&... args)
    : mFunc(f), mArgs(std::forward<Arguments>(args)...)
  {}

  NS_IMETHOD Run() override {
    detail::RunnableFunctionCallHelper<void>::apply(mFunc, mArgs, std::index_sequence_for<Args...>{});
    return NS_OK;
  }

private:
  FunType mFunc;
  Tuple<Args...> mArgs;
};

template<typename FunType, typename... Args>
runnable_args_func<FunType, typename mozilla::Decay<Args>::Type...>*
WrapRunnableNM(FunType f, Args&&... args)
{
  return new runnable_args_func<FunType, typename mozilla::Decay<Args>::Type...>(f, std::forward<Args>(args)...);
}

template<typename Ret, typename FunType, typename... Args>
class runnable_args_func_ret : public detail::runnable_args_base<detail::ReturnsResult>
{
public:
  template<typename... Arguments>
  runnable_args_func_ret(Ret* ret, FunType f, Arguments&&... args)
    : mReturn(ret), mFunc(f), mArgs(std::forward<Arguments>(args)...)
  {}

  NS_IMETHOD Run() override {
    *mReturn = detail::RunnableFunctionCallHelper<Ret>::apply(mFunc, mArgs, std::index_sequence_for<Args...>{});
    return NS_OK;
  }

private:
  Ret* mReturn;
  FunType mFunc;
  Tuple<Args...> mArgs;
};

template<typename R, typename FunType, typename... Args>
runnable_args_func_ret<R, FunType, typename mozilla::Decay<Args>::Type...>*
WrapRunnableNMRet(R* ret, FunType f, Args&&... args)
{
  return new runnable_args_func_ret<R, FunType, typename mozilla::Decay<Args>::Type...>(ret, f, std::forward<Args>(args)...);
}

template<typename Class, typename M, typename... Args>
class runnable_args_memfn : public detail::runnable_args_base<detail::NoResult>
{
public:
  template<typename... Arguments>
  runnable_args_memfn(Class obj, M method, Arguments&&... args)
    : mObj(obj), mMethod(method), mArgs(std::forward<Arguments>(args)...)
  {}

  NS_IMETHOD Run() override {
    detail::RunnableMethodCallHelper<void>::apply(mObj, mMethod, mArgs, std::index_sequence_for<Args...>{});
    return NS_OK;
  }

private:
  Class mObj;
  M mMethod;
  Tuple<Args...> mArgs;
};

template<typename Class, typename M, typename... Args>
runnable_args_memfn<Class, M, typename mozilla::Decay<Args>::Type...>*
WrapRunnable(Class obj, M method, Args&&... args)
{
  return new runnable_args_memfn<Class, M, typename mozilla::Decay<Args>::Type...>(obj, method, std::forward<Args>(args)...);
}

template<typename Ret, typename Class, typename M, typename... Args>
class runnable_args_memfn_ret : public detail::runnable_args_base<detail::ReturnsResult>
{
public:
  template<typename... Arguments>
  runnable_args_memfn_ret(Ret* ret, Class obj, M method, Arguments... args)
    : mReturn(ret), mObj(obj), mMethod(method), mArgs(std::forward<Arguments>(args)...)
  {}

  NS_IMETHOD Run() override {
    *mReturn = detail::RunnableMethodCallHelper<Ret>::apply(mObj, mMethod, mArgs, std::index_sequence_for<Args...>{});
    return NS_OK;
  }

private:
  Ret* mReturn;
  Class mObj;
  M mMethod;
  Tuple<Args...> mArgs;
};

template<typename R, typename Class, typename M, typename... Args>
runnable_args_memfn_ret<R, Class, M, typename mozilla::Decay<Args>::Type...>*
WrapRunnableRet(R* ret, Class obj, M method, Args&&... args)
{
  return new runnable_args_memfn_ret<R, Class, M, typename mozilla::Decay<Args>::Type...>(ret, obj, method, std::forward<Args>(args)...);
}

static inline nsresult RUN_ON_THREAD(nsIEventTarget *thread, detail::runnable_args_base<detail::NoResult> *runnable, uint32_t flags) {
  return detail::RunOnThreadInternal(thread, static_cast<nsIRunnable *>(runnable), flags);
}

static inline nsresult
RUN_ON_THREAD(nsIEventTarget *thread, detail::runnable_args_base<detail::ReturnsResult> *runnable)
{
  return detail::RunOnThreadInternal(thread, static_cast<nsIRunnable *>(runnable), NS_DISPATCH_SYNC);
}

#ifdef DEBUG
#define ASSERT_ON_THREAD(t) do {                \
    if (t) {                                    \
      bool on;                                    \
      nsresult rv;                                \
      rv = t->IsOnCurrentThread(&on);             \
      MOZ_ASSERT(NS_SUCCEEDED(rv));               \
      MOZ_ASSERT(on);                             \
    }                                           \
  } while(0)
#else
#define ASSERT_ON_THREAD(t)
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
DispatchedRelease<T>* WrapRelease(already_AddRefed<T>&& ref)
{
  return new DispatchedRelease<T>(ref);
}

} /* namespace mozilla */

#endif
