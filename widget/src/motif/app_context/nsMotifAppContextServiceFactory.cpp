/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIMotifAppContextService.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsMotifAppContextService.h"

static NS_DEFINE_CID(kCMotifAppContextServiceCID, NS_MOTIF_APP_CONTEXT_SERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsMotifAppContextServiceFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
			    const nsIID &aIID,
			    void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
  
  nsMotifAppContextServiceFactory(const nsCID &aClass);
  virtual ~nsMotifAppContextServiceFactory();

private:
  nsCID mClassID;
};

nsMotifAppContextServiceFactory::nsMotifAppContextServiceFactory(const nsCID &aClass) :
  mRefCnt(0),
  mClassID(aClass)
{
}

nsMotifAppContextServiceFactory::~nsMotifAppContextServiceFactory()
{
}

NS_IMPL_ISUPPORTS(nsMotifAppContextServiceFactory, NS_GET_IID(nsIFactory))

NS_IMETHODIMP
nsMotifAppContextServiceFactory::CreateInstance(nsISupports *aOuter,
					   const nsIID &aIID,
					   void **aResult)
{
  if (aResult == nsnull)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;

  nsISupports *inst = nsnull;
  if (mClassID.Equals(kCMotifAppContextServiceCID)) {
    inst = (nsISupports *)(nsMotifAppContextService *) new nsMotifAppContextService();
  }

  if (inst == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK)
    delete inst;
  return rv;
}

nsresult nsMotifAppContextServiceFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.  
  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports *servMgr,
	     const nsCID &aClass,
	     const char *aClassName,
	     const char *aContractID,
	     nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsMotifAppContextServiceFactory(aClass);
  
  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return (*aFactory)->QueryInterface(NS_GET_IID(nsIFactory),
				     (void **)aFactory);
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports *aServMgr)
{
  return PR_FALSE;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char *fullpath)
{
  nsresult rv;

#ifdef NS_DEBUG
  printf("*** Registering MotifAppContextService\n");
#endif

  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = compMgr->RegisterComponent(kCMotifAppContextServiceCID,
                                  "Motif App Context Service",
                                  NS_MOTIF_APP_CONTEXT_SERVICE_CONTRACTID,
                                  fullpath,
                                  PR_TRUE,
                                  PR_TRUE);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports *aServMgr, const char *fullpath)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;
  
  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  compMgr->UnregisterComponent(kCMotifAppContextServiceCID, fullpath);
  
  return NS_OK;
}
