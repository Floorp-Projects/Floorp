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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are
 * Copyright (C) 1999 Christopher Blizzard. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Hartshorn <peter@igelaus.com.au>
 */

#include "nsIXlibWindowService.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsXlibWindowService.h"

static NS_DEFINE_CID(kCXlibWindowServiceCID, NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsXlibWindowServiceFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
			    const nsIID &aIID,
			    void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
  
  nsXlibWindowServiceFactory(const nsCID &aClass);
  virtual ~nsXlibWindowServiceFactory();

private:
  nsCID mClassID;
};

nsXlibWindowServiceFactory::nsXlibWindowServiceFactory(const nsCID &aClass) :
  mRefCnt(0),
  mClassID(aClass)
{
}

nsXlibWindowServiceFactory::~nsXlibWindowServiceFactory()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXlibWindowServiceFactory, nsIFactory)

NS_IMETHODIMP
nsXlibWindowServiceFactory::CreateInstance(nsISupports *aOuter,
					   const nsIID &aIID,
					   void **aResult)
{
  if (aResult == nsnull)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;

  nsISupports *inst = nsnull;
  if (mClassID.Equals(kCXlibWindowServiceCID)) {
    inst = (nsISupports *)(nsXlibWindowService *) new nsXlibWindowService();
  }

  if (inst == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK)
    delete inst;
  return rv;
}

nsresult nsXlibWindowServiceFactory::LockFactory(PRBool aLock)
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

  *aFactory = new nsXlibWindowServiceFactory(aClass);
  
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
  printf("*** Registering XlibWindowService\n");
#endif

  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIComponentManager> compMgr = 
           do_GetService(kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = compMgr->RegisterComponent(kCXlibWindowServiceCID,
                                  "Xlib Window Service",
                                  NS_XLIB_WINDOW_SERVICE_CONTRACTID,
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
  
  nsCOMPtr<nsIComponentManager> compMgr = 
           do_GetService(kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  compMgr->UnregisterComponent(kCXlibWindowServiceCID, fullpath);
  
  return NS_OK;
}
