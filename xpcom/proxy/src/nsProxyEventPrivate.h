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

#ifndef __nsProxyEventPrivate_h_
#define __nsProxyEventPrivate_h_

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"

#include "plevent.h"
#include "xptcall.h"    // defines nsXPTCVariant
#include "nsIEventQueue.h"

#include "nsProxyEvent.h"
#include "nsProxyObjectManager.h"

class nsProxyEventObject;
class nsProxyEventClass;

#define NS_PROXYEVENT_CLASS_IID                  \
{ 0xeea90d42, 									 \
  0xb059, 										 \
  0x11d2,						                 \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33}\
} 

#define NS_PROXYEVENT_IDENTITY_CLASS_IID \
{ 0xeea90d45, 0xb059, 0x11d2,                       \
  { 0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33 } }


class nsProxyEventClass : public nsISupports
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS

public:

    static nsProxyEventClass* GetNewOrUsedClass(REFNSIID aIID);
    
    
    NS_IMETHOD DelegatedQueryInterface(nsProxyEventObject* self, REFNSIID aIID, void** aInstancePtr);
    REFNSIID                 GetIID() const {return mIID;}
    nsIInterfaceInfo*        GetInterfaceInfo() const {return mInfo;}
    nsProxyEventObject*      GetRootProxyObject(nsProxyEventObject* anObject);
    nsProxyEventObject*      CallQueryInterfaceOnProxy(nsProxyEventObject* self, REFNSIID aIID);
    
    virtual ~nsProxyEventClass();
private:
    nsProxyEventClass();   // not implemented
    nsProxyEventClass(REFNSIID aIID, nsIInterfaceInfo* aInfo);

private:
    nsIInterfaceInfo* mInfo;
    nsIID mIID;
    uint32* mDescriptors;
};



class nsProxyEventObject : public nsXPTCStubBase
{
public:

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params);

    
    static nsProxyEventObject* GetNewOrUsedProxy(nsIEventQueue *destQueue,
                                                 PRInt32 proxyType,
                                                 nsISupports *aObj,
                                                 REFNSIID aIID);


    nsIEventQueue*        GetQueue() const { return mProxyObject->GetQueue(); }
    nsIEventQueue*        GetPLQueue() const { return mProxyObject->GetQueue(); }

    REFNSIID             GetIID()   const {return GetClass()->GetIID();}
    nsProxyEventClass*   GetClass() const { return mClass; }

    nsProxyEventObject*  GetRootProxyObject() const { return mRoot; }
    
    nsProxyEventObject*  Find(REFNSIID aIID);
    
    virtual ~nsProxyEventObject();

private:
    nsProxyEventObject();   // not implemented
    nsProxyEventObject(nsIEventQueue *destQueue,
                       PRInt32 proxyType,
                       nsISupports* aObj,
    				   nsProxyEventClass* aClass,
                       nsProxyEventObject* root);


    nsProxyObject*  mProxyObject;
   
    nsProxyEventClass* mClass;
    nsProxyEventObject* mRoot;
    nsProxyEventObject* mNext;
};




////////////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
////////////////////////////////////////////////////////////////////////////////

class nsProxyObjectManager: public nsIProxyObjectManager
{
public:

    NS_DECL_ISUPPORTS
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPROXYEVENT_MANAGER_IID);
    
    NS_IMETHOD GetProxyObject(nsIEventQueue *destQueue, 
                              REFNSIID aIID, 
                              nsISupports* aObj, 
                              PRInt32 proxyType,
                              void** aProxyObject);
    
    NS_IMETHOD GetProxyObject(nsIEventQueue *destQueue, 
                              const nsCID &aClass, 
                              nsISupports *aDelegate, 
                              const nsIID &aIID, 
                              PRInt32 proxyType,
                              void** aProxyObject);
    
    
    
    // Helpers
	static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
    
    
    nsProxyObjectManager();
    virtual ~nsProxyObjectManager();
    
    static nsProxyObjectManager *GetInstance();
    
    nsHashtable *GetRealObjectToProxyObjectMap();
    nsHashtable *GetIIDToProxyClassMap();
    
    
private:
    static nsProxyObjectManager* mInstance;
    
    PRLock      *mLock;
    nsHashtable *mProxyObjectMap;
    nsHashtable *mProxyClassMap;
};


#endif
