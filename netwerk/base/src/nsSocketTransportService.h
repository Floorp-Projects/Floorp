/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsSocketTransportService_h___
#define nsSocketTransportService_h___

#include "nspr.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsISocketTransportService.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsIEventQueueService.h"

#if defined(XP_PC) || defined(XP_UNIX) || defined(XP_BEOS)
//
// Both Windows and Unix support PR_PollableEvents which are used to break
// the socket transport thread out of calls to PR_Poll(...) when new
// file descriptors must be added to the poll list...
//
#define USE_POLLABLE_EVENT
#endif

//
// This is the Maximum number of Socket Transport instances that can be active
// at once...
//
#define MAX_OPEN_CONNECTIONS 50
#define DEFAULT_POLL_TIMEOUT_IN_MS  35*1000

// Forward declarations...
class nsSocketTransport;

class nsSocketTransportService : public nsISocketTransportService,
public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS
        NS_DECL_NSISOCKETTRANSPORTSERVICE
        NS_DECL_NSIRUNNABLE
        
        // nsSocketTransportService methods:
        nsSocketTransportService();
    virtual ~nsSocketTransportService();
    
    static NS_METHOD
        Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
    
    nsresult AddToWorkQ(nsSocketTransport* aTransport);
    
    // XXX: Should these use intervals or Milliseconds?
    nsresult GetSocketTimeoutInterval(PRIntervalTime* aResult);
    nsresult SetSocketTimeoutInterval(PRIntervalTime  aTime);
    
    // The following methods are called by the transport thread...
    nsresult ProcessWorkQ(void);
    
    nsresult AddToSelectList(nsSocketTransport* aTransport);
    nsresult RemoveFromSelectList(nsSocketTransport* aTransport);
    
    PRInt32   mConnectedTransports;
    PRInt32   mTotalTransports;
    
    nsresult  GetNeckoStringByName (const char *aName, PRUnichar **aString);

protected:
    nsIThread*            mThread;
    PRFileDesc*           mThreadEvent;
    PRLock*               mThreadLock;
    PRBool                mThreadRunning;
    
    PRCList               mWorkQ;
    
    PRInt32               mSelectFDSetCount;
    PRPollDesc*           mSelectFDSet;
    nsSocketTransport**   mActiveTransportList;
	nsCOMPtr<nsIStringBundle>   m_stringBundle;
    nsCOMPtr<nsIEventQueueService> mEventQueueService;
};


#endif /* nsSocketTransportService_h___ */


