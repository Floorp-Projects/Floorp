/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsIAppShell.h"
#include <Pt.h>

/**
 * Native Photon Application shell wrapper
 */

class nsIEventQueueService;
class EventQueueTokenQueue;

class nsAppShell : public nsIAppShell
{
  public:
    nsAppShell(); 
    virtual           ~nsAppShell();

    NS_DECL_ISUPPORTS

    PRBool            OnPaint();

    // nsIAppShellInterface
  
    NS_IMETHOD        Create(int* argc, char ** argv);
    virtual nsresult  Run(); 
    NS_IMETHOD        Spinup();
    NS_IMETHOD        Spindown();
    NS_IMETHOD        ListenToEventQueue(nsIEventQueue *aQueue, PRBool aListen);
    NS_IMETHOD        GetNativeEvent(PRBool &aRealEvent, void *&aEvent);
    NS_IMETHOD        DispatchNativeEvent(PRBool aRealEvent, void * aEvent);

    NS_IMETHOD        Exit();
    NS_IMETHOD        SetDispatchListener(nsDispatchListener* aDispatchListener);

private:
    nsDispatchListener   *mDispatchListener;
	EventQueueTokenQueue *mEventQueueTokens;
    static PRBool        mPtInited;
    static int           mModalCount;
	
//  unsigned long        mEventBufferSz;
//  PhEvent_t            *mEvent;
//  nsIEventQueueService * mEventQService;
//	PRLock               *mLock;

};

#endif // nsAppShell_h__
