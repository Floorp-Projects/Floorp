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


#include "nsProxyEvent.h"
#include "prmem.h"          // for  PR_NEW
#include "xptcall.h"

#include "nsRepository.h"

nsProxyObject::nsProxyObject()
{
    mRealObjectOwned = PR_FALSE;
    mRealObject      = nsnull;
    mDestQueue       = nsnull;
}


nsProxyObject::nsProxyObject(PLEventQueue *destQueue, nsISupports *realObject)
{
    mRealObjectOwned = PR_FALSE;
    mRealObject      = realObject;
    mDestQueue       = destQueue;

    mRealObject->AddRef();
}


nsProxyObject::nsProxyObject(PLEventQueue *destQueue, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
    mRealObjectOwned = PR_TRUE;
    mDestQueue       = destQueue;
    
    nsresult rv = nsComponentManager::CreateInstance(aClass, 
                                                     aDelegate,
                                                     aIID,
                                                     (void**) &mRealObject);

    if (NS_FAILED(rv)) 
    {
        mRealObjectOwned = PR_FALSE;
        mRealObject      = nsnull;
    }
}



nsProxyObject::~nsProxyObject()
{
    PL_ENTER_EVENT_QUEUE_MONITOR(mDestQueue);
    PL_RevokeEvents(mDestQueue, this);
	
    if(mRealObject != nsnull)
    {
        if (!mRealObjectOwned)
            mRealObject->Release();
        else
            NS_RELEASE(mRealObject);
    }

    PL_EXIT_EVENT_QUEUE_MONITOR(mDestQueue);
}

nsresult
nsProxyObject::Post(  PRUint32        methodIndex,           /* which method to be called? */
                      PRUint32        paramCount,            /* number of params */
                      nsXPTCVariant   *params)
{

    PL_ENTER_EVENT_QUEUE_MONITOR(mDestQueue);

    PLEvent *event = PR_NEW(PLEvent);
    if (event == NULL) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ASSERTION(mRealObject, "no native object");

    PL_InitEvent(event, 
                 this,
                 nsProxyObject::EventHandler,
                 nsProxyObject::DestroyHandler);
    
    
    mMethodIndex       = methodIndex;
    mParameterCount    = paramCount;
    mParameterList     = params;

    PL_PostSynchronousEvent(mDestQueue, (PLEvent *)event);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(mDestQueue);

    PR_FREEIF(event);
        
    return mResult;
}



void
nsProxyObject::InvokeMethod()
{
    // invoke the magic of xptc...
    PL_ENTER_EVENT_QUEUE_MONITOR(mDestQueue);

    mResult = XPTC_InvokeByIndex( mRealObject, 
                                  mMethodIndex,
                                  mParameterCount, 
                                  mParameterList);
    
    PL_EXIT_EVENT_QUEUE_MONITOR(mDestQueue);
}



void nsProxyObject::DestroyHandler(PLEvent *self) {}


void* nsProxyObject::EventHandler(PLEvent *self) 
{
    nsProxyObject* proxyObject = (nsProxyObject*)PL_GetEventOwner(self);
    
    if (proxyObject != nsnull)
    {
        proxyObject->InvokeMethod();
    }    
    return NULL;
}












