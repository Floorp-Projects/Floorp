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

#ifndef __nsProxyEvent_h_
#define __nsProxyEvent_h_

#include "nsISupports.h"
#include "plevent.h"

typedef void (*nsProxyMethodHandler)(nsISupports *, void *);

// this is a pretty generic structure, can probably be
// generalized by making nsISample into an nsISupports and
// making nsISampleMethod into an integer
struct nsProxyEvent {
    // must be the first entry in this structure so that
    // a nsProxyEvent* is compatible with a PLEvent*
    PLEvent e;
    nsIID* iid;                 // sanity check, make
                                // sure we have the right interface
    nsISupports *realObject;     // the non-proxy object that this
                                // event is referring to
    nsProxyMethodHandler methodHandler; // which method was called?
    void *paramBuffer;          // parameter buffer
};

// this is also pretty generic, can probably

nsProxyEvent *NewProxyEvent(PLEventQueue *, nsISupports *,
                            nsProxyMethodHandler, int);
nsresult PostProxyEvent(PLEventQueue *eventQueue,
                        nsProxyEvent *event);
void *ProxyEventHandler(PLEvent *self);
void ProxyDestroyHandler(PLEvent *self);

#define NS_DECL_PROXY(_class, _interface) \
public: \
  _class(PLEventQueue *, _interface *); \
private: \
  PLEventQueue* m_eventQueue; \
  _interface *m_realObject; \
public:

#define NS_IMPL_PROXY(_class, _interface)\
_class::_class(PLEventQueue *eventQueue, _interface *realObject) {\
  m_eventQueue = eventQueue;\
  m_realObject = realObject;\
}\


#endif
