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
#ifndef ___nshttpnotifyevents_h__
#define ___nshttpnotifyevents_h__

#include "nsNetNotifyEvent.h"

#include "nsIEventQueue.h"
#include "nscore.h"
#include "nsString2.h"

/* These events comprise the networking libraries nsIHttpNotify implementation
 * detail. They are fired by the networking library and call the external
 * module's corresponding method. These events are fired synchronously.
 */

class nsHttpOnHeadersEvent: public nsNetNotifyEvent
{
public:
    nsHttpOnHeadersEvent(nsIProtocolConnection* aPConn, nsISupports* context);
    virtual ~nsHttpOnHeadersEvent();

    NS_IMETHOD HandleEvent();
};


class nsHttpSetHeadersEvent: public nsNetNotifyEvent
{
public:
    nsHttpSetHeadersEvent(nsIProtocolConnection* aPConn, nsISupports* context);
    virtual ~nsHttpSetHeadersEvent();

    NS_IMETHOD HandleEvent();
};

#endif // ___nshttpnotifyevents_h__
