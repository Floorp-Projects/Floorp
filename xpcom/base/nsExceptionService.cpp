/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * ActiveState Tool Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <MarkH@ActiveState.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsISupports.h"
#include "nsExceptionService.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "prthread.h"
#include "prlock.h"
#include "mozilla/Services.h"

static const PRUintn BAD_TLS_INDEX = (PRUintn) -1;

#define CHECK_SERVICE_USE_OK() if (!lock) return NS_ERROR_NOT_INITIALIZED
#define CHECK_MANAGER_USE_OK() if (!mService || !nsExceptionService::lock) return NS_ERROR_NOT_INITIALIZED

// A key for our registered module providers hashtable
class nsProviderKey : public nsHashKey {
protected:
  PRUint32 mKey;
public:
  nsProviderKey(PRUint32 key) : mKey(key) {}
  PRUint32 HashCode(void) const {
    return mKey;
  }
  PRBool Equals(const nsHashKey *aKey) const {
    return mKey == ((const nsProviderKey *) aKey)->mKey;
  }
  nsHashKey *Clone() const {
    return new nsProviderKey(mKey);
  }
  PRUint32 GetValue() { return mKey; }
};

/** Exception Manager definition **/
class nsExceptionManager : public nsIExceptionManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTIONMANAGER

  nsExceptionManager(nsExceptionService *svc);
  /* additional members */
  nsCOMPtr<nsIException> mCurrentException;
  nsExceptionManager *mNextThread; // not ref-counted.
  nsExceptionService *mService; // not ref-counted
#ifdef NS_DEBUG
  static PRInt32 totalInstances;
#endif

private:
  ~nsExceptionManager();
};


#ifdef NS_DEBUG
PRInt32 nsExceptionManager::totalInstances = 0;
#endif

// Note this object is single threaded - the service itself ensures
// one per thread.
// An exception if the destructor, which may be called on
// the thread shutting down xpcom
NS_IMPL_THREADSAFE_ISUPPORTS1(nsExceptionManager, nsIExceptionManager)

nsExceptionManager::nsExceptionManager(nsExceptionService *svc) :
  mNextThread(nsnull),
  mService(svc)
{
  /* member initializers and constructor code */
#ifdef NS_DEBUG
  PR_ATOMIC_INCREMENT(&totalInstances);
#endif
}

nsExceptionManager::~nsExceptionManager()
{
  /* destructor code */
#ifdef NS_DEBUG
  PR_ATOMIC_DECREMENT(&totalInstances);
#endif // NS_DEBUG
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

PRUintn nsExceptionService::tlsIndex = BAD_TLS_INDEX;
PRLock *nsExceptionService::lock = nsnull;
nsExceptionManager *nsExceptionService::firstThread = nsnull;

#ifdef NS_DEBUG
PRInt32 nsExceptionService::totalInstances = 0;
#endif

NS_IMPL_THREADSAFE_ISUPPORTS3(nsExceptionService,
                              nsIExceptionService,
                              nsIExceptionManager,
                              nsIObserver)

nsExceptionService::nsExceptionService()
  : mProviders(4, PR_TRUE) /* small, thread-safe hashtable */
{
#ifdef NS_DEBUG
  if (PR_ATOMIC_INCREMENT(&totalInstances)!=1) {
    NS_ERROR("The nsExceptionService is a singleton!");
  }
#endif
  /* member initializers and constructor code */
  if (tlsIndex == BAD_TLS_INDEX) {
    PRStatus status;
    status = PR_NewThreadPrivateIndex( &tlsIndex, ThreadDestruct );
    NS_ASSERTION(status==0, "ScriptErrorService could not allocate TLS storage.");
  }
  lock = PR_NewLock();
  NS_ASSERTION(lock, "Error allocating ExceptionService lock");

  // observe XPCOM shutdown.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ASSERTION(observerService, "Could not get observer service!");
  if (observerService)
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}

nsExceptionService::~nsExceptionService()
{
  Shutdown();
  /* destructor code */
#ifdef NS_DEBUG
  PR_ATOMIC_DECREMENT(&totalInstances);
#endif
}

/*static*/
void nsExceptionService::ThreadDestruct( void *data )
{
  if (!lock) {
    NS_WARNING("nsExceptionService ignoring thread destruction after shutdown");
    return;
  }
  DropThread( (nsExceptionManager *)data );
}


void nsExceptionService::Shutdown()
{
  mProviders.Reset();
  if (lock) {
    DropAllThreads();
    PR_DestroyLock(lock);
    lock = nsnull;
  }
  PR_SetThreadPrivate(tlsIndex, nsnull);
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
    if (mgr == nsnull) {
        // Stick the new exception object in with no reference count.
        mgr = new nsExceptionManager(this);
        if (mgr == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        PR_SetThreadPrivate(tlsIndex, mgr);
        // The reference count is held in the thread-list
        AddThread(mgr);
    }
    *aCurrentScriptManager = mgr;
    NS_ADDREF(*aCurrentScriptManager);
    return NS_OK;
}

/* void registerErrorProvider (in nsIExceptionProvider provider, in PRUint32 moduleCode); */
NS_IMETHODIMP nsExceptionService::RegisterExceptionProvider(nsIExceptionProvider *provider, PRUint32 errorModule)
{
    CHECK_SERVICE_USE_OK();

    nsProviderKey key(errorModule);
    if (mProviders.Put(&key, provider)) {
        NS_WARNING("Registration of exception provider overwrote another provider with the same module code!");
    }
    return NS_OK;
}

/* void unregisterErrorProvider (in nsIExceptionProvider provider, in PRUint32 errorModule); */
NS_IMETHODIMP nsExceptionService::UnregisterExceptionProvider(nsIExceptionProvider *provider, PRUint32 errorModule)
{
    CHECK_SERVICE_USE_OK();
    nsProviderKey key(errorModule);
    if (!mProviders.Remove(&key)) {
        NS_WARNING("Attempt to unregister an unregistered exception provider!");
        return NS_ERROR_UNEXPECTED;
    }
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
    nsProviderKey key(NS_ERROR_GET_MODULE(errCode));
    nsCOMPtr<nsIExceptionProvider> provider =
        dont_AddRef((nsIExceptionProvider *)mProviders.Get(&key));

    // No provider so we'll return the default exception
    if (!provider) {
        *_exc = defaultException;
        NS_IF_ADDREF(*_exc);
        return NS_OK;
    }

    return provider->GetException(errCode, defaultException, _exc);
}

// thread management
/*static*/ void nsExceptionService::AddThread(nsExceptionManager *thread)
{
    PR_Lock(lock);
    thread->mNextThread = firstThread;
    firstThread = thread;
    NS_ADDREF(thread);
    PR_Unlock(lock);
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
    PR_Lock(lock);
    DoDropThread(thread);
    PR_Unlock(lock);
}

/*static*/ void nsExceptionService::DropAllThreads()
{
    PR_Lock(lock);
    while (firstThread)
        DoDropThread(firstThread);
    PR_Unlock(lock);
}
