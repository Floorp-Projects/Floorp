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
#include "prmem.h"
#include "plstr.h"

#if defined(XP_PC)
#include <windows.h>  // Needed for Interlocked APIs defined in nsISupports.h
#endif /* XP_PC */


// nsLoadAttribs definition.
class nsLoadAttribs : public nsILoadAttribs {
public:
  nsLoadAttribs();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsILoadAttribs
  NS_IMETHOD Clone(nsILoadAttribs* aLoadAttribs);

  NS_IMETHOD SetBypassProxy(PRBool aBypass);
  NS_IMETHOD GetBypassProxy(PRBool *aBypass);
  
  NS_IMETHOD SetLocalIP(const PRUint32 aIP);
  NS_IMETHOD GetLocalIP(PRUint32 *aIP);
  
  NS_IMETHOD SetReloadType(nsURLReloadType aType);
  NS_IMETHOD GetReloadType(nsURLReloadType* aResult);

  NS_IMETHOD SetLoadType(nsURLLoadType aType);
  NS_IMETHOD GetLoadType(nsURLLoadType* aResult);

  // Byte Range Support
  NS_IMETHOD SetByteRangeHeader(const char *aByteRangeHeader);
  NS_IMETHOD GetByteRangeHeader(char** aByteRangeHeader);

protected:
    virtual ~nsLoadAttribs();

private:
    PRBool          mBypass;
    PRUint32        mLocalIP;
    nsURLLoadType   mLoadType;
    nsURLReloadType mReloadType;
    char*           mByteRangeHeader;
};

// nsLoadAttribs Implementation

static NS_DEFINE_IID(kILoadAttribsIID, NS_ILOAD_ATTRIBS_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsLoadAttribs, kILoadAttribsIID);

nsLoadAttribs::nsLoadAttribs() 
{
  NS_INIT_REFCNT();

  mBypass     = PR_FALSE;
  mLocalIP    = 0;
  mLoadType   = nsURLLoadNormal;
  mReloadType = nsURLReload;
  mByteRangeHeader = nsnull;
}

nsLoadAttribs::~nsLoadAttribs() 
{
    PR_FREEIF(mByteRangeHeader);
}

NS_IMETHODIMP
nsLoadAttribs::Clone(nsILoadAttribs* aLoadAttribs)
{
  nsresult rv = NS_OK;

  if (nsnull == aLoadAttribs) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    NS_LOCK_INSTANCE();
    
    aLoadAttribs->GetBypassProxy(&mBypass);
    aLoadAttribs->GetLocalIP(&mLocalIP);
    aLoadAttribs->GetReloadType(&mReloadType);

    NS_UNLOCK_INSTANCE();
  }
  return rv;

}

NS_IMETHODIMP
nsLoadAttribs::SetBypassProxy(PRBool aBypass) 
{
  NS_LOCK_INSTANCE();
  mBypass = aBypass;
  NS_UNLOCK_INSTANCE();
  return NS_OK;
}

NS_IMETHODIMP
nsLoadAttribs::GetBypassProxy(PRBool *aBypass) 
{
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

NS_IMETHODIMP
nsLoadAttribs::SetLocalIP(const PRUint32 aLocalIP) 
{
  NS_LOCK_INSTANCE();
  mLocalIP = aLocalIP;
  NS_UNLOCK_INSTANCE();
  return NS_OK;
}

NS_IMETHODIMP
nsLoadAttribs::GetLocalIP(PRUint32 *aLocalIP) 
{
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

NS_IMETHODIMP
nsLoadAttribs::SetReloadType(nsURLReloadType aType) 
{
  nsresult rv = NS_OK;

  if ((aType < nsURLReload) || (aType >= nsURLReloadMax)) {
    rv = NS_ERROR_ILLEGAL_VALUE;
  } else {
    NS_LOCK_INSTANCE();
    mReloadType = aType;
    NS_UNLOCK_INSTANCE();
  }
  return rv;
}

NS_IMETHODIMP
nsLoadAttribs::GetReloadType(nsURLReloadType* aResult) 
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    NS_LOCK_INSTANCE();
    *aResult = mReloadType;
    NS_UNLOCK_INSTANCE();
  }
  return rv;
}

NS_IMETHODIMP
nsLoadAttribs::SetLoadType(nsURLLoadType aType) 
{
  nsresult rv = NS_OK;

  if ((aType < nsURLLoadNormal) || (aType >= nsURLLoadMax)) {
    rv = NS_ERROR_ILLEGAL_VALUE;
  } else {
    NS_LOCK_INSTANCE();
    mLoadType = aType;
    NS_UNLOCK_INSTANCE();
  }
  return rv;
}

NS_IMETHODIMP
nsLoadAttribs::GetLoadType(nsURLLoadType* aResult) 
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    NS_LOCK_INSTANCE();
    *aResult = mLoadType;
    NS_UNLOCK_INSTANCE();
  }
  return rv;
}

NS_IMETHODIMP
nsLoadAttribs::SetByteRangeHeader(const char* aByteRangeHeader) 
{
  NS_LOCK_INSTANCE();

  // Free the old range header if any...
  PR_FREEIF(mByteRangeHeader);
  mByteRangeHeader = nsnull;

  if (nsnull != aByteRangeHeader) {
    mByteRangeHeader = PL_strdup(aByteRangeHeader);
  }

  NS_UNLOCK_INSTANCE();
  return NS_OK;
}

NS_IMETHODIMP
nsLoadAttribs::GetByteRangeHeader(char **aByteRangeHeader) 
{
  nsresult rv = NS_OK;

  NS_LOCK_INSTANCE();

  if (nsnull == aByteRangeHeader) {
    rv = NS_ERROR_NULL_POINTER;
  } else if (nsnull != mByteRangeHeader) {
    *aByteRangeHeader = PL_strdup(mByteRangeHeader);
  } else {
    *aByteRangeHeader = nsnull;
  }

  NS_UNLOCK_INSTANCE();
  return rv;
}

// Creation routines
NS_NET nsresult NS_NewLoadAttribs(nsILoadAttribs** aInstancePtrResult) {
  nsILoadAttribs* it;

  NS_NEWXPCOM(it, nsLoadAttribs);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kILoadAttribsIID, (void **) aInstancePtrResult);
}
