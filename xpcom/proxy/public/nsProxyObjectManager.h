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

#ifndef __nsProxyObjectManager_h_
#define __nsProxyObjectManager_h_

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIEventQueue.h"

#include "nsHashtable.h"

#include "xptcall.h"    // defines nsXPTCVariant

#include "nsProxyEvent.h"


#define NS_PROXYEVENT_FACTORY_IID                \
{ 0xeea90d40, 									 \
  0xb059, 										 \
  0x11d2,						                 \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33}\
} 


#define NS_IPROXYEVENT_MANAGER_IID               \
{ 0xeea90d43, 									 \
  0xb059, 										 \
  0x11d2,						                 \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33}\
} 

#define NS_PROXYEVENT_MANAGER_CID                \
{ 0xeea90d41, 									 \
  0xb059, 										 \
  0x11d2,						                 \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33}\
} 


#define NS_XPCOMPROXY_PROGID "component://netscape/xpcomproxy"
#define NS_XPCOMPROXY_CLASSNAME "XPCom Proxy"

////////////////////////////////////////////////////////////////////////////////
// nsProxyEventFactory:
////////////////////////////////////////////////////////////////////////////////

class nsProxyEventFactory : public nsIFactory 
{
    public:
        
        nsProxyEventFactory();
        virtual ~nsProxyEventFactory();
        
        NS_DECL_ISUPPORTS

        NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                  REFNSIID aIID,
                                  void **aResult);

        NS_IMETHOD LockFactory(PRBool aLock);

};


////////////////////////////////////////////////////////////////////////////////
// nsIProxyObjectManager
////////////////////////////////////////////////////////////////////////////////

class nsIProxyObjectManager : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IPROXYEVENT_MANAGER_IID; return iid; }

    NS_IMETHOD GetProxyObject(nsIEventQueue *destQueue, 
                              REFNSIID aIID, 
                              nsISupports* aObj, 
                              PRInt32 proxyType,
                              void** aProxyObject) = 0;
    
    NS_IMETHOD GetProxyObject(nsIEventQueue *destQueue, 
                              const nsCID &aClass, 
                              nsISupports *aDelegate, 
                              const nsIID &aIID,
                              PRInt32 proxyType,
                              void** aProxyObject) = 0;
};

#endif
