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
 * It contains the implementation providing nsISecurityCOntext XP-COM interface.
 * This file snapshots a JS frame before entering into java.
 *
 */


#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "nsCSecurityContext.h"
#include "jvmmgr.h"

// For GetOrigin()

#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsScriptSecurityManager.h"

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
    nsIPrincipal* pIPrincipal = NULL;

    if(!bAllowedAccess) {
        return PR_FALSE;
    }
  
    if(!nsCRT::strcmp(target,"UniversalBrowserRead")) {
        *bAllowedAccess = m_HasUniversalBrowserReadCapability;
        return NS_OK;
    } else if(!nsCRT::strcmp(target,"UniversalJavaPermission")) {
        *bAllowedAccess = m_HasUniversalJavaCapability;
        return NS_OK;
    } else {
        *bAllowedAccess = PR_FALSE;
    }
    return PR_TRUE;
}


NS_METHOD 
nsCSecurityContext::GetOrigin(char* buf, int buflen)
{
    nsCOMPtr<nsIPrincipal> principal = NULL;
    //    nsIPrincipal* principal = NULL;
  
    // Get the Script Security Manager.

    nsresult rv      = NS_OK;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv)
    if (NS_FAILED(rv) || !secMan) return PR_FALSE;


    if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
    if (!codebase) 
        return NS_ERROR_FAILURE;

    char* origin=nsnull;
    codebase->GetOrigin(&origin);

    if( origin ) {
        PRInt32 originlen = (PRInt32) nsCRT::strlen(origin);
        if(!buf || buflen<=originlen) {
            if( origin ) {
                nsCRT::free(origin);
            }
            return PR_FALSE;
        }

        // Copy the string into to user supplied buffer. Is there a better
        // way to do this?

        nsCRT::memcpy(buf,origin,originlen);
        buf[originlen]=nsnull; // Gotta terminate it.
        nsCRT::free(origin);
    } else {
        *buf = nsnull;
    }

    return PR_TRUE;
}

NS_METHOD 
nsCSecurityContext::GetCertificateID(char* buf, int buflen)
{
    nsCOMPtr<nsIPrincipal> principal = NULL;
    //nsIPrincipal* principal = NULL;
  
    // Get the Script Security Manager.

    nsresult rv      = NS_OK;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv)
    if (NS_FAILED(rv) || !secMan) return PR_FALSE;


    secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    nsCOMPtr<nsICertificatePrincipal> cprincipal = do_QueryInterface(principal);
    if (!cprincipal) 
        return NS_ERROR_FAILURE;

    char* certificate = nsnull;
    cprincipal->GetCertificateID(&certificate);

    if( certificate ) {
        PRInt32 certlen = (PRInt32) nsCRT::strlen(certificate);
        if( buflen<=certlen ) {
            nsCRT::free(certificate);
            return PR_FALSE;
        }
        nsCRT::memcpy(buf,certificate,certlen);
        buf[certlen]=nsnull;
        nsCRT::free(certificate);
    } else {
        *buf = nsnull;
    }

    return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////////
// from nsCSecurityContext:
extern PRUintn tlsIndex3_g;
nsCSecurityContext::nsCSecurityContext(JSContext* cx)
                   : m_pJStoJavaFrame(NULL), m_pJSCX(NULL),
                     m_pPrincipal(NULL),
                     m_HasUniversalBrowserReadCapability(PR_FALSE),
                     m_HasUniversalJavaCapability(PR_FALSE)
{
    NS_INIT_REFCNT();

      // Get the Script Security Manager.

    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv)
    if (NS_FAILED(rv) || !secMan) return;

    // Do early evaluation of "UniversalJavaPermission" capability.
    
    secMan->IsCapabilityEnabled("UniversalBrowserRead",&m_HasUniversalBrowserReadCapability);
    secMan->IsCapabilityEnabled("UniversalJavaPermission",&m_HasUniversalJavaCapability);
}

nsCSecurityContext::~nsCSecurityContext()
{
}

