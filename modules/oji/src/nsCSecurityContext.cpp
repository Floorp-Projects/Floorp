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
/*
 *
 * It contains the implementation providing nsISecurityCOntext XP-COM interface.
 * This file snapshots a JS frame before entering into java.
 *
 */


#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "libmocha.h"
#include "nsCSecurityContext.h"
#include "jvmmgr.h"

static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


////////////////////////////////////////////////////////////////////////////
// from nsISupports 

// Thes macro expands to the aggregated query interface scheme.

NS_IMPL_ADDREF(nsCSecurityContext);
NS_IMPL_RELEASE(nsCSecurityContext);

NS_METHOD
nsCSecurityContext::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }   
    *aInstancePtr = NULL;
    if (aIID.Equals(kISecurityContextIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (nsISecurityContext*) this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////
// from nsISecurityContext:

NS_METHOD 
nsCSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
    //NOTE: Test purpose only. Turn this on if you do not want security stack walking code.
    //*bAllowedAccess = PR_TRUE;
    //if(1)
     //return NS_OK;

    if(m_pJStoJavaFrame == NULL)
    {
      *bAllowedAccess = PR_FALSE;
       return NS_OK;
    }
    JSStackFrame** startFrame = JVM_GetStartJSFrameFromParallelStack();
    *startFrame = m_pJStoJavaFrame; // This updates the TLS start frame for a brief period
                                    // until the following code runs.
    /*
    ** TODO: Get a new API from Tom. 
    ** bAllowedAccess = LM_CanAccessTargetStr(m_pJSCX, target);
    */
    *startFrame = NULL;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////
// from nsCSecurityContext:
extern PRUintn tlsIndex3_g;
nsCSecurityContext::nsCSecurityContext(JSContext* cx)
                   : m_pJStoJavaFrame(NULL), m_pJSCX(NULL)
{
    NS_INIT_REFCNT();
    JSStackFrame *fp = NULL;
    m_pJStoJavaFrame = JS_FrameIterator(cx, &fp);
    m_pJSCX          = cx;
}

nsCSecurityContext::~nsCSecurityContext()
{
}

