/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#include "pk11func.h"
#include "nsCOMPtr.h"
#include "nsProxiedService.h"
#include "nsKeygenThread.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsKeygenThread, nsIKeygenThread)


nsKeygenThread::nsKeygenThread()
:mutex(nsnull),
 statusDialogPtr(nsnull),
 iAmRunning(PR_FALSE),
 keygenReady(PR_FALSE),
 statusDialogClosed(PR_FALSE),
 alreadyReceivedParams(PR_FALSE),
 privateKey(nsnull),
 publicKey(nsnull),
 slot(nsnull),
 keyGenMechanism(0),
 params(nsnull),
 isPerm(PR_FALSE),
 isSensitive(PR_FALSE),
 wincx(nsnull),
 threadHandle(nsnull)
{
  NS_INIT_ISUPPORTS();
  mutex = PR_NewLock();
}

nsKeygenThread::~nsKeygenThread()
{
  if (mutex) {
    PR_DestroyLock(mutex);
  }
  
  if (statusDialogPtr) {
    NS_RELEASE(statusDialogPtr);
  }
}

void nsKeygenThread::SetParams(
    PK11SlotInfo *a_slot,
    PRUint32 a_keyGenMechanism,
    void *a_params,
    PRBool a_isPerm,
    PRBool a_isSensitive,
    void *a_wincx )
{
  PR_Lock(mutex);
 
    if (!alreadyReceivedParams) {
      alreadyReceivedParams = PR_TRUE;
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

  PR_Unlock(mutex);
}

nsresult nsKeygenThread::GetParams(
    SECKEYPrivateKey **a_privateKey,
    SECKEYPublicKey **a_publicKey)
{
  if (!a_privateKey || !a_publicKey) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  
  PR_Lock(mutex);
  
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
  
  PR_Unlock(mutex);
  
  return rv;
}

static void PR_CALLBACK nsKeygenThreadRunner(void *arg)
{
  nsKeygenThread *self = NS_STATIC_CAST(nsKeygenThread *, arg);
  self->Run();
}

nsresult nsKeygenThread::StartKeyGeneration(nsIDOMWindowInternal *statusDialog)
{
  if (!mutex)
    return NS_OK;

  if (!statusDialog )
    return NS_OK;

  nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
  if (!proxyman)
    return NS_OK;

  nsCOMPtr<nsIDOMWindowInternal> wi;
  proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                               nsIDOMWindowInternal::GetIID(),
                               statusDialog,
                               PROXY_SYNC | PROXY_ALWAYS,
                               getter_AddRefs(wi));

  PR_Lock(mutex);

    if (iAmRunning || keygenReady) {
      PR_Unlock(mutex);
      return NS_OK;
    }

    statusDialogPtr = wi;
    NS_ADDREF(statusDialogPtr);
    wi = 0;

    iAmRunning = PR_TRUE;

    threadHandle = PR_CreateThread(PR_USER_THREAD, nsKeygenThreadRunner, NS_STATIC_CAST(void*, this), 
      PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    // bool thread_started_ok = (threadHandle != nsnull);
    // we might want to return "thread started ok" to caller in the future
    NS_ASSERTION(threadHandle, "Could not create nsKeygenThreadRunner thread\n");

  PR_Unlock(mutex);
  
  return NS_OK;
}

nsresult nsKeygenThread::UserCanceled(PRBool *threadAlreadyClosedDialog)
{
  threadAlreadyClosedDialog = PR_FALSE;

  if (!mutex)
    return NS_OK;

  PR_Lock(mutex);
  
    if (keygenReady)
      *threadAlreadyClosedDialog = statusDialogClosed;

    // User somehow closed the dialog, but we will not cancel.
    // Bad luck, we told him not do, and user still has to wait.
    // However, we remember that it's closed and will not close
    // it again to avoid problems.
    statusDialogClosed = PR_TRUE;

  PR_Unlock(mutex);

  return NS_OK;
}

void nsKeygenThread::Run(void)
{
  PRBool canGenerate = PR_FALSE;

  PR_Lock(mutex);

    if (alreadyReceivedParams) {
      canGenerate = PR_TRUE;
      keygenReady = PR_FALSE;
    }

  PR_Unlock(mutex);

  if (canGenerate)
    privateKey = PK11_GenerateKeyPair(slot, keyGenMechanism,
                                         params, &publicKey,
                                         isPerm, isSensitive, wincx);
  
  // This call gave us ownership over privateKey and publicKey.
  // But as the params structure is owner by our caller,
  // we effectively transferred ownership to the caller.
  // As long as key generation can't be canceled, we don't need 
  // to care for cleaning this up.

  nsIDOMWindowInternal *windowToClose = 0;

  PR_Lock(mutex);

    keygenReady = PR_TRUE;
    iAmRunning = PR_FALSE;

    // forget our parameters
    if (slot) {
      PK11_FreeSlot(slot);
      slot = 0;
    }
    keyGenMechanism = 0;
    params = 0;
    wincx = 0;

    if (!statusDialogClosed)
      windowToClose = statusDialogPtr;

    statusDialogPtr = 0;
    statusDialogClosed = PR_TRUE;

  PR_Unlock(mutex);

  if (windowToClose)
    windowToClose->Close();
}

void nsKeygenThread::Join()
{
  if (!threadHandle)
    return;
  
  PR_JoinThread(threadHandle);
  threadHandle = nsnull;

  return;
}
