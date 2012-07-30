/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11func.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsKeygenThread.h"
#include "nsIObserver.h"
#include "nsNSSShutDown.h"
#include "PSMRunnable.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsKeygenThread, nsIKeygenThread)


nsKeygenThread::nsKeygenThread()
:mutex("nsKeygenThread.mutex"),
 iAmRunning(false),
 keygenReady(false),
 statusDialogClosed(false),
 alreadyReceivedParams(false),
 privateKey(nullptr),
 publicKey(nullptr),
 slot(nullptr),
 flags(0),
 altSlot(nullptr),
 altFlags(0),
 usedSlot(nullptr),
 keyGenMechanism(0),
 params(nullptr),
 wincx(nullptr),
 threadHandle(nullptr)
{
}

nsKeygenThread::~nsKeygenThread()
{
  // clean up in the unlikely case that nobody consumed our results
  
  if (privateKey)
    SECKEY_DestroyPrivateKey(privateKey);
    
  if (publicKey)
    SECKEY_DestroyPublicKey(publicKey);
    
  if (usedSlot)
    PK11_FreeSlot(usedSlot);
}

void nsKeygenThread::SetParams(
    PK11SlotInfo *a_slot,
    PK11AttrFlags a_flags,
    PK11SlotInfo *a_alternative_slot,
    PK11AttrFlags a_alternative_flags,
    PRUint32 a_keyGenMechanism,
    void *a_params,
    void *a_wincx )
{
  nsNSSShutDownPreventionLock locker;
  MutexAutoLock lock(mutex);
 
    if (!alreadyReceivedParams) {
      alreadyReceivedParams = true;
      slot = (a_slot) ? PK11_ReferenceSlot(a_slot) : nullptr;
      flags = a_flags;
      altSlot = (a_alternative_slot) ? PK11_ReferenceSlot(a_alternative_slot) : nullptr;
      altFlags = a_alternative_flags;
      keyGenMechanism = a_keyGenMechanism;
      params = a_params;
      wincx = a_wincx;
    }
}

nsresult nsKeygenThread::ConsumeResult(
    PK11SlotInfo **a_used_slot,
    SECKEYPrivateKey **a_privateKey,
    SECKEYPublicKey **a_publicKey)
{
  if (!a_used_slot || !a_privateKey || !a_publicKey) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  MutexAutoLock lock(mutex);
  
    // GetParams must not be called until thread creator called
    // Join on this thread.
    NS_ASSERTION(keygenReady, "logic error in nsKeygenThread::GetParams");

    if (keygenReady) {
      *a_privateKey = privateKey;
      *a_publicKey = publicKey;
      *a_used_slot = usedSlot;

      privateKey = 0;
      publicKey = 0;
      usedSlot = 0;
      
      rv = NS_OK;
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
  
  return rv;
}

static void PR_CALLBACK nsKeygenThreadRunner(void *arg)
{
  PR_SetCurrentThreadName("Keygen");
  nsKeygenThread *self = static_cast<nsKeygenThread *>(arg);
  self->Run();
}

nsresult nsKeygenThread::StartKeyGeneration(nsIObserver* aObserver)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsKeygenThread::StartKeyGeneration called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }
  
  if (!aObserver)
    return NS_OK;

  MutexAutoLock lock(mutex);

    if (iAmRunning || keygenReady) {
      return NS_OK;
    }

    // We must AddRef aObserver only here on the main thread, because it
    // probably does not implement a thread-safe AddRef.
    mNotifyObserver = new NotifyObserverRunnable(aObserver, "keygen-finished");

    iAmRunning = true;

    threadHandle = PR_CreateThread(PR_USER_THREAD, nsKeygenThreadRunner, static_cast<void*>(this), 
      PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    // bool thread_started_ok = (threadHandle != nullptr);
    // we might want to return "thread started ok" to caller in the future
    NS_ASSERTION(threadHandle, "Could not create nsKeygenThreadRunner thread\n");
  
  return NS_OK;
}

nsresult nsKeygenThread::UserCanceled(bool *threadAlreadyClosedDialog)
{
  if (!threadAlreadyClosedDialog)
    return NS_OK;

  *threadAlreadyClosedDialog = false;

  MutexAutoLock lock(mutex);
  
    if (keygenReady)
      *threadAlreadyClosedDialog = statusDialogClosed;

    // User somehow closed the dialog, but we will not cancel.
    // Bad luck, we told him not do, and user still has to wait.
    // However, we remember that it's closed and will not close
    // it again to avoid problems.
    statusDialogClosed = true;

  return NS_OK;
}

void nsKeygenThread::Run(void)
{
  nsNSSShutDownPreventionLock locker;
  bool canGenerate = false;

  {
    MutexAutoLock lock(mutex);
    if (alreadyReceivedParams) {
      canGenerate = true;
      keygenReady = false;
    }
  }

  if (canGenerate) {
    privateKey = PK11_GenerateKeyPairWithFlags(slot, keyGenMechanism,
                                               params, &publicKey,
                                               flags, wincx);

    if (privateKey) {
      usedSlot = PK11_ReferenceSlot(slot);
    }
    else if (altSlot) {
      privateKey = PK11_GenerateKeyPairWithFlags(altSlot, keyGenMechanism,
                                                 params, &publicKey,
                                                 altFlags, wincx);
      if (privateKey) {
        usedSlot = PK11_ReferenceSlot(altSlot);
      }
    }
  }
  
  // This call gave us ownership over privateKey and publicKey.
  // But as the params structure is owner by our caller,
  // we effectively transferred ownership to the caller.
  // As long as key generation can't be canceled, we don't need 
  // to care for cleaning this up.

  nsCOMPtr<nsIRunnable> notifyObserver;
  {
    MutexAutoLock lock(mutex);

    keygenReady = true;
    iAmRunning = false;

    // forget our parameters
    if (slot) {
      PK11_FreeSlot(slot);
      slot = 0;
    }
    if (altSlot) {
      PK11_FreeSlot(altSlot);
      altSlot = 0;
    }
    keyGenMechanism = 0;
    params = 0;
    wincx = 0;

    if (!statusDialogClosed && mNotifyObserver)
      notifyObserver = mNotifyObserver;

    mNotifyObserver = nullptr;
  }

  if (notifyObserver) {
    nsresult rv = NS_DispatchToMainThread(notifyObserver);
    NS_ASSERTION(NS_SUCCEEDED(rv),
		 "failed to dispatch keygen thread observer to main thread");
  }
}

void nsKeygenThread::Join()
{
  if (!threadHandle)
    return;
  
  PR_JoinThread(threadHandle);
  threadHandle = nullptr;

  return;
}
