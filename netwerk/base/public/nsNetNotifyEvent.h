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
#ifndef ___nsnetnotifyevent_h__
#define ___nsnetnotifyevent_h__

#include "nsIEventQueue.h"
#include "nscore.h"
#include "nsString2.h"

/* The nsNetNotifyEvent class is the top level class for the events in
 * networking libraries external module event firing mechanism. These events are
 * fired on the event queues supplied by modules external to the networking
 * library that wish to interract with the networking library.
 *
 * See mozilla/base/public/nsINetModuleMgr.idl for more info.
 */


class nsNetNotifyEvent : public PLEvent {
public:
    nsNetNotifyEvent(nsISupports* context);
    virtual ~nsNetNotifyEvent();

    nsresult Fire(nsIEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:

    // callback routines required by the EventQueue api. see plevent.h
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsISupports*                mContext;
};
