/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsQApplication.h"
#include <qsocketnotifier.h>

nsQApplication::nsQApplication(int argc, char ** argv)
	: QApplication(argc, argv)
{
    mEventQueue = nsnull;
}

nsQApplication::nsQApplication(Display * display)
	: QApplication(display)
{
    mEventQueue = nsnull;
    setGlobalMouseTracking(true);
}

nsQApplication::~nsQApplication()
{
    setGlobalMouseTracking(false);
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
