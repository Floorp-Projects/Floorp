/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDNSService_h__
#define nsDNSService_h__

#include "nsIDNSService.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#if defined(XP_MAC)
#include <OSUtils.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#elif defined (XP_WIN)
#include <windows.h>
#include <Winsock2.h>
#endif
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "prmon.h"

#ifdef DEBUG
#define DNS_TIMING 1
#endif

class nsIDNSListener;
class nsDNSLookup;

class nsDNSService : public nsIDNSService,
                     public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIDNSSERVICE

    // nsDNSService methods:
    nsDNSService();
    virtual ~nsDNSService();
 
    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    friend class nsDNSLookup;
    
protected:
    nsresult LateInit();
    nsresult InitDNSThread();
    nsresult GetLookupEntry(const char* hostName, nsDNSLookup* *result);

    static nsDNSService * gService;
    static PRBool         gNeedLateInitialization;
    
    nsCOMPtr<nsIThread>   mThread;
    nsresult              mState;
    PRMonitor*            mMonitor;
    nsSupportsHashtable   mLookups; // of nsDNSLookups

#if defined(XP_MAC)
    friend pascal void    nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
    PRBool                mThreadRunning;
    InetSvcRef            mServiceRef;
    QHdr                  mCompletionQueue;
#if TARGET_CARBON
    OTClientContextPtr    mClientContext;
#endif /* TARGET_CARBON */
    OTNotifyUPP           nsDnsServiceNotifierRoutineUPP;
#endif /* XP_MAC */

#if defined(XP_WIN)
    PRUint32   AllocMsgID(void);
    void       FreeMsgID(PRUint32 msgID);

    friend static LRESULT CALLBACK nsDNSEventProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ProcessLookup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HWND                mDNSWindow;
    PRUint32            mMsgIDBitVector[4];
#endif /* XP_WIN */

#ifdef DNS_TIMING
    double              mCount;
    double              mTimes;
    double              mSquaredTimes;
    FILE*               mOut;
    friend class nsDNSRequest;
#endif
    char*               mMyIPAddress;
};

#endif /* nsDNSService_h__ */
