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

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"

#include "plevent.h"
#include "xptcall.h"


class NS_EXPORT nsProxyObject
{
    public:
                            nsProxyObject();
                            nsProxyObject(PLEventQueue *destQueue, nsISupports *realObject);
                            nsProxyObject(PLEventQueue *destQueue, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID);
        
        virtual             ~nsProxyObject();


        nsresult            Post(  PRUint32        methodIndex,           /* which method to be called? */
                                   PRUint32        paramCount,            /* number of params */
                                   nsXPTCVariant   *params);

        
        nsresult            GetLastResult() const { return mResult; }
        nsISupports*        GetRealObject() const { return mRealObject; }
        PRUint32            GetMethodIndex() const { return mMethodIndex; }
        PRUint32            GetParameterCount() const { return mParameterCount; }
        nsXPTCVariant*      GetParameterList() const { return mParameterList; }
        PLEventQueue*       GetQueue() const { return mDestQueue; }
        
        
        
        // These are called from PLEvent. They must be public.  You should not use them.

        static              void* EventHandler(PLEvent *self);
        static              void DestroyHandler(PLEvent *self);
                            void InvokeMethod(void);
    private:
        
        PLEventQueue    *mDestQueue;                 /* destination queue */
        nsISupports     *mRealObject;                /* the non-proxy object that this event is referring to */
        PRBool          mRealObjectOwned;

        PRUint32         mMethodIndex;               /* which method to be called? */
        nsresult         mResult;                    /* this is the return result of the called function */
        PRUint32         mParameterCount;            /* number of params */
        nsXPTCVariant   *mParameterList;             /* marshalled in parameter buffer */
};


#define NS_DECL_PROXY(_class, _interface) \
public: \
  _class(PLEventQueue *, _interface *); \
private: \
  nsProxyObject mProxyObject;\
public:


#define NS_IMPL_PROXY(_class, _interface)\
_class::_class(PLEventQueue *eventQueue, _interface *realObject) \
: mProxyObject(eventQueue, realObject) \
{\
}\

#endif
