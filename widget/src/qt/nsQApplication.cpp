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

#include "nsQApplication.h"
#include <qsocketnotifier.h>

nsQApplication::nsQApplication(int argc, char ** argv)
	: QApplication(argc, argv)
{
    mEventQueue = nsnull;
}

nsQApplication::~nsQApplication()
{
    NS_IF_RELEASE(mEventQueue);
}

void nsQApplication::SetEventProcessorCallback(nsIEventQueue * EQueue)
{
    mEventQueue = EQueue;
    NS_IF_ADDREF(mEventQueue);

    QSocketNotifier * sn = new QSocketNotifier(EQueue->GetEventQueueSelectFD(),
                                               QSocketNotifier::Read,
                                               this);
    connect(sn, 
            SIGNAL(activated(int)),
            this, 
            SLOT(DataReceived()));
}

void nsQApplication::DataReceived()
{
    if (mEventQueue)
    {
        mEventQueue->ProcessPendingEvents();
    }
}
