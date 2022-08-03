/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5StreamListener_h
#define nsHtml5StreamListener_h

#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsHtml5StreamParser.h"
#include "mozilla/ReentrantMonitor.h"

/**
 * The purpose of this class is to reconcile the problem that
 * nsHtml5StreamParser is a cycle collection participant, which means that it
 * can only be refcounted on the main thread, but
 * nsIThreadRetargetableStreamListener can be refcounted from another thread,
 * so nsHtml5StreamParser being an nsIThreadRetargetableStreamListener was
 * a memory corruption problem.
 *
 * mDelegate is an nsHtml5StreamParserPtr, which releases the object that it
 * points to from a runnable on the main thread. DropDelegate() is only called
 * on the main thread. This call will finish before the main-thread derefs the
 * nsHtml5StreamListener itself, so there is no risk of another thread making
 * the refcount of nsHtml5StreamListener go to zero and running the destructor
 * concurrently. Other than that, the thread-safe nsISupports implementation
 * takes care of the destructor not running concurrently from different
 * threads, so there is no need to have a mutex around nsHtml5StreamParserPtr to
 * prevent it from double-releasing nsHtml5StreamParser.
 */
class nsHtml5StreamListener : public nsIStreamListener,
                              public nsIThreadRetargetableStreamListener {
 public:
  explicit nsHtml5StreamListener(nsHtml5StreamParser* aDelegate);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  // Main-thread-only
  nsHtml5StreamParser* GetDelegate();

  // Main-thread-only
  void DropDelegate();

 private:
  void DropDelegateImpl();
  virtual ~nsHtml5StreamListener();

  // ReentrantMonitor instead of Mutex, because `GetDelegate()`
  // can be called from within the Necko callbacks when Necko events
  // are delivered on the main thread.
  mozilla::ReentrantMonitor mDelegateMonitor MOZ_UNANNOTATED;
  // Owning pointer with manually-managed refcounting, protected by
  // mDelegateMonitor.   Access to it is Atomic, which avoids getting a lock
  // to check if it's set or to return the pointer.   Access to the data within
  // it needs the monitor.   MOZ_PT_GUARDED_BY() can't be used with Atomic<...*>
  mozilla::Atomic<nsHtml5StreamParser*, mozilla::ReleaseAcquire> mDelegate;
};

#endif  // nsHtml5StreamListener_h
