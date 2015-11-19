/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ASYNCINVOKER_H_
#define WEBRTC_BASE_ASYNCINVOKER_H_

#include "webrtc/base/asyncinvoker-inl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/scopedptrcollection.h"
#include "webrtc/base/thread.h"

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
//       invoker_.AsyncInvoke<int>(
//           thread, Bind(&MyClass::AsyncTaskWithResult, this, x),
//           &MyClass::OnTaskComplete, this);
//     }
//     void FireAnotherAsyncTask(Thread* thread) {
//       // No callback specified means fire-and-forget.
//       invoker_.AsyncInvoke<void>(
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
class AsyncInvoker : public MessageHandler {
 public:
  AsyncInvoker();
  ~AsyncInvoker() override;

  // Call |functor| asynchronously on |thread|, with no callback upon
  // completion. Returns immediately.
  template <class ReturnT, class FunctorT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   uint32 id = 0) {
    scoped_refptr<AsyncClosure> closure(
        new RefCountedObject<FireAndForgetAsyncClosure<FunctorT> >(functor));
    DoInvoke(thread, closure, id);
  }

  // Call |functor| asynchronously on |thread|, calling |callback| when done.
  template <class ReturnT, class FunctorT, class HostT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   void (HostT::*callback)(ReturnT),
                   HostT* callback_host,
                   uint32 id = 0) {
    scoped_refptr<AsyncClosure> closure(
        new RefCountedObject<NotifyingAsyncClosure<ReturnT, FunctorT, HostT> >(
            this, Thread::Current(), functor, callback, callback_host));
    DoInvoke(thread, closure, id);
  }

  // Call |functor| asynchronously on |thread|, calling |callback| when done.
  // Overloaded for void return.
  template <class ReturnT, class FunctorT, class HostT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   void (HostT::*callback)(),
                   HostT* callback_host,
                   uint32 id = 0) {
    scoped_refptr<AsyncClosure> closure(
        new RefCountedObject<NotifyingAsyncClosure<void, FunctorT, HostT> >(
            this, Thread::Current(), functor, callback, callback_host));
    DoInvoke(thread, closure, id);
  }

  // Synchronously execute on |thread| all outstanding calls we own
  // that are pending on |thread|, and wait for calls to complete
  // before returning. Optionally filter by message id.
  // The destructor will not wait for outstanding calls, so if that
  // behavior is desired, call Flush() before destroying this object.
  void Flush(Thread* thread, uint32 id = MQID_ANY);

  // Signaled when this object is destructed.
  sigslot::signal0<> SignalInvokerDestroyed;

 private:
  void OnMessage(Message* msg) override;
  void DoInvoke(Thread* thread, const scoped_refptr<AsyncClosure>& closure,
                uint32 id);

  bool destroying_;

  DISALLOW_COPY_AND_ASSIGN(AsyncInvoker);
};

}  // namespace rtc


#endif  // WEBRTC_BASE_ASYNCINVOKER_H_
