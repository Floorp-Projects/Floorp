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

#ifndef __nsProxyEvent_h_
#define __nsProxyEvent_h_

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"

#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "plevent.h"
#include "prtypes.h"
#include "xptcall.h"
#include "xptinfo.h"
    
class nsProxyObjectCallInfo;

#define PROXY_SYNC    0x0001  // acts just like a function call.
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
//   
//  So what gets saved?  
//
//  You can safely pass base types by value.  You can also pass interface pointers.
//  I will make sure that the interface pointers are addrefed while they are being 
//  proxied.  You can also pass string and wstring.  These I will copy and free.
//
//  I do **NOT** copy arrays or strings with size.  If you are using these either
//  change your interface, or contact me about this feature request.




// Using the ISupports interface just for addrefing.  

class nsProxyObject : public nsISupports  
{
public:
                        
    NS_DECL_ISUPPORTS
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISUPPORTS_IID)
    	// wierd, but it shouldn't break.  Need to discuss this with dougt

    nsProxyObject();
    nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, nsISupports *realObject);
    nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID);

    virtual             ~nsProxyObject();

    nsresult            Post( PRUint32            methodIndex, 
                              nsXPTMethodInfo   * info, 
                              nsXPTCMiniVariant * params, 
                              nsIInterfaceInfo  * interfaceInfo);
    
    nsresult            PostAndWait(nsProxyObjectCallInfo *proxyInfo);
    nsISupports*        GetRealObject() const;
    nsIEventQueue*      GetQueue() const;
    PRInt32             GetProxyType() const { return mProxyType; }

    friend class nsProxyEventObject;
private:
    
    PRInt32                   mProxyType;
    
    nsCOMPtr<nsIEventQueue>   mDestQueue;        /* destination queue */
    
    nsCOMPtr<nsISupports>     mRealObject;       /* the non-proxy object that this event is referring to. 
                                                    This is a strong ref. */
    nsCOMPtr<nsIEventQueueService> mEventQService;

    nsresult convertMiniVariantToVariant(nsXPTMethodInfo   * methodInfo, 
                                         nsXPTCMiniVariant * params, 
                                         nsXPTCVariant     **fullParam, 
                                         uint8 *paramCount);

};


class NS_EXPORT nsProxyObjectCallInfo
{
public:
    
    nsProxyObjectCallInfo(nsProxyObject* owner,
                          nsXPTMethodInfo *methodInfo,
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
    void                SetCompleted();
    void                PostCompleted();

    void                SetResult(nsresult rv) {mResult = rv; }
    
    nsIEventQueue*      GetCallersQueue();
    void                SetCallersQueue(nsIEventQueue* queue);

private:
    
    nsresult         mResult;                    /* this is the return result of the called function */
    nsXPTMethodInfo *mMethodInfo;
    PRUint32         mMethodIndex;               /* which method to be called? */
    nsXPTCVariant   *mParameterList;             /* marshalled in parameter buffer */
    PRUint32         mParameterCount;            /* number of params */
    PLEvent         *mEvent;                     /* the current plevent */       
    PRInt32          mCompleted;                 /* is true when the method has been called. */
       
    nsCOMPtr<nsIEventQueue>  mCallersEventQ;     /* this is the eventQ that we must post a message back to 
                                                    when we are done invoking the method (only PROXY_SYNC). 
                                                  */

    nsCOMPtr<nsProxyObject>   mOwner;            /* this is the strong referenced nsProxyObject */
   
    void RefCountInInterfacePointers(PRBool addRef);
    void CopyStrings(PRBool copy);

};


#endif
