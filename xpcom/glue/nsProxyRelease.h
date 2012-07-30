/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProxyRelease_h__
#define nsProxyRelease_h__

#include "nsIEventTarget.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

#ifdef XPCOM_GLUE_AVOID_NSPR
#error NS_ProxyRelease implementation depends on NSPR.
#endif

/**
 * Ensure that a nsCOMPtr is released on the target thread.
 *
 * @see NS_ProxyRelease(nsIEventTarget*, nsISupports*, bool)
 */
template <class T>
inline NS_HIDDEN_(nsresult)
NS_ProxyRelease
    (nsIEventTarget *target, nsCOMPtr<T> &doomed, bool alwaysProxy=false)
{
   T* raw = nullptr;
   doomed.swap(raw);
   return NS_ProxyRelease(target, raw, alwaysProxy);
}

/**
 * Ensure that a nsRefPtr is released on the target thread.
 *
 * @see NS_ProxyRelease(nsIEventTarget*, nsISupports*, bool)
 */
template <class T>
inline NS_HIDDEN_(nsresult)
NS_ProxyRelease
    (nsIEventTarget *target, nsRefPtr<T> &doomed, bool alwaysProxy=false)
{
   T* raw = nullptr;
   doomed.swap(raw);
   return NS_ProxyRelease(target, raw, alwaysProxy);
}

/**
 * Ensures that the delete of a nsISupports object occurs on the target thread.
 *
 * @param target
 *        the target thread where the doomed object should be released.
 * @param doomed
 *        the doomed object; the object to be released on the target thread.
 * @param alwaysProxy
 *        normally, if NS_ProxyRelease is called on the target thread, then the
 *        doomed object will released directly.  however, if this parameter is
 *        true, then an event will always be posted to the target thread for
 *        asynchronous release.
 */
NS_COM_GLUE nsresult
NS_ProxyRelease
    (nsIEventTarget *target, nsISupports *doomed, bool alwaysProxy=false);

/**
 * Class to safely handle main-thread-only pointers off the main thread.
 *
 * Classes like XPCWrappedJS are main-thread-only, which means that it is
 * forbidden to call methods on instances of these classes off the main thread.
 * For various reasons (see bug 771074), this restriction recently began to
 * apply to AddRef/Release as well.
 *
 * This presents a problem for consumers that wish to hold a callback alive
 * on non-main-thread code. A common example of this is the proxy callback
 * pattern, where non-main-thread code holds a strong-reference to the callback
 * object, and dispatches new Runnables (also with a strong reference) to the
 * main thread in order to execute the callback. This involves several AddRef
 * and Release calls on the other thread, which is (now) verboten.
 *
 * The basic idea of this class is to introduce a layer of indirection.
 * nsMainThreadPtrHolder is a threadsafe reference-counted class that internally
 * maintains one strong reference to the main-thread-only object. It must be
 * instantiated on the main thread (so that the AddRef of the underlying object
 * happens on the main thread), but consumers may subsequently pass references
 * to the holder anywhere they please. These references are meant to be opaque
 * when accessed off-main-thread (assertions enforce this).
 *
 * The semantics of nsRefPtr<nsMainThreadPtrHolder<T> > would be cumbersome, so
 * we also introduce nsMainThreadPtrHandle<T>, which is conceptually identical
 * to the above (though it includes various convenience methods). The basic
 * pattern is as follows.
 *
 * // On the main thread:
 * nsCOMPtr<nsIFooCallback> callback = ...;
 * nsMainThreadPtrHandle<nsIFooCallback> callbackHandle =
 *   new nsMainThreadPtrHolder<nsIFooCallback>(callback);
 * // Pass callbackHandle to structs/classes that might be accessed on other
 * // threads.
 *
 * All structs and classes that might be accessed on other threads should store
 * an nsMainThreadPtrHandle<T> rather than an nsCOMPtr<T>.
 */
template<class T>
class nsMainThreadPtrHolder
{
public:
  // We can only acquire a pointer on the main thread.
  nsMainThreadPtrHolder(T* ptr) : mRawPtr(NULL) {
    // We can only AddRef our pointer on the main thread, which means that the
    // holder must be constructed on the main thread.
    MOZ_ASSERT(NS_IsMainThread());
    NS_IF_ADDREF(mRawPtr = ptr);
  }

  // We can be released on any thread.
  ~nsMainThreadPtrHolder() {
    if (NS_IsMainThread()) {
      NS_IF_RELEASE(mRawPtr);
    } else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      if (!mainThread) {
        NS_WARNING("Couldn't get main thread! Leaking pointer.");
        return;
      }
      NS_ProxyRelease(mainThread, mRawPtr);
    }
  }

  T* get() {
    // Nobody should be touching the raw pointer off-main-thread.
    if (NS_UNLIKELY(!NS_IsMainThread()))
      MOZ_CRASH();
    return mRawPtr;
  }

  NS_IMETHOD_(nsrefcnt) Release();
  NS_IMETHOD_(nsrefcnt) AddRef();

private:
  // This class is threadsafe and reference-counted.
  nsAutoRefCnt mRefCnt;

  // Our wrapped pointer.
  T* mRawPtr;

  // Copy constructor and operator= not implemented. Once constructed, the
  // holder is immutable.
  T& operator=(nsMainThreadPtrHolder& other);
  nsMainThreadPtrHolder(const nsMainThreadPtrHolder& other);
};

template<class T>
NS_IMPL_THREADSAFE_ADDREF(nsMainThreadPtrHolder<T>)
template<class T>
NS_IMPL_THREADSAFE_RELEASE(nsMainThreadPtrHolder<T>)

template<class T>
class nsMainThreadPtrHandle
{
  nsRefPtr<nsMainThreadPtrHolder<T> > mPtr;

  public:
  nsMainThreadPtrHandle(nsMainThreadPtrHolder<T> *aHolder) : mPtr(aHolder) {}
  nsMainThreadPtrHandle(const nsMainThreadPtrHandle& aOther) : mPtr(aOther.mPtr) {}
  nsMainThreadPtrHandle& operator=(const nsMainThreadPtrHandle& aOther) {
    mPtr = aOther.mPtr;
  }

  operator nsMainThreadPtrHolder<T>*() { return mPtr.get(); }

  // These all call through to nsMainThreadPtrHolder, and thus implicitly
  // assert that we're on the main thread. Off-main-thread consumers must treat
  // these handles as opaque.
  T* get() { return mPtr.get()->get(); }
  operator T*() { return get(); }
  T* operator->() { return get(); }
};

#endif
