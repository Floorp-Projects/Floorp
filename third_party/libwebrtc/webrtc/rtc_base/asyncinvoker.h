/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ASYNCINVOKER_H_
#define RTC_BASE_ASYNCINVOKER_H_

#include <atomic>
#include <memory>
#include <utility>

#include "rtc_base/asyncinvoker-inl.h"
#include "rtc_base/bind.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/thread.h"

namespace rtc {

// Invokes function objects (aka functors) asynchronously on a Thread, and
// owns the lifetime of calls (ie, when this object is destroyed, calls in
// flight are cancelled). AsyncInvoker can optionally execute a user-specified
// function when the asynchronous call is complete, or operates in
// fire-and-forget mode otherwise.
//
// AsyncInvoker does not own the thread it calls functors on.
//
// A note about async calls and object lifetimes: users should
// be mindful of object lifetimes when calling functions asynchronously and
// ensure objects used by the function _cannot_ be deleted between the
// invocation and execution of the functor. AsyncInvoker is designed to
// help: any calls in flight will be cancelled when the AsyncInvoker used to
// make the call is destructed, and any calls executing will be allowed to
// complete before AsyncInvoker destructs.
//
// The easiest way to ensure lifetimes are handled correctly is to create a
// class that owns the Thread and AsyncInvoker objects, and then call its
// methods asynchronously as needed.
//
// Example:
//   class MyClass {
//    public:
//     void FireAsyncTaskWithResult(Thread* thread, int x) {
//       // Specify a callback to get the result upon completion.
//       invoker_.AsyncInvoke<int>(RTC_FROM_HERE,
//           thread, Bind(&MyClass::AsyncTaskWithResult, this, x),
//           &MyClass::OnTaskComplete, this);
//     }
//     void FireAnotherAsyncTask(Thread* thread) {
//       // No callback specified means fire-and-forget.
//       invoker_.AsyncInvoke<void>(RTC_FROM_HERE,
//           thread, Bind(&MyClass::AnotherAsyncTask, this));
//
//    private:
//     int AsyncTaskWithResult(int x) {
//       // Some long running process...
//       return x * x;
//     }
//     void AnotherAsyncTask() {
//       // Some other long running process...
//     }
//     void OnTaskComplete(int result) { result_ = result; }
//
//     AsyncInvoker invoker_;
//     int result_;
//   };
//
// More details about threading:
// - It's safe to construct/destruct AsyncInvoker on different threads.
// - It's safe to call AsyncInvoke from different threads.
// - It's safe to call AsyncInvoke recursively from *within* a functor that's
//   being AsyncInvoked.
// - However, it's *not* safe to call AsyncInvoke from *outside* a functor
//   that's being AsyncInvoked while the AsyncInvoker is being destroyed on
//   another thread. This is just inherently unsafe and there's no way to
//   prevent that. So, the user of this class should ensure that the start of
//   each "chain" of invocations is synchronized somehow with the AsyncInvoker's
//   destruction. This can be done by starting each chain of invocations on the
//   same thread on which it will be destroyed, or by using some other
//   synchronization method.
class AsyncInvoker : public MessageHandler {
 public:
  AsyncInvoker();
  ~AsyncInvoker() override;

  // Call |functor| asynchronously on |thread|, with no callback upon
  // completion. Returns immediately.
  template <class ReturnT, class FunctorT>
  void AsyncInvoke(const Location& posted_from,
                   Thread* thread,
                   const FunctorT& functor,
                   uint32_t id = 0) {
    std::unique_ptr<AsyncClosure> closure(
        new FireAndForgetAsyncClosure<FunctorT>(this, functor));
    DoInvoke(posted_from, thread, std::move(closure), id);
  }

  // Call |functor| asynchronously on |thread| with |delay_ms|, with no callback
  // upon completion. Returns immediately.
  template <class ReturnT, class FunctorT>
  void AsyncInvokeDelayed(const Location& posted_from,
                          Thread* thread,
                          const FunctorT& functor,
                          uint32_t delay_ms,
                          uint32_t id = 0) {
    std::unique_ptr<AsyncClosure> closure(
        new FireAndForgetAsyncClosure<FunctorT>(this, functor));
    DoInvokeDelayed(posted_from, thread, std::move(closure), delay_ms, id);
  }

  // Synchronously execute on |thread| all outstanding calls we own
  // that are pending on |thread|, and wait for calls to complete
  // before returning. Optionally filter by message id.
  // The destructor will not wait for outstanding calls, so if that
  // behavior is desired, call Flush() before destroying this object.
  void Flush(Thread* thread, uint32_t id = MQID_ANY);

 private:
  void OnMessage(Message* msg) override;
  void DoInvoke(const Location& posted_from,
                Thread* thread,
                std::unique_ptr<AsyncClosure> closure,
                uint32_t id);
  void DoInvokeDelayed(const Location& posted_from,
                       Thread* thread,
                       std::unique_ptr<AsyncClosure> closure,
                       uint32_t delay_ms,
                       uint32_t id);

  // Used to keep track of how many invocations (AsyncClosures) are still
  // alive, so that the destructor can wait for them to finish, as described in
  // the class documentation.
  //
  // TODO(deadbeef): Using a raw std::atomic like this is prone to error and
  // difficult to maintain. We should try to wrap this functionality in a
  // separate class to reduce the chance of errors being introduced in the
  // future.
  std::atomic<int> pending_invocations_;

  // Reference counted so that if the AsyncInvoker destructor finishes before
  // an AsyncClosure's destructor that's about to call
  // "invocation_complete_->Set()", it's not dereferenced after being
  // destroyed.
  scoped_refptr<RefCountedObject<Event>> invocation_complete_;

  // This flag is used to ensure that if an application AsyncInvokes tasks that
  // recursively AsyncInvoke other tasks ad infinitum, the cycle eventually
  // terminates.
  std::atomic<bool> destroying_;

  friend class AsyncClosure;

  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncInvoker);
};

// Similar to AsyncInvoker, but guards against the Thread being destroyed while
// there are outstanding dangling pointers to it. It will connect to the current
// thread in the constructor, and will get notified when that thread is
// destroyed. After GuardedAsyncInvoker is constructed, it can be used from
// other threads to post functors to the thread it was constructed on. If that
// thread dies, any further calls to AsyncInvoke() will be safely ignored.
class GuardedAsyncInvoker : public sigslot::has_slots<> {
 public:
  GuardedAsyncInvoker();
  ~GuardedAsyncInvoker() override;

  // Synchronously execute all outstanding calls we own, and wait for calls to
  // complete before returning. Optionally filter by message id. The destructor
  // will not wait for outstanding calls, so if that behavior is desired, call
  // Flush() first. Returns false if the thread has died.
  bool Flush(uint32_t id = MQID_ANY);

  // Call |functor| asynchronously with no callback upon completion. Returns
  // immediately. Returns false if the thread has died.
  template <class ReturnT, class FunctorT>
  bool AsyncInvoke(const Location& posted_from,
                   const FunctorT& functor,
                   uint32_t id = 0) {
    CritScope cs(&crit_);
    if (thread_ == nullptr)
      return false;
    invoker_.AsyncInvoke<ReturnT, FunctorT>(posted_from, thread_, functor, id);
    return true;
  }

  // Call |functor| asynchronously with |delay_ms|, with no callback upon
  // completion. Returns immediately. Returns false if the thread has died.
  template <class ReturnT, class FunctorT>
  bool AsyncInvokeDelayed(const Location& posted_from,
                          const FunctorT& functor,
                          uint32_t delay_ms,
                          uint32_t id = 0) {
    CritScope cs(&crit_);
    if (thread_ == nullptr)
      return false;
    invoker_.AsyncInvokeDelayed<ReturnT, FunctorT>(posted_from, thread_,
                                                   functor, delay_ms, id);
    return true;
  }

  // Call |functor| asynchronously, calling |callback| when done. Returns false
  // if the thread has died.
  template <class ReturnT, class FunctorT, class HostT>
  bool AsyncInvoke(const Location& posted_from,
                   const Location& callback_posted_from,
                   const FunctorT& functor,
                   void (HostT::*callback)(ReturnT),
                   HostT* callback_host,
                   uint32_t id = 0) {
    CritScope cs(&crit_);
    if (thread_ == nullptr)
      return false;
    invoker_.AsyncInvoke<ReturnT, FunctorT, HostT>(
        posted_from, callback_posted_from, thread_, functor, callback,
        callback_host, id);
    return true;
  }

  // Call |functor| asynchronously calling |callback| when done. Overloaded for
  // void return. Returns false if the thread has died.
  template <class ReturnT, class FunctorT, class HostT>
  bool AsyncInvoke(const Location& posted_from,
                   const Location& callback_posted_from,
                   const FunctorT& functor,
                   void (HostT::*callback)(),
                   HostT* callback_host,
                   uint32_t id = 0) {
    CritScope cs(&crit_);
    if (thread_ == nullptr)
      return false;
    invoker_.AsyncInvoke<ReturnT, FunctorT, HostT>(
        posted_from, callback_posted_from, thread_, functor, callback,
        callback_host, id);
    return true;
  }

 private:
  // Callback when |thread_| is destroyed.
  void ThreadDestroyed();

  CriticalSection crit_;
  Thread* thread_ RTC_GUARDED_BY(crit_);
  AsyncInvoker invoker_ RTC_GUARDED_BY(crit_);
};

}  // namespace rtc

#endif  // RTC_BASE_ASYNCINVOKER_H_
