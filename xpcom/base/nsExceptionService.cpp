/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExceptionService.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsDOMException.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsThreadUtils.h"
#include "pratom.h"
#include "prthread.h"

using namespace mozilla;

static const unsigned BAD_TLS_INDEX = (unsigned) -1;

#define CHECK_SERVICE_USE_OK() if (!sLock) return NS_ERROR_NOT_INITIALIZED
#define CHECK_MANAGER_USE_OK() if (!mService || !nsExceptionService::sLock) return NS_ERROR_NOT_INITIALIZED

/** Exception Manager definition **/
class nsExceptionManager MOZ_FINAL : public nsIExceptionManager
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEXCEPTIONMANAGER

  nsExceptionManager(nsExceptionService *svc);
  /* additional members */
  nsCOMPtr<nsIException> mCurrentException;
  nsExceptionManager *mNextThread; // not ref-counted.
  nsExceptionService *mService; // not ref-counted
#ifdef DEBUG
  static int32_t totalInstances;
#endif

private:
  ~nsExceptionManager();
};


#ifdef DEBUG
int32_t nsExceptionManager::totalInstances = 0;
#endif

// Note this object is single threaded - the service itself ensures
// one per thread.
// An exception if the destructor, which may be called on
// the thread shutting down xpcom
NS_IMPL_ISUPPORTS1(nsExceptionManager, nsIExceptionManager)

nsExceptionManager::nsExceptionManager(nsExceptionService *svc) :
  mNextThread(nullptr),
  mService(svc)
{
  /* member initializers and constructor code */
#ifdef DEBUG
  PR_ATOMIC_INCREMENT(&totalInstances);
#endif
}

nsExceptionManager::~nsExceptionManager()
{
  /* destructor code */
#ifdef DEBUG
  PR_ATOMIC_DECREMENT(&totalInstances);
#endif // DEBUG
}

/* void setCurrentException (in nsIException error); */
NS_IMETHODIMP nsExceptionManager::SetCurrentException(nsIException *error)
{
    CHECK_MANAGER_USE_OK();
    mCurrentException = error;
    return NS_OK;
}

/* nsIException getCurrentException (); */
NS_IMETHODIMP nsExceptionManager::GetCurrentException(nsIException **_retval)
{
    CHECK_MANAGER_USE_OK();
    *_retval = mCurrentException;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/* nsIException getExceptionFromProvider( in nsresult rc, in nsIException defaultException); */
NS_IMETHODIMP nsExceptionManager::GetExceptionFromProvider(nsresult rc, nsIException * defaultException, nsIException **_retval)
{
    CHECK_MANAGER_USE_OK();
    // Just delegate back to the service with the provider map.
    return mService->GetExceptionFromProvider(rc, defaultException, _retval);
}

/* The Exception Service */

unsigned nsExceptionService::tlsIndex = BAD_TLS_INDEX;
Mutex *nsExceptionService::sLock = nullptr;
nsExceptionManager *nsExceptionService::firstThread = nullptr;

#ifdef DEBUG
int32_t nsExceptionService::totalInstances = 0;
#endif

NS_IMPL_ISUPPORTS3(nsExceptionService,
                   nsIExceptionService,
                   nsIExceptionManager,
                   nsIObserver)

nsExceptionService::nsExceptionService()
{
#ifdef DEBUG
  if (PR_ATOMIC_INCREMENT(&totalInstances)!=1) {
    NS_ERROR("The nsExceptionService is a singleton!");
  }
#endif
  /* member initializers and constructor code */
  if (tlsIndex == BAD_TLS_INDEX) {
    DebugOnly<PRStatus> status;
    status = PR_NewThreadPrivateIndex( &tlsIndex, ThreadDestruct );
    NS_ASSERTION(status==0, "ScriptErrorService could not allocate TLS storage.");
  }
  sLock = new Mutex("nsExceptionService.sLock");

  // observe XPCOM shutdown.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ASSERTION(observerService, "Could not get observer service!");
  if (observerService)
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

nsExceptionService::~nsExceptionService()
{
  Shutdown();
  /* destructor code */
#ifdef DEBUG
  PR_ATOMIC_DECREMENT(&totalInstances);
#endif
}

/*static*/
void nsExceptionService::ThreadDestruct( void *data )
{
  if (!sLock) {
    NS_WARNING("nsExceptionService ignoring thread destruction after shutdown");
    return;
  }
  DropThread( (nsExceptionManager *)data );
}


void nsExceptionService::Shutdown()
{
  if (sLock) {
    DropAllThreads();
    delete sLock;
    sLock = nullptr;
  }
  PR_SetThreadPrivate(tlsIndex, nullptr);
}

/* void setCurrentException (in nsIException error); */
NS_IMETHODIMP nsExceptionService::SetCurrentException(nsIException *error)
{
    CHECK_SERVICE_USE_OK();
    nsCOMPtr<nsIExceptionManager> sm;
    nsresult nr = GetCurrentExceptionManager(getter_AddRefs(sm));
    if (NS_FAILED(nr))
        return nr;
    return sm->SetCurrentException(error);
}

/* nsIException getCurrentException (); */
NS_IMETHODIMP nsExceptionService::GetCurrentException(nsIException **_retval)
{
    CHECK_SERVICE_USE_OK();
    nsCOMPtr<nsIExceptionManager> sm;
    nsresult nr = GetCurrentExceptionManager(getter_AddRefs(sm));
    if (NS_FAILED(nr))
        return nr;
    return sm->GetCurrentException(_retval);
}

/* nsIException getExceptionFromProvider( in nsresult rc, in nsIException defaultException); */
NS_IMETHODIMP nsExceptionService::GetExceptionFromProvider(nsresult rc, 
    nsIException * defaultException, nsIException **_retval)
{
    CHECK_SERVICE_USE_OK();
    return DoGetExceptionFromProvider(rc, defaultException, _retval);
}

/* readonly attribute nsIExceptionManager currentExceptionManager; */
NS_IMETHODIMP nsExceptionService::GetCurrentExceptionManager(nsIExceptionManager * *aCurrentScriptManager)
{
    CHECK_SERVICE_USE_OK();
    nsExceptionManager *mgr = (nsExceptionManager *)PR_GetThreadPrivate(tlsIndex);
    if (mgr == nullptr) {
        // Stick the new exception object in with no reference count.
        mgr = new nsExceptionManager(this);
        PR_SetThreadPrivate(tlsIndex, mgr);
        // The reference count is held in the thread-list
        AddThread(mgr);
    }
    *aCurrentScriptManager = mgr;
    NS_ADDREF(*aCurrentScriptManager);
    return NS_OK;
}

// nsIObserver
NS_IMETHODIMP nsExceptionService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
     Shutdown();
     return NS_OK;
}

nsresult
nsExceptionService::DoGetExceptionFromProvider(nsresult errCode, 
                                               nsIException * defaultException,
                                               nsIException **_exc)
{
    // Check for an existing exception
    nsresult nr = GetCurrentException(_exc);
    if (NS_SUCCEEDED(nr) && *_exc) {
        (*_exc)->GetResult(&nr);
        // If it matches our result then use it
        if (nr == errCode)
            return NS_OK;
        NS_RELEASE(*_exc);
    }

    switch (NS_ERROR_GET_MODULE(errCode)) {
    case NS_ERROR_MODULE_DOM:
    case NS_ERROR_MODULE_SVG:
    case NS_ERROR_MODULE_DOM_XPATH:
    case NS_ERROR_MODULE_DOM_INDEXEDDB:
    case NS_ERROR_MODULE_DOM_FILEHANDLE:
        if (NS_IsMainThread()) {
            return NS_NewDOMException(errCode, defaultException, _exc);
        }
        break;

    default:
        break;
    }

    // No provider so we'll return the default exception
    *_exc = defaultException;
    NS_IF_ADDREF(*_exc);
    return NS_OK;
}

// thread management
/*static*/ void nsExceptionService::AddThread(nsExceptionManager *thread)
{
    MutexAutoLock lock(*sLock);
    thread->mNextThread = firstThread;
    firstThread = thread;
    NS_ADDREF(thread);
}

/*static*/ void nsExceptionService::DoDropThread(nsExceptionManager *thread)
{
    nsExceptionManager **emp = &firstThread;
    while (*emp != thread) {
        NS_ABORT_IF_FALSE(*emp, "Could not find the thread to drop!");
        emp = &(*emp)->mNextThread;
    }
    *emp = thread->mNextThread;
    NS_RELEASE(thread);
}

/*static*/ void nsExceptionService::DropThread(nsExceptionManager *thread)
{
    MutexAutoLock lock(*sLock);
    DoDropThread(thread);
}

/*static*/ void nsExceptionService::DropAllThreads()
{
    MutexAutoLock lock(*sLock);
    while (firstThread)
        DoDropThread(firstThread);
}
