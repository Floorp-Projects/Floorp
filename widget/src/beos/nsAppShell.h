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

#include "nsCOMPtr.h"
#include "nsObject.h"
#include "nsIAppShell.h"
#include "nsIEventQueue.h"
#include "nsSwitchToUIThread.h"
#include <OS.h>
#include <List.h>

/**
 * Native BeOS Application shell wrapper
 */

class nsAppShell : public nsIAppShell
{
  public:
                            nsAppShell(); 
    virtual                 ~nsAppShell();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAPPSHELL

    virtual void*	GetNativeData(PRUint32 aDataType);

  private:
    nsCOMPtr<nsIEventQueue> mEventQueue;

    // event priorities
    enum {
      PRIORITY_TOP = 0,
      PRIORITY_SECOND,
      PRIORITY_THIRD,
      PRIORITY_NORMAL,
      PRIORITY_LOW,
      PRIORITY_LEVELS = 5
    };
    
    void ConsumeRedundantMouseMoveEvent(MethodInfo *pNewEventMInfo);
    void RetrieveAllEvents(bool blockable);
    int CountStoredEvents();
    void *GetNextEvent();
    
    nsDispatchListener	*mDispatchListener;
    port_id					eventport;
    sem_id					syncsem;
    BList events[PRIORITY_LEVELS];

    bool is_port_error;
};

#endif // nsAppShell_h__
