/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11func.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "PSMRunnable.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPKCS11Slot.h"
#include "nsProtectedAuthThread.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsProtectedAuthThread, nsIProtectedAuthThread)

static void PR_CALLBACK nsProtectedAuthThreadRunner(void *arg)
{
    nsProtectedAuthThread *self = static_cast<nsProtectedAuthThread *>(arg);
    self->Run();
}

nsProtectedAuthThread::nsProtectedAuthThread()
: mMutex("nsProtectedAuthThread.mMutex")
, mIAmRunning(false)
, mLoginReady(false)
, mThreadHandle(nsnull)
, mSlot(0)
, mLoginResult(SECFailure)
{
    NS_INIT_ISUPPORTS();
}

nsProtectedAuthThread::~nsProtectedAuthThread()
{
}

NS_IMETHODIMP nsProtectedAuthThread::Login(nsIObserver *aObserver)
{
    NS_ENSURE_ARG(aObserver);
    
    if (!mSlot)
        // We need pointer to the slot
        return NS_ERROR_FAILURE;

    MutexAutoLock lock(mMutex);
    
    if (mIAmRunning || mLoginReady) {
        return NS_OK;
    }

    if (aObserver) {
      // We must AddRef aObserver here on the main thread, because it probably
      // does not implement a thread-safe AddRef.
      mNotifyObserver = new NotifyObserverRunnable(aObserver,
                                                   "operation-completed");
    }

    mIAmRunning = true;
    
    mThreadHandle = PR_CreateThread(PR_USER_THREAD, nsProtectedAuthThreadRunner, static_cast<void*>(this), 
        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    
    // bool thread_started_ok = (threadHandle != nsnull);
    // we might want to return "thread started ok" to caller in the future
    NS_ASSERTION(mThreadHandle, "Could not create nsProtectedAuthThreadRunner thread\n");
    
    return NS_OK;
}

NS_IMETHODIMP nsProtectedAuthThread::GetTokenName(nsAString &_retval)
{
    MutexAutoLock lock(mMutex);

    // Get token name
    CopyUTF8toUTF16(nsDependentCString(PK11_GetTokenName(mSlot)), _retval);

    return NS_OK;
}

NS_IMETHODIMP nsProtectedAuthThread::GetSlot(nsIPKCS11Slot **_retval)
{
    nsRefPtr<nsPKCS11Slot> slot;
    {
        MutexAutoLock lock(mMutex);
        slot = new nsPKCS11Slot(mSlot);
    }
    if (!slot)
      return NS_ERROR_OUT_OF_MEMORY;

    return CallQueryInterface (slot.get(), _retval);
}

void nsProtectedAuthThread::SetParams(PK11SlotInfo* aSlot)
{
    MutexAutoLock lock(mMutex);

    mSlot = (aSlot) ? PK11_ReferenceSlot(aSlot) : 0;
}

SECStatus nsProtectedAuthThread::GetResult()
{
    return mLoginResult;
}

void nsProtectedAuthThread::Run(void)
{
    // Login with null password. This call will also do C_Logout() but 
    // it is harmless here
    mLoginResult = PK11_CheckUserPassword(mSlot, 0);

    nsCOMPtr<nsIRunnable> notifyObserver;
    {
        MutexAutoLock lock(mMutex);

        mLoginReady = true;
        mIAmRunning = false;

        // Forget the slot
        if (mSlot)
        {
            PK11_FreeSlot(mSlot);
            mSlot = 0;
        }

        notifyObserver.swap(mNotifyObserver);
    }
    
    if (notifyObserver) {
        nsresult rv = NS_DispatchToMainThread(notifyObserver);
	NS_ASSERTION(NS_SUCCEEDED(rv),
		     "failed to dispatch protected auth observer to main thread");
    }
}

void nsProtectedAuthThread::Join()
{
    if (!mThreadHandle)
        return;
    
    PR_JoinThread(mThreadHandle);
    mThreadHandle = nsnull;
}
