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
#include "nsIWidget.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsKeygenThread, nsIKeygenThread)


nsKeygenThread::nsKeygenThread()
:mutex(nsnull),
 statusDialogPtr(nsnull),
 iAmRunning(PR_FALSE),
 keygenReady(PR_FALSE),
 statusDialogClosed(PR_FALSE),
 threadHandle(nsnull),
 params(nsnull)
{
  mutex = PR_NewLock();
}

nsKeygenThread::~nsKeygenThread()
{
  if (mutex)
    PR_DestroyLock(mutex);
}

void nsKeygenThread::SetParams(GenerateKeypairParameters *p)
{
  PR_Lock(mutex);
  params = p;
  PR_Unlock(mutex);
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

    statusDialogPtr = wi;
    NS_ADDREF(statusDialogPtr);
    wi = 0;

    if (iAmRunning || keygenReady) {
      PR_Unlock(mutex);
      return NS_OK;
    }

    iAmRunning = PR_TRUE;

    threadHandle = PR_CreateThread(PR_USER_THREAD, run, NS_STATIC_CAST(void*, this), 
      PR_PRIORITY_LOW, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    // bool thread_started_ok = (threadHandle != nsnull);
    // we might want to return "thread started ok" to caller in the future

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

    NS_RELEASE(statusDialogPtr);

  PR_Unlock(mutex);

  return NS_OK;
}

void nsKeygenThread::run(void *args)
{
  if (!args)
    return;

  nsKeygenThread *self = NS_STATIC_CAST(nsKeygenThread *, args);
  if (!self)
    return;
  
  GenerateKeypairParameters *p = 0;

  PR_Lock(self->mutex);

    if (self->params) {
      p = self->params;
      // Make sure it's impossible that will use the same parameters again.
      self->params = 0;
    }

  PR_Unlock(self->mutex);

  if (p)
    p->privateKey = PK11_GenerateKeyPair(p->slot, p->keyGenMechanism,
                                         p->params, &p->publicKey,
                                         PR_TRUE, PR_TRUE, nsnull);
  
  // This call gave us ownership over privateKey and publicKey.
  // But as the params structure is owner by our caller,
  // we effectively transferred ownership to the caller.
  // As long as key generation can't be canceled, we don't need 
  // to care for cleaning this up.

  nsIDOMWindowInternal *windowToClose = 0;

  PR_Lock(self->mutex);

    self->keygenReady = PR_TRUE;
    self->iAmRunning = PR_FALSE;

    if (!self->statusDialogClosed)
      windowToClose = self->statusDialogPtr;

    self->statusDialogPtr = 0;
    self->statusDialogClosed = PR_TRUE;

  PR_Unlock(self->mutex);

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
