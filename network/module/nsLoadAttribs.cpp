/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsString.h"
#include "nsILoadAttribs.h"
#include "prtypes.h"


// nsLoadAttribs definition.
class nsLoadAttribs : public nsILoadAttribs {
public:
    nsLoadAttribs();
    virtual ~nsLoadAttribs();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsILoadAttribs
    NS_IMETHOD SetBypassProxy(PRBool aBypass);
    NS_IMETHOD GetBypassProxy(PRBool *aBypass);
    NS_IMETHOD SetLocalIP(const PRUint32 aIP);
    NS_IMETHOD GetLocalIP(PRUint32 *aIP);

private:
    PRBool mBypass;
    PRUint32 mLocalIP;
};

// nsLoadAttribs Implementation

static NS_DEFINE_IID(kILoadAttribsIID, NS_ILOAD_ATTRIBS_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsLoadAttribs, kILoadAttribsIID);

nsLoadAttribs::nsLoadAttribs() {
    mBypass = PR_FALSE;
    mLocalIP = 0;
}

nsLoadAttribs::~nsLoadAttribs() {
}

nsresult
nsLoadAttribs::SetBypassProxy(PRBool aBypass) {
    NS_LOCK_INSTANCE();
    mBypass = aBypass;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult
nsLoadAttribs::GetBypassProxy(PRBool *aBypass) {
    nsresult rv = NS_OK;

    if (nsnull == aBypass) {
        rv = NS_ERROR_NULL_POINTER;
    } else {
        NS_LOCK_INSTANCE();
        *aBypass = mBypass;
        NS_UNLOCK_INSTANCE();
    }
    return rv;
}

nsresult
nsLoadAttribs::SetLocalIP(const PRUint32 aLocalIP) {
    NS_LOCK_INSTANCE();
    mLocalIP = aLocalIP;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult
nsLoadAttribs::GetLocalIP(PRUint32 *aLocalIP) {
    nsresult rv = NS_OK;

    if (nsnull == aLocalIP) {
        rv = NS_ERROR_NULL_POINTER;
    } else {
        NS_LOCK_INSTANCE();
        *aLocalIP = mLocalIP;
        NS_UNLOCK_INSTANCE();
    }
    return rv;
}

// Creation routines
NS_NET nsresult NS_NewLoadAttribs(nsILoadAttribs** aInstancePtrResult) {
  nsILoadAttribs* it = new nsLoadAttribs();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kILoadAttribsIID, (void **) aInstancePtrResult);
}