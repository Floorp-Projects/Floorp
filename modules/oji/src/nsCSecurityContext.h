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
 * It contains the class definition to implement nsISecurityContext XP-COM interface.
 *
 */


#ifndef nsCSecurityContext_h___
#define nsCSecurityContext_h___

#include "jscntxt.h"
#include "jsdbgapi.h"
#include "nsISecurityContext.h"

struct JSContext;

/**
 * nsCSecurityContext implements nsISecurityContext interface for navigator.
 * This is used by a JVM to implement netscape.javascript.JSObject functionality.
 */
class nsCSecurityContext :public nsISecurityContext {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports 

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsISecurityContext:

    /**
     * Get the security context to be used in LiveConnect.
     * This is used for JavaScript <--> Java.
     *
     * @param target        -- Possible target.
     * @param action        -- Possible action on the target.
     * @return              -- NS_OK if the target and action is permitted on the security context.
     *                      -- NS_FALSE otherwise.
     */
    NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCSecurityContext:

    nsCSecurityContext(JSContext* cx);
    virtual ~nsCSecurityContext(void);

protected:
    JSStackFrame *m_pJStoJavaFrame;
    JSContext    *m_pJSCX;
};

#endif // nsCSecurityContext_h___
