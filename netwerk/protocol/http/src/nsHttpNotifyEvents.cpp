/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsHttpNotifyEvents.h"

#include "nscore.h"
#include "nsIString.h"


////////////////////////////////////////////////////////////////////////////////
//
// OnHeaders...
//
////////////////////////////////////////////////////////////////////////////////

nsHttpOnHeadersEvent::nsHttpOnHeadersEvent(nsIProtocolConnection* aPConn,
                                             nsISupports* context)
    : mContext(context) {

    NS_IF_ADDREF(mContext);
}

nsHttpOnHeadersEvent::~nsHttpOnHeadersEvent() {
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsHttpOnHeadersEvent::HandlePLEvent(PLEvent* aEvent) {

}

void PR_CALLBACK nsHttpOnHeadersEvent::DestroyPLEvent(PLEvent* aEvent) {

}

nsresult nsHttpOnHeadersEvent::Fire(nsIEventQueue* aEventQueue) {
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsNetNotifyEVent::HandlePLEvent,
                 (PLDestroyEventProc) nsNetNotifyEVent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////////////
//
// SetHeaders...
//
////////////////////////////////////////////////////////////////////////////////
nsHttpSetHeadersEvent::nsHttpSetHeadersEvent(nsIProtocolConnection* aPConn,
                                             nsISupports* context)
    : mContext(context) {

    NS_IF_ADDREF(mContext);
}

nsHttpSetHeadersEvent::~nsHttpSetHeadersEvent() {
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsHttpSetHeadersEvent::HandlePLEvent(PLEvent* aEvent) {

}

void PR_CALLBACK nsHttpSetHeadersEvent::DestroyPLEvent(PLEvent* aEvent) {

}

nsresult nsHttpSetHeadersEvent::Fire(nsIEventQueue* aEventQueue) {
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsNetNotifyEVent::HandlePLEvent,
                 (PLDestroyEventProc) nsNetNotifyEVent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}