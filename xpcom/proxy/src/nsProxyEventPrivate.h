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

#ifndef __nsProxyEventPrivate_h_
#define __nsProxyEventPrivate_h_

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsHashtable.h"

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


#define NS_PROXYEVENT_OBJECT_IID \
{ 0xec373590, 0x9164, 0x11d3,    \
{0x8c, 0x73, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

static NS_DEFINE_IID(kProxyEventClassIID, NS_PROXYEVENT_CLASS_IID);
static NS_DEFINE_IID(kProxyEventObjectIID, NS_PROXYEVENT_OBJECT_IID);


class nsProxyEventClass : public nsISupports
{
public:
    NS_DECL_ISUPPORTS
    
    
    static const nsIID& GetIID() {static nsIID iid = NS_PROXYEVENT_CLASS_IID; return iid;}
    static nsProxyEventClass* GetNewOrUsedClass(REFNSIID aIID);
    
    NS_IMETHOD DelegatedQueryInterface( nsProxyEventObject* self, 
                                        REFNSIID aIID, 
                                        void** aInstancePtr);


    nsIInterfaceInfo*        GetInterfaceInfo() const {return mInfo;}
    const nsIID&             GetProxiedIID()    const {return mIID; }
protected:
    nsProxyEventClass();
    nsProxyEventClass(REFNSIID aIID, nsIInterfaceInfo* aInfo);
    virtual ~nsProxyEventClass();
    
private:
    nsIID                      mIID;
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    uint32*                    mDescriptors;

	nsresult	      CallQueryInterfaceOnProxy(nsProxyEventObject* self, 
                                                REFNSIID aIID, 
                                                nsProxyEventObject** aInstancePtr);
};



class nsProxyEventObject : public nsXPTCStubBase
{
public:

    NS_DECL_ISUPPORTS
    static const nsIID& GetIID() {static nsIID iid = NS_PROXYEVENT_OBJECT_IID; return iid;}
    
    static nsProxyEventObject* GetNewOrUsedProxy(nsIEventQueue *destQueue,
                                                 PRInt32 proxyType,
                                                 nsISupports *aObj,
                                                 REFNSIID aIID);
    
    
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex, const nsXPTMethodInfo* info, nsXPTCMiniVariant* params);


    nsProxyEventClass*    GetClass()           const { return mClass; }
    nsIEventQueue*        GetQueue()           const { return (mProxyObject ? mProxyObject->GetQueue()     : nsnull);}
    nsISupports*          GetRealObject()      const { return (mProxyObject ? mProxyObject->GetRealObject(): nsnull);}
    PRInt32               GetProxyType()       const { return (mProxyObject ? mProxyObject->GetProxyType() : nsnull);} 

    nsProxyEventObject();
    nsProxyEventObject(nsIEventQueue *destQueue,
                       PRInt32 proxyType,
                       nsISupports* aObj,
    				   nsProxyEventClass* aClass,
                       nsProxyEventObject* root,
                       const char * hashStr);
    
    virtual ~nsProxyEventObject();
    
    nsProxyEventObject*   Find(REFNSIID aIID);

#ifdef DEBUG_dougt
    void DebugDump(const char * message, PRUint32 hashKey);
#endif

protected:
    nsStringKey           mHashKey;

    nsCOMPtr<nsProxyEventClass>  mClass;
    nsCOMPtr<nsProxyObject> mProxyObject;
    /* 
        nsProxyEventObject are
        incomplete classes.  do not try changing 
        them into nsCOMPtr's, or you will find 
        that it wont work :-)
    */
    
    nsProxyEventObject *mRoot;
    nsProxyEventObject *mNext;
};




////////////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
////////////////////////////////////////////////////////////////////////////////

class nsProxyObjectManager: public nsIProxyObjectManager
{
public:

    NS_DECL_ISUPPORTS

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
