/* -*- Mode: C++; tab-width: 2; indentT-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsTimerQt.h"

#include "nsUnixTimerCIID.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kCTimerQt, NS_TIMER_QT_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsTimerQtFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

  nsTimerQtFactory(const nsCID &aClass);
  virtual ~nsTimerQtFactory();

private:
  nsCID mClassID;

};

nsTimerQtFactory::nsTimerQtFactory(const nsCID &aClass) :
  mRefCnt(0),
  mClassID(aClass)
{   
}   

nsTimerQtFactory::~nsTimerQtFactory()   
{   
}   

NS_IMPL_ISUPPORTS(nsTimerQtFactory, NS_GET_IID(nsIFactory))


NS_IMETHODIMP
nsTimerQtFactory::CreateInstance(nsISupports *aOuter,  
                                const nsIID &aIID,  
                                void **aResult)  
{  
  if (aResult == nsnull)
    return NS_ERROR_NULL_POINTER;  

  *aResult = nsnull;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCTimerQt)) 
  {
    inst = (nsISupports *)(nsTimerQt *) new nsTimerQt();
  }

  if (inst == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = inst->QueryInterface(aIID, aResult);

  if (rv != NS_OK) 
    delete inst;
  
  return rv;
}

nsresult nsTimerQtFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.  
  return NS_OK;
}

nsresult
NSGetFactory(nsISupports* servMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsTimerQtFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(NS_GET_IID(nsIFactory), (void**)aFactory);
  
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
  printf("*** Registering QT timer\n");
#endif

  nsCOMPtr<nsIServiceManager>
    serviceManager(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;
  
  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kCTimerQt,
                                  "QT timer",
                                  "@mozilla.org/timer/unix/qt;1",
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

  compMgr->UnregisterComponent(kCTimerQt, fullpath);
  
  return NS_OK;
}
