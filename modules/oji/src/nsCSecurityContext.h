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
#include "nsIPrincipal.h"

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

    /**
     * Get the code base of the subject (caller).
     *
     * @param buf        -- Result buffer.
     * @param len        -- Buffer length.
     * @return           -- NS_OK if the codebase string was obtained.
     *                   -- NS_FALSE otherwise.
     */
    NS_IMETHOD GetOrigin(char* buf, int len);

    /**
     * Get the certificate thumbprint of the subject (caller).
     *
     * @param buf        -- Result buffer.
     * @param len        -- Buffer length.
     * @return           -- NS_OK if the codebase string was obtained.
     *                   -- NS_FALSE otherwise.
     */
    NS_IMETHOD GetCertificateID(char* buf, int len);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCSecurityContext:

    nsCSecurityContext(JSContext* cx);
    nsCSecurityContext(nsIPrincipal* principal);
    virtual ~nsCSecurityContext(void);

protected:
    JSStackFrame *m_pJStoJavaFrame;
    JSContext    *m_pJSCX;
private:
    nsIPrincipal *m_pPrincipal;
    PRBool        m_HasUniversalJavaCapability;
    PRBool        m_HasUniversalBrowserReadCapability;
};

#endif // nsCSecurityContext_h___
