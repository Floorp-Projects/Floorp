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
#if defined(XP_MAC)
#include <OSUtils.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#elif defined (XP_PC)
#include <windows.h>
#endif

class nsIDNSListener;
class nsICancelable;
class nsDNSLookup;

class nsDNSService : public nsIDNSService,
                     public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    // nsDNSService methods:
    nsDNSService();
    virtual ~nsDNSService();
    nsresult Init();
 
    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // nsIDNSService methods:
    NS_DECL_NSIDNSSERVICE

protected:
    friend class nsDNSLookup;
#ifdef XP_MAC
    friend pascal void  nsDnsServiceNotifierRoutine(void * contextPtr, OTEventCode code, OTResult result, void * cookie);
#endif

    nsIThread *   mThread;
    PRBool        mThreadRunning;
    // nsDNSLookup cache? - list of nsDNSLookups, hash table (nsHashTable, nsStringKey)
    // list of nsDNSLookups in order of expiration (PRCList?)

#if defined(XP_MAC)

    InetSvcRef  mServiceRef;
	QHdr		mCompletionQueue;

#if TARGET_CARBON
    OTClientContextPtr  mClientContext;
    OTNotifyUPP         nsDnsServiceNotifierRoutineUPP;
#endif /* TARGET_CARBON */
	
#elif defined(XP_UNIX)
    //XXX - to be defined

#elif defined(_WIN32)
    WNDCLASS wc;
    HWND     DNSWindow;
    UINT     msgAsyncSelect;
    UINT     msgFoundDNS;
#endif
};

#endif /* nsDNSService_h__ */
