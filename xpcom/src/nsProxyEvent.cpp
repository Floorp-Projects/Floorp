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

/*
 * nsProxyEventCreate
 * Returns a pointer to a data structure which will be placed on
 * the given event queue.
 * PLEventQueue *eventQueue - event queue of the remote thread
 * nsISupports  *realObject - the actual object which will recieve this event
 * nsProxyMethodHandler methodHandler - the callback which will be run
 *                            to demarshal the arguments
 * int           buffersize - the size of the buffer required to marshall
 *                            the arguments
 *
 * you MUST call PostProxyEvent() when this event is ready
 * otherwise you'll lock the Event Queue monitor
 */
nsProxyEvent *
nsProxyEventCreate(PLEventQueue *eventQueue, // queue where the event is going
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
                 nsProxyEventHandler,
                 nsProxyEventDestroyHandler);
    
    realObject->AddRef();
    event->realObject = realObject;
    event->methodHandler = methodHandler;
    event->destQueue = eventQueue;
    event->paramBuffer = PR_Malloc(bufferSize);
    
    return event;
}

/*
 * PostProxyEvent
 * This function posts the event to the Event Queue and returns immediately
 * all results and out parameters are lost
 * PLEventQueue *eventQueue - Event Queue to post this message to
 *                            (eventually this should be
 *                            inside the eventQueue)
 * nsProxyEvent *event      - Event to be posted
 */
nsresult
nsProxyEventPost(nsProxyEvent *event) {
    // post the event to the queue
    if (event)
        PL_PostEvent(event->destQueue, (PLEvent *)event);
    PL_EXIT_EVENT_QUEUE_MONITOR(event->destQueue);
    
    return NS_OK;
}


/*
 * ProxyEventHandler
 * 
 * this is the generic event handler callback which will be run
 * when the event is processed. This routine will will
 * break apart the nsProxyEvent to call the correct callback
 */
void* nsProxyEventHandler(PLEvent *self) {
    nsProxyEvent *event = (nsProxyEvent *)self;
    event->methodHandler(event->realObject,
                         event->paramBuffer);
    return NULL;
}

/*
 * ProxyDestroyHandler
 *
 * this is the generic destroy callback which will be run
 * after the event has been processed. This allows us to free
 * up the event structure and release references
 * 
 */
void nsProxyEventDestroyHandler(PLEvent *self) {
    nsProxyEvent *event = (nsProxyEvent *)self;
    
    NS_RELEASE(event->realObject);

    PR_FREEIF(event->paramBuffer);
    PR_FREEIF(event);
}
