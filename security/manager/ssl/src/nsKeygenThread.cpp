/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 privateKey(nsnull),
 publicKey(nsnull),
 slot(nsnull),
 keyGenMechanism(0),
 params(nsnull),
 isPerm(false),
 isSensitive(false),
 wincx(nsnull),
 threadHandle(nsnull)
{
}

nsKeygenThread::~nsKeygenThread()
{
}

void nsKeygenThread::SetParams(
    PK11SlotInfo *a_slot,
    PRUint32 a_keyGenMechanism,
    void *a_params,
    bool a_isPerm,
    bool a_isSensitive,
    void *a_wincx )
{
  nsNSSShutDownPreventionLock locker;
  MutexAutoLock lock(mutex);
 
    if (!alreadyReceivedParams) {
      alreadyReceivedParams = true;
      if (a_slot) {
        slot = PK11_ReferenceSlot(a_slot);
      }
      else {
        slot = nsnull;
      }
      keyGenMechanism = a_keyGenMechanism;
      params = a_params;
      isPerm = a_isPerm;
      isSensitive = a_isSensitive;
      wincx = a_wincx;
    }
}

nsresult nsKeygenThread::GetParams(
    SECKEYPrivateKey **a_privateKey,
    SECKEYPublicKey **a_publicKey)
{
  if (!a_privateKey || !a_publicKey) {
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

      privateKey = 0;
      publicKey = 0;
      
      rv = NS_OK;
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
  
  return rv;
}

static void PR_CALLBACK nsKeygenThreadRunner(void *arg)
{
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

    // bool thread_started_ok = (threadHandle != nsnull);
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

  if (canGenerate)
    privateKey = PK11_GenerateKeyPair(slot, keyGenMechanism,
                                         params, &publicKey,
                                         isPerm, isSensitive, wincx);
  
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
    keyGenMechanism = 0;
    params = 0;
    wincx = 0;

    if (!statusDialogClosed && mNotifyObserver)
      notifyObserver = mNotifyObserver;

    mNotifyObserver = nsnull;
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
  threadHandle = nsnull;

  return;
}
