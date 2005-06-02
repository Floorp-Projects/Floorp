/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the mozilla.org.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
 *   Robert Relyea     <rrelyea@redhat.com>
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
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNSSEvent.h"
#include "plevent.h"
#include "nsIRunnable.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

//
// This is the class we'll use to post ui events
//
struct nsNSSEventRunnable : PLEvent {
  nsNSSEventRunnable(nsIRunnable* runnable);
  ~nsNSSEventRunnable();

  nsCOMPtr<nsIRunnable> mRunnable;
};

//Grab the UI event queue so that we can post some events to it.
already_AddRefed<nsIEventQueue>
nsNSSEventGetUIEventQueue()
{
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> service = 
                        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsIEventQueue* result = nsnull;
  rv = service->GetThreadEventQueue(NS_UI_THREAD, &result);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  return result;
}

// post something to it
nsresult
nsNSSEventPostToUIEventQueue(nsIRunnable *event)
{
  nsNSSEventRunnable *runnable = new nsNSSEventRunnable(event);
  if (!runnable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIEventQueue>uiQueue = nsNSSEventGetUIEventQueue();
  uiQueue->PostEvent(runnable);
  return NS_OK;
}

//A wrapper for PLEvent that we can use to post
//our nsIRunnable Events.
static void PR_CALLBACK
handleNSSEventRunnable(nsNSSEventRunnable* aEvent)
{
  aEvent->mRunnable->Run();
}

static void PR_CALLBACK
destroyNSSEventRunnable(nsNSSEventRunnable* aEvent)
{
  delete aEvent;
}

nsNSSEventRunnable::nsNSSEventRunnable(nsIRunnable* runnable)
  :  mRunnable(runnable)
{
  PL_InitEvent(this, nsnull, PLHandleEventProc(handleNSSEventRunnable),
               PLDestroyEventProc(&destroyNSSEventRunnable));
}

nsNSSEventRunnable::~nsNSSEventRunnable()
{
}

