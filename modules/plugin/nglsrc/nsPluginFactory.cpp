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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"

//#include "nsPluginsCID.h"
#include "nsPluginHostImpl.h"
#include "nsIServiceManager.h"
#include "nsIPluginManager.h"

//static NS_DEFINE_IID(kCPluginHost, NS_PLUGIN_HOST_CID);
static NS_DEFINE_CID(kCPluginManagerCID,          NS_PLUGINMANAGER_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);

class nsPluginFactory : public nsIFactory
{   
  public:   
    NS_DECL_ISUPPORTS

    NS_DECL_NSIFACTORY

    nsPluginFactory(const nsCID &aClass, nsIServiceManager* serviceMgr);   

  protected:
    virtual ~nsPluginFactory();   

  private:   
    nsCID     mClassID;
    nsIServiceManager *mserviceMgr;
};   

nsPluginFactory :: nsPluginFactory(const nsCID &aClass, nsIServiceManager* serviceMgr)   
{   
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
  // XXX Are we sure about this weak reference. -dp
  mserviceMgr = serviceMgr;
}   

nsPluginFactory :: ~nsPluginFactory()   
{
}   

NS_IMPL_ISUPPORTS(nsPluginFactory, NS_GET_IID(nsIFactory))

nsresult nsPluginFactory :: CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }

  *aResult = NULL;  
  
  nsPluginHostImpl *inst = nsnull;

  //if (mClassID.Equals(kCPluginHost) || mClassID.Equals(kCPluginManagerCID) ){
  if (mClassID.Equals(kCPluginManagerCID) ) {
    inst = new nsPluginHostImpl(mserviceMgr);
  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  NS_ADDREF(inst);  // Stabilize
  
  nsresult res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

  return res;  
}  

nsresult nsPluginFactory :: LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_PLUGIN nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr = do_QueryInterface(serviceMgr, &rv);
  if (NS_FAILED(rv)) return rv;

  *aFactory = new nsPluginFactory(aClass, servMgr);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

