/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsTimerXlib.h"

#include "nsUnixTimerCIID.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kCTimerXlib, NS_TIMER_XLIB_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsTimerXlibFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

  nsTimerXlibFactory(const nsCID &aClass);
  virtual ~nsTimerXlibFactory();

private:
  nsCID mClassID;

};

nsTimerXlibFactory::nsTimerXlibFactory(const nsCID &aClass) :
  mRefCnt(0),
  mClassID(aClass)
{   
}   

nsTimerXlibFactory::~nsTimerXlibFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

NS_IMPL_ISUPPORTS(nsTimerXlibFactory, nsIFactory::GetIID())


NS_IMETHODIMP
nsTimerXlibFactory::CreateInstance(nsISupports *aOuter,  
                                const nsIID &aIID,  
                                void **aResult)  
{  
  if (aResult == nsnull)
    return NS_ERROR_NULL_POINTER;  

  *aResult = nsnull;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCTimerXlib)) 
  {
    inst = (nsISupports *)(nsTimerXlib *) new nsTimerXlib();
  }

  if (inst == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = inst->QueryInterface(aIID, aResult);

  if (rv != NS_OK) 
    delete inst;
  
  return rv;
}

nsresult nsTimerXlibFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.  
  return NS_OK;
}

nsresult
NSGetFactory(nsISupports* servMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsTimerXlibFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(nsIFactory::GetIID(), (void**)aFactory);
  
}

PRBool
NSCanUnload(nsISupports* aServMgr)
{
  return PR_FALSE;
}

nsresult
NSRegisterSelf(nsISupports* aServMgr, const char *fullpath)
{
  nsresult rv;

#ifdef NS_DEBUG
  printf("*** Registering XLIB timer\n");
#endif

  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;
  
  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kCTimerXlib,
                                  "XLIB timer",
                                  "component://netscape/timer/unix/xlib",
                                  fullpath,
                                  PR_TRUE, 
                                  PR_TRUE);
  
  return rv;
}

nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char *fullpath)
{

  nsresult rv;
  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;
  
  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  compMgr->UnregisterComponent(kCTimerXlib, fullpath);
  
  return NS_OK;
}
