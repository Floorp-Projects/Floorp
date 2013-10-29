// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_THREADSAFE_H_
#define BASE_OBSERVER_LIST_THREADSAFE_H_

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A thread-safe container for a list of observers.
//   This is similar to the observer_list (see observer_list.h), but it
//   is more robust for multi-threaded situations.
//
//   The following use cases are supported:
//    * Observers can register for notifications from any thread.
//      Callbacks to the observer will occur on the same thread where
//      the observer initially called AddObserver() from.
//    * Any thread may trigger a notification via Notify().
//    * Observers can remove themselves from the observer list inside
//      of a callback.
//    * If one thread is notifying observers concurrently with an observer
//      removing itself from the observer list, the notifications will
//      be silently dropped.
//
//   The drawback of the threadsafe observer list is that notifications
//   are not as real-time as the non-threadsafe version of this class.
//   Notifications will always be done via PostTask() to another thread,
//   whereas with the non-thread-safe observer_list, notifications happen
//   synchronously and immediately.
//
//   IMPLEMENTATION NOTES
//   The ObserverListThreadSafe maintains an ObserverList for each thread
//   which uses the ThreadSafeObserver.  When Notifying the observers,
//   we simply call PostTask to each registered thread, and then each thread
//   will notify its regular ObserverList.
//
///////////////////////////////////////////////////////////////////////////////

// Forward declaration for ObserverListThreadSafeTraits.
template <class ObserverType>
class ObserverListThreadSafe;

// An UnboundMethod is a wrapper for a method where the actual object is
// provided at Run dispatch time.
template <class T, class Method, class Params>
class UnboundMethod {
 public:
  UnboundMethod(Method m, const Params& p) : m_(m), p_(p) {
    COMPILE_ASSERT(
        (base::internal::ParamsUseScopedRefptrCorrectly<Params>::value),
        badunboundmethodparams);
  }
  void Run(T* obj) const {
    DispatchToMethod(obj, m_, p_);
  }
 private:
  Method m_;
  Params p_;
};

// This class is used to work around VS2005 not accepting:
//
// friend class
//     base::RefCountedThreadSafe<ObserverListThreadSafe<ObserverType> >;
//
// Instead of friending the class, we could friend the actual function
// which calls delete.  However, this ends up being
// RefCountedThreadSafe::DeleteInternal(), which is private.  So we
// define our own templated traits class so we can friend it.
template <class T>
struct ObserverListThreadSafeTraits {
  static void Destruct(const ObserverListThreadSafe<T>* x) {
    delete x;
  }
};

template <class ObserverType>
class ObserverListThreadSafe
    : public base::RefCountedThreadSafe<
        ObserverListThreadSafe<ObserverType>,
        ObserverListThreadSafeTraits<ObserverType> > {
 public:
  typedef typename ObserverList<ObserverType>::NotificationType
      NotificationType;

  ObserverListThreadSafe()
      : type_(ObserverListBase<ObserverType>::NOTIFY_ALL) {}
  explicit ObserverListThreadSafe(NotificationType type) : type_(type) {}

  // Add an observer to the list.  An observer should not be added to
  // the same list more than once.
  void AddObserver(ObserverType* obs) {
    // If there is not a current MessageLoop, it is impossible to notify on it,
    // so do not add the observer.
    if (!base::MessageLoop::current())
      return;

    ObserverList<ObserverType>* list = NULL;
    base::PlatformThreadId thread_id = base::PlatformThread::CurrentId();
    {
      base::AutoLock lock(list_lock_);
      if (observer_lists_.find(thread_id) == observer_lists_.end())
        observer_lists_[thread_id] = new ObserverListContext(type_);
      list = &(observer_lists_[thread_id]->list);
    }
    list->AddObserver(obs);
  }

  // Remove an observer from the list if it is in the list.
  // If there are pending notifications in-transit to the observer, they will
  // be aborted.
  // If the observer to be removed is in the list, RemoveObserver MUST
  // be called from the same thread which called AddObserver.
  void RemoveObserver(ObserverType* obs) {
    ObserverListContext* context = NULL;
    ObserverList<ObserverType>* list = NULL;
    base::PlatformThreadId thread_id = base::PlatformThread::CurrentId();
    {
      base::AutoLock lock(list_lock_);
      typename ObserversListMap::iterator it = observer_lists_.find(thread_id);
      if (it == observer_lists_.end()) {
        // This will happen if we try to remove an observer on a thread
        // we never added an observer for.
        return;
      }
      context = it->second;
      list = &context->list;

      // If we're about to remove the last observer from the list,
      // then we can remove this observer_list entirely.
      if (list->HasObserver(obs) && list->size() == 1)
        observer_lists_.erase(it);
    }
    list->RemoveObserver(obs);

    // If RemoveObserver is called from a notification, the size will be
    // nonzero.  Instead of deleting here, the NotifyWrapper will delete
    // when it finishes iterating.
    if (list->size() == 0)
      delete context;
  }

  // Verifies that the list is currently empty (i.e. there are no observers).
  void AssertEmpty() const {
    base::AutoLock lock(list_lock_);
    DCHECK(observer_lists_.empty());
  }

  // Notify methods.
  // Make a thread-safe callback to each Observer in the list.
  // Note, these calls are effectively asynchronous.  You cannot assume
  // that at the completion of the Notify call that all Observers have
  // been Notified.  The notification may still be pending delivery.
  template <class Method>
  void Notify(Method m) {
    UnboundMethod<ObserverType, Method, Tuple0> method(m, MakeTuple());
    Notify<Method, Tuple0>(method);
  }

  template <class Method, class A>
  void Notify(Method m, const A& a) {
    UnboundMethod<ObserverType, Method, Tuple1<A> > method(m, MakeTuple(a));
    Notify<Method, Tuple1<A> >(method);
  }

  template <class Method, class A, class B>
  void Notify(Method m, const A& a, const B& b) {
    UnboundMethod<ObserverType, Method, Tuple2<A, B> > method(
        m, MakeTuple(a, b));
    Notify<Method, Tuple2<A, B> >(method);
  }

  template <class Method, class A, class B, class C>
  void Notify(Method m, const A& a, const B& b, const C& c) {
    UnboundMethod<ObserverType, Method, Tuple3<A, B, C> > method(
        m, MakeTuple(a, b, c));
    Notify<Method, Tuple3<A, B, C> >(method);
  }

  template <class Method, class A, class B, class C, class D>
  void Notify(Method m, const A& a, const B& b, const C& c, const D& d) {
    UnboundMethod<ObserverType, Method, Tuple4<A, B, C, D> > method(
        m, MakeTuple(a, b, c, d));
    Notify<Method, Tuple4<A, B, C, D> >(method);
  }

  // TODO(mbelshe):  Add more wrappers for Notify() with more arguments.

 private:
  // See comment above ObserverListThreadSafeTraits' definition.
  friend struct ObserverListThreadSafeTraits<ObserverType>;

  struct ObserverListContext {
    explicit ObserverListContext(NotificationType type)
        : loop(base::MessageLoopProxy::current()),
          list(type) {
    }

    scoped_refptr<base::MessageLoopProxy> loop;
    ObserverList<ObserverType> list;

    DISALLOW_COPY_AND_ASSIGN(ObserverListContext);
  };

  ~ObserverListThreadSafe() {
    STLDeleteValues(&observer_lists_);
  }

  template <class Method, class Params>
  void Notify(const UnboundMethod<ObserverType, Method, Params>& method) {
    base::AutoLock lock(list_lock_);
    typename ObserversListMap::iterator it;
    for (it = observer_lists_.begin(); it != observer_lists_.end(); ++it) {
      ObserverListContext* context = (*it).second;
      context->loop->PostTask(
          FROM_HERE,
          base::Bind(&ObserverListThreadSafe<ObserverType>::
              template NotifyWrapper<Method, Params>, this, context, method));
    }
  }

  // Wrapper which is called to fire the notifications for each thread's
  // ObserverList.  This function MUST be called on the thread which owns
  // the unsafe ObserverList.
  template <class Method, class Params>
  void NotifyWrapper(ObserverListContext* context,
      const UnboundMethod<ObserverType, Method, Params>& method) {

    // Check that this list still needs notifications.
    {
      base::AutoLock lock(list_lock_);
      typename ObserversListMap::iterator it =
          observer_lists_.find(base::PlatformThread::CurrentId());

      // The ObserverList could have been removed already.  In fact, it could
      // have been removed and then re-added!  If the master list's loop
      // does not match this one, then we do not need to finish this
      // notification.
      if (it == observer_lists_.end() || it->second != context)
        return;
    }

    {
      typename ObserverList<ObserverType>::Iterator it(context->list);
      ObserverType* obs;
      while ((obs = it.GetNext()) != NULL)
        method.Run(obs);
    }

    // If there are no more observers on the list, we can now delete it.
    if (context->list.size() == 0) {
      {
        base::AutoLock lock(list_lock_);
        // Remove |list| if it's not already removed.
        // This can happen if multiple observers got removed in a notification.
        // See http://crbug.com/55725.
        typename ObserversListMap::iterator it =
            observer_lists_.find(base::PlatformThread::CurrentId());
        if (it != observer_lists_.end() && it->second == context)
          observer_lists_.erase(it);
      }
      delete context;
    }
  }

  // Key by PlatformThreadId because in tests, clients can attempt to remove
  // observers without a MessageLoop. If this were keyed by MessageLoop, that
  // operation would be silently ignored, leaving garbage in the ObserverList.
  typedef std::map<base::PlatformThreadId, ObserverListContext*>
      ObserversListMap;

  mutable base::Lock list_lock_;  // Protects the observer_lists_.
  ObserversListMap observer_lists_;
  const NotificationType type_;

  DISALLOW_COPY_AND_ASSIGN(ObserverListThreadSafe);
};

#endif  // BASE_OBSERVER_LIST_THREADSAFE_H_
