/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIUnixToolkitService.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsUnixToolkitService.h"

static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsUnixToolkitServiceFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
			    const nsIID &aIID,
			    void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
  
  nsUnixToolkitServiceFactory(const nsCID &aClass);
  virtual ~nsUnixToolkitServiceFactory();

private:
  nsCID mClassID;
};

nsUnixToolkitServiceFactory::nsUnixToolkitServiceFactory(const nsCID &aClass) :
  mRefCnt(0),
  mClassID(aClass)
{
}

nsUnixToolkitServiceFactory::~nsUnixToolkitServiceFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsUnixToolkitServiceFactory, nsIFactory::GetIID())

NS_IMETHODIMP
nsUnixToolkitServiceFactory::CreateInstance(nsISupports *aOuter,
					   const nsIID &aIID,
					   void **aResult)
{
  if (aResult == nsnull)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;

  nsISupports *inst = nsnull;
  if (mClassID.Equals(kCUnixToolkitServiceCID)) {
    inst = (nsISupports *)(nsUnixToolkitService *) new nsUnixToolkitService();
  }

  if (inst == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK)
    delete inst;
  return rv;
}

nsresult nsUnixToolkitServiceFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.  
  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports *servMgr,
	     const nsCID &aClass,
	     const char *aClassName,
	     const char *aProgID,
	     nsIFactory **aFactory)
{
  if (nsnull == aFactory)
    return NS_ERROR_NULL_POINTER;

  *aFactory = new nsUnixToolkitServiceFactory(aClass);
  
  if (nsnull == aFactory) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  return (*aFactory)->QueryInterface(nsIFactory::GetIID(),
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
  printf("*** Registering UnixToolkitService\n");
#endif

  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = compMgr->RegisterComponent(kCUnixToolkitServiceCID,
                                  "Unix Window Service",
                                  NS_UNIX_TOOLKIT_SERVICE_PROGID,
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

  compMgr->UnregisterComponent(kCUnixToolkitServiceCID, fullpath);
  
  return NS_OK;
}
