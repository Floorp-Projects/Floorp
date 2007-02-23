/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *sLog = PR_NewLogModule("nsThread");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

NS_DECL_CI_INTERFACE_GETTER(nsThread)

//-----------------------------------------------------------------------------
// Because we do not have our own nsIFactory, we have to implement nsIClassInfo
// somewhat manually.

class nsThreadClassInfo : public nsIClassInfo {
public:
  NS_DECL_ISUPPORTS_INHERITED  // no mRefCnt
  NS_DECL_NSICLASSINFO

  nsThreadClassInfo() {}
};

static nsThreadClassInfo sThreadClassInfo;

NS_IMETHODIMP_(nsrefcnt) nsThreadClassInfo::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsThreadClassInfo::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE1(nsThreadClassInfo, nsIClassInfo)

NS_IMETHODIMP
nsThreadClassInfo::GetInterfaces(PRUint32 *count, nsIID ***array)
{
  return NS_CI_INTERFACE_GETTER_NAME(nsThread)(count, array);
}

NS_IMETHODIMP
nsThreadClassInfo::GetHelperForLanguage(PRUint32 lang, nsISupports **result)
{
  *result = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetContractID(char **result)
{
  *result = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassDescription(char **result)
{
  *result = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassID(nsCID **result)
{
  *result = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetImplementationLanguage(PRUint32 *result)
{
  *result = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetFlags(PRUint32 *result)
{
  *result = THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassIDNoAlloc(nsCID *result)
{
  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ADDREF(nsThread)
NS_IMPL_THREADSAFE_RELEASE(nsThread)
NS_INTERFACE_MAP_BEGIN(nsThread)
  NS_INTERFACE_MAP_ENTRY(nsIThread)
  NS_INTERFACE_MAP_ENTRY(nsIThreadInternal)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIThread)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    foundInterface = NS_STATIC_CAST(nsIClassInfo*, &sThreadClassInfo);
  } else
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER4(nsThread, nsIThread, nsIThreadInternal,
                             nsIEventTarget, nsISupportsPriority)

//-----------------------------------------------------------------------------

class nsThreadStartupEvent : public nsRunnable {
public:
  // Create a new thread startup object.
  static nsThreadStartupEvent *Create() {
    nsThreadStartupEvent *startup = new nsThreadStartupEvent();
    if (startup && startup->mMon)
      return startup;
    // Allocation failure
    delete startup;
    return nsnull;
  }

  // This method does not return until the thread startup object is in the
  // completion state.
  void Wait() {
    if (mInitialized)  // Maybe avoid locking...
      return;
    nsAutoMonitor mon(mMon);
    while (!mInitialized)
      mon.Wait();
  }

  // This method needs to be public to support older compilers (xlC_r on AIX).
  // It should be called directly as this class type is reference counted.
  virtual ~nsThreadStartupEvent() {
    if (mMon)
      nsAutoMonitor::DestroyMonitor(mMon);
  }

private:
  NS_IMETHOD Run() {
    nsAutoMonitor mon(mMon);
    mInitialized = PR_TRUE;
    mon.Notify();
    return NS_OK;
  }

  nsThreadStartupEvent()
    : mMon(nsAutoMonitor::NewMonitor("xpcom.threadstartup"))
    , mInitialized(PR_FALSE) {
  }

  PRMonitor *mMon;
  PRBool     mInitialized;
};

//-----------------------------------------------------------------------------

struct nsThreadShutdownContext {
  nsThread *joiningThread;
  PRBool    shutdownAck;
};

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread.
class nsThreadShutdownAckEvent : public nsRunnable {
public:
  nsThreadShutdownAckEvent(nsThreadShutdownContext *ctx)
    : mShutdownContext(ctx) {
  }
  NS_IMETHOD Run() {
    mShutdownContext->shutdownAck = PR_TRUE;
    return NS_OK;
  }
private:
  nsThreadShutdownContext *mShutdownContext;
};

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public nsRunnable {
public:
  nsThreadShutdownEvent(nsThread *thr, nsThreadShutdownContext *ctx)
    : mThread(thr), mShutdownContext(ctx) {
  } 
  NS_IMETHOD Run() {
    mThread->mShutdownContext = mShutdownContext;
    return NS_OK;
  }
private:
  nsRefPtr<nsThread>       mThread;
  nsThreadShutdownContext *mShutdownContext;
};

//-----------------------------------------------------------------------------

/*static*/ void
nsThread::ThreadFunc(void *arg)
{
  nsThread *self = NS_STATIC_CAST(nsThread *, arg);  // strong reference
  self->mThread = PR_GetCurrentThread();

  // Inform the ThreadManager
  nsThreadManager::get()->RegisterCurrentThread(self);

  // Wait for and process startup event
  nsCOMPtr<nsIRunnable> event;
  if (!self->GetEvent(PR_TRUE, getter_AddRefs(event))) {
    NS_WARNING("failed waiting for thread startup event");
    return;
  }
  event->Run();  // unblocks nsThread::Init
  event = nsnull;

  // Now, process incoming events...
  while (!self->ShuttingDown())
    NS_ProcessNextEvent(self);

  NS_ProcessPendingEvents(self);

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get()->UnregisterCurrentThread(self);

  // Dispatch shutdown ACK
  event = new nsThreadShutdownAckEvent(self->mShutdownContext);
  self->mShutdownContext->joiningThread->Dispatch(event, NS_DISPATCH_NORMAL);

  NS_RELEASE(self);
}

//-----------------------------------------------------------------------------

nsThread::nsThread()
  : mLock(PR_NewLock())
  , mEvents(&mEventsRoot)
  , mPriority(PRIORITY_NORMAL)
  , mThread(nsnull)
  , mRunningEvent(0)
  , mShutdownContext(nsnull)
  , mShutdownRequired(PR_FALSE)
{
}

nsThread::~nsThread()
{
  if (mLock)
    PR_DestroyLock(mLock);
}

nsresult
nsThread::Init()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

  // spawn thread and wait until it is fully setup
  nsRefPtr<nsThreadStartupEvent> startup = nsThreadStartupEvent::Create();
  NS_ENSURE_TRUE(startup, NS_ERROR_OUT_OF_MEMORY);
 
  NS_ADDREF_THIS();
 
  mShutdownRequired = PR_TRUE;

  // ThreadFunc is responsible for setting mThread
  PRThread *thr = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                                  PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                  PR_JOINABLE_THREAD, 0);
  if (!thr) {
    NS_RELEASE_THIS();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // ThreadFunc will wait for this event to be run before it tries to access
  // mThread.  By delaying insertion of this event into the queue, we ensure
  // that mThread is set properly.
  {
    nsAutoLock lock(mLock);
    mEvents->PutEvent(startup);
  }

  // Wait for thread to call ThreadManager::SetupCurrentThread, which completes
  // initialization of ThreadFunc.
  startup->Wait();
  return NS_OK;
}

nsresult
nsThread::InitCurrentThread()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

  mThread = PR_GetCurrentThread();

  nsThreadManager::get()->RegisterCurrentThread(this);
  return NS_OK;
}

PRBool
nsThread::PutEvent(nsIRunnable *event)
{
  PRBool rv;
  {
    nsAutoLock lock(mLock);
    rv = mEvents->PutEvent(event);
  }
  if (!rv)
    return PR_FALSE;

  nsCOMPtr<nsIThreadObserver> obs = GetObserver();
  if (obs)
    obs->OnDispatchedEvent(this);

  return PR_TRUE;
}

//-----------------------------------------------------------------------------
// nsIEventTarget

NS_IMETHODIMP
nsThread::Dispatch(nsIRunnable *event, PRUint32 flags)
{
  LOG(("THRD(%p) Dispatch [%p %x]\n", this, event, flags));

  NS_ENSURE_ARG_POINTER(event);

  PRBool dispatched;
  if (flags & DISPATCH_SYNC) {
    nsThread *thread = nsThreadManager::get()->GetCurrentThread();
    NS_ENSURE_STATE(thread);

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.
 
    nsRefPtr<nsThreadSyncDispatch> wrapper =
        new nsThreadSyncDispatch(thread, event);
    if (!wrapper)
      return NS_ERROR_OUT_OF_MEMORY;
    dispatched = PutEvent(wrapper);

    while (wrapper->IsPending())
      NS_ProcessNextEvent(thread);
  } else {
    NS_ASSERTION(flags == NS_DISPATCH_NORMAL, "unexpected dispatch flags");
    dispatched = PutEvent(event);
  }

  if (NS_UNLIKELY(!dispatched))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsThread::IsOnCurrentThread(PRBool *result)
{
  *result = (PR_GetCurrentThread() == mThread);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIThread

NS_IMETHODIMP
nsThread::GetPRThread(PRThread **result)
{
  *result = mThread;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::Shutdown()
{
  LOG(("THRD(%p) shutdown\n", this));

  // XXX If we make this warn, then we hit that warning at xpcom shutdown while
  //     shutting down a thread in a thread pool.  That happens b/c the thread
  //     in the thread pool is already shutdown by the thread manager.
  if (!mThread)
    return NS_OK;

  NS_ENSURE_STATE(mThread != PR_GetCurrentThread());

  // Prevent multiple calls to this method
  {
    nsAutoLock lock(mLock);
    if (!mShutdownRequired)
      return NS_ERROR_UNEXPECTED;
    mShutdownRequired = PR_FALSE;
  }

  nsThreadShutdownContext context;
  context.joiningThread = nsThreadManager::get()->GetCurrentThread();
  context.shutdownAck = PR_FALSE;

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event = new nsThreadShutdownEvent(this, &context);
  if (!event)
    return NS_ERROR_OUT_OF_MEMORY;
  PutEvent(event);

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.
  
  // Process events on the current thread until we receive a shutdown ACK.
  while (!context.shutdownAck)
    NS_ProcessNextEvent(context.joiningThread);

  // Now, it should be safe to join without fear of dead-locking.

  PR_JoinThread(mThread);
  mThread = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingEvents(PRBool *result)
{
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  *result = mEvents->GetEvent(PR_FALSE, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::ProcessNextEvent(PRBool mayWait, PRBool *result)
{
  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, mayWait, mRunningEvent));

  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  nsCOMPtr<nsIThreadObserver> obs = mObserver;
  if (obs)
    obs->OnProcessNextEvent(this, mayWait && !ShuttingDown(), mRunningEvent);

  // If we are shutting down, then do not wait for new events.
  nsCOMPtr<nsIRunnable> event; 
  mEvents->GetEvent(mayWait && !ShuttingDown(), getter_AddRefs(event));

  *result = (event.get() != nsnull);

  nsresult rv = NS_OK;

  if (event) {
    LOG(("THRD(%p) running [%p]\n", this, event.get()));
    ++mRunningEvent;
    event->Run();
    --mRunningEvent;
  } else if (mayWait) {
    NS_ASSERTION(ShuttingDown(), "This should only happen when shutting down");
    rv = NS_ERROR_UNEXPECTED;
  }

  if (obs)
    obs->AfterProcessNextEvent(this, mRunningEvent);

  return rv;
}

//-----------------------------------------------------------------------------
// nsISupportsPriority

NS_IMETHODIMP
nsThread::GetPriority(PRInt32 *priority)
{
  *priority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(PRInt32 priority)
{
  NS_ENSURE_STATE(mThread);

  // NSPR defines the following four thread priorities:
  //   PR_PRIORITY_LOW
  //   PR_PRIORITY_NORMAL
  //   PR_PRIORITY_HIGH
  //   PR_PRIORITY_URGENT
  // We map the priority values defined on nsISupportsPriority to these values.

  mPriority = priority;

  PRThreadPriority pri;
  if (mPriority <= PRIORITY_HIGHEST) {
    pri = PR_PRIORITY_URGENT;
  } else if (mPriority < PRIORITY_NORMAL) {
    pri = PR_PRIORITY_HIGH;
  } else if (mPriority > PRIORITY_NORMAL) {
    pri = PR_PRIORITY_LOW;
  } else {
    pri = PR_PRIORITY_NORMAL;
  }
  PR_SetThreadPriority(mThread, pri);

  return NS_OK;
}

NS_IMETHODIMP
nsThread::AdjustPriority(PRInt32 delta)
{
  return SetPriority(mPriority + delta);
}

//-----------------------------------------------------------------------------
// nsIThreadInternal

NS_IMETHODIMP
nsThread::GetObserver(nsIThreadObserver **obs)
{
  nsAutoLock lock(mLock);
  NS_IF_ADDREF(*obs = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetObserver(nsIThreadObserver *obs)
{
  NS_ENSURE_STATE(PR_GetCurrentThread() == mThread);

  nsAutoLock lock(mLock);
  mObserver = obs;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::PushEventQueue(nsIThreadEventFilter *filter)
{
  nsChainedEventQueue *queue = new nsChainedEventQueue(filter);
  if (!queue || !queue->IsInitialized()) {
    delete queue;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoLock lock(mLock);
  queue->mNext = mEvents;
  mEvents = queue;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::PopEventQueue()
{
  nsAutoLock lock(mLock);

  // Make sure we do not pop too many!
  NS_ENSURE_STATE(mEvents != &mEventsRoot);

  nsChainedEventQueue *queue = mEvents;
  mEvents = mEvents->mNext;

  nsCOMPtr<nsIRunnable> event;
  while (queue->GetEvent(PR_FALSE, getter_AddRefs(event)))
    mEvents->PutEvent(event);

  delete queue;
  
  return NS_OK;
}

PRBool
nsThread::nsChainedEventQueue::PutEvent(nsIRunnable *event)
{
  PRBool val;
  if (!mFilter || mFilter->AcceptEvent(event)) {
    val = mQueue.PutEvent(event);
  } else {
    val = mNext->PutEvent(event);
  }
  return val;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsThreadSyncDispatch::Run()
{
  if (mSyncTask) {
    mSyncTask->Run();
    mSyncTask = nsnull;
    // unblock the origin thread
    mOrigin->Dispatch(this, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}
