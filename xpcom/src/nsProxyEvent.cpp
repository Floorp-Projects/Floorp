/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsProxyEvent.h"
#include "prmem.h"

// thse are helper routines and are not nsISampleProxy specific
// you MUST call finishMarshalling() when this event is done,
// otherwise you'll lock the Event Queue monitor
nsProxyEvent *
NewProxyEvent(PLEventQueue *eventQueue, // queue where the event is going
              nsISupports *realObject, // real object this is a proxy for
              nsProxyMethodHandler methodHandler, // callback
              int bufferSize) { // marshaled method parameter buffer
    nsProxyEvent *event;
    
    // create the PLEvent and initialize it
    PR_ENTER_EVENT_QUEUE_MONITOR(eventQueue);
    
    // can we allocate memory and initialize all this stuff
    // before entering the monitor?
    event = PR_NEW(nsProxyEvent);
    if (event == NULL) return NULL;
    
    PL_InitEvent((PLEvent *)event, NULL,
                 ProxyEventHandler,
                 ProxyDestroyHandler);
    
    realObject->AddRef();
    event->realObject = realObject;
    event->methodHandler = methodHandler;
    event->paramBuffer = PR_Malloc(bufferSize);
    
    return event;
}

nsresult
PostProxyEvent(PLEventQueue *eventQueue,
               nsProxyEvent *event) {
    // post the event to the queue
    if (event)
        PL_PostEvent(eventQueue, (PLEvent *)event);
    PL_EXIT_EVENT_QUEUE_MONITOR(eventQueue);
    
    return NS_OK;
}

void* ProxyEventHandler(PLEvent *self) {
    nsProxyEvent *event = (nsProxyEvent *)self;
    event->methodHandler(event->realObject,
                         event->paramBuffer);
    return NULL;
}

void ProxyDestroyHandler(PLEvent *self) {
    nsProxyEvent *event = (nsProxyEvent *)self;
    
    NS_RELEASE(event->realObject);

    PR_FREEIF(event->paramBuffer);
    PR_FREEIF(event);
}
