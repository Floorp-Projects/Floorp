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

#ifndef __nsProxyEvent_h_
#define __nsProxyEvent_h_

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"

#include "nsIEventQueue.h"
#include "plevent.h"
#include "prtypes.h"
#include "xptcall.h"
#include "xptinfo.h"
    
class nsProxyObjectCallInfo;

#define PROXY_SYNC    0x0001  // act just like a function call.
#define PROXY_ASYNC   0x0002  // fire and forget.  This will return immediately and you will lose all return information.
#define PROXY_ALWAYS  0x0004   // ignore check to see if the eventQ is on the same thread as the caller, and alway return a proxied object.

//#define AUTOPROXIFICATION

// WARNING about PROXY_ASYNC:  
//
// If the calling thread goes away, any function which accesses the calling stack 
// will blow up.
//
//  example:
//
//     myFoo->bar(&x)
//
//     ... thread goes away ...
//
//    bar(PRInt32 *x)
//    {
//         *x = 0;   <-----  You will blow up here.
//




// Using the ISupports interface just for addrefing.  

#define NS_PROXYOBJECT_CID                       \
{ 0xeea90d40,                                    \
  0xb09f,                                        \
  0x11d2,                                        \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33}\
} 


class nsProxyObject : public nsISupports  
{
public:
                        
    NS_DECL_ISUPPORTS
        NS_DEFINE_STATIC_IID_ACCESSOR(nsCOMTypeInfo<nsISupports>::GetIID())    
    nsProxyObject();
    nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, nsISupports *realObject);
    nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID);

    virtual             ~nsProxyObject();

    nsresult            Post(PRUint32 methodIndex, nsXPTMethodInfo *info, 
                             nsXPTCMiniVariant *params, nsIInterfaceInfo *interfaceInfo);
    
    nsresult            NestedEventLoop(nsProxyObjectCallInfo *proxyInfo);
    nsISupports*        GetRealObject() const { return mRealObject; }
    nsIEventQueue*      GetQueue();
    PRInt32             GetProxyType() const { return mProxyType; }

    


private:
#ifdef AUTOPROXIFICATION       
    typedef enum
    {
        convertOutParameters = 1, 
        convertInParameters,
        convertAllParameters

    } AutoProxyConvertTypes;


    nsresult            AutoProxyParameterList(PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, 
                                               nsIInterfaceInfo *interfaceInfo, AutoProxyConvertTypes convertType);
#endif
    
    nsIEventQueue       *mDestQueue;                 /* destination queue */
    nsISupports         *mRealObject;                /* the non-proxy object that this event is referring to. 
                                                        This is a strong ref. */

    PRBool              mRealObjectOwned;
    PRInt32             mProxyType;
    
    nsresult convertMiniVariantToVariant(nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, 
                                         nsXPTCVariant **fullParam, uint8 *paramCount);

        
 };


class NS_EXPORT nsProxyObjectCallInfo
{
public:
    
    nsProxyObjectCallInfo(nsProxyObject* owner,
                          PRUint32 methodIndex, 
                          nsXPTCVariant* parameterList, 
                          PRUint32 parameterCount, 
                          PLEvent *event);

    virtual ~nsProxyObjectCallInfo();
    
    PRUint32            GetMethodIndex() const { return mMethodIndex; }
    nsXPTCVariant*      GetParameterList() const { return mParameterList; }
    PRUint32            GetParameterCount() const { return mParameterCount; }
    PLEvent*            GetPLEvent() const { return mEvent; }
    nsresult            GetResult() const { return mResult; }
    nsProxyObject*      GetProxyObject() const { return mOwner; }
    PRBool              GetCompleted();

    void                SetResult(nsresult rv) {mResult = rv; }
    void                SetCompleted();

private:
    
    nsresult         mResult;                    /* this is the return result of the called function */
    PRUint32         mMethodIndex;               /* which method to be called? */
    nsXPTCVariant   *mParameterList;             /* marshalled in parameter buffer */
    PRUint32         mParameterCount;            /* number of params */
    PLEvent         *mEvent;                     /* the current plevent */       
    PRInt32          mCompleted;                 /* is true when the method has been called. */
    
    nsCOMPtr<nsProxyObject>   mOwner;            /* this is the strong referenced nsProxyObject */
    

};


#endif
