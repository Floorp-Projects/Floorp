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
#include "nsISupports.h"

#include "nsViewsCID.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsScrollingView.h"

static NS_DEFINE_IID(kCViewManager, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kCView, NS_VIEW_CID);
static NS_DEFINE_IID(kCScrollingView, NS_SCROLLING_VIEW_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

class nsViewFactory : public nsIFactory
{   
public:   
    nsViewFactory(const nsCID &aClass);   
    virtual ~nsViewFactory();   

	NS_DECL_ISUPPORTS

	NS_DECL_NSIFACTORY

  private:   
    nsCID     mClassID;
};   

nsViewFactory::nsViewFactory(const nsCID &aClass)   
{   
	NS_INIT_ISUPPORTS();
	mClassID = aClass;
}   

nsViewFactory::~nsViewFactory()   
{   
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

NS_IMPL_ISUPPORTS1(nsViewFactory, nsIFactory)

nsresult nsViewFactory::CreateInstance(nsISupports *aOuter,  
                                       const nsIID &aIID,  
                                       void **aResult)
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCViewManager)) {
    inst = (nsISupports*) new nsViewManager();
  } else if (mClassID.Equals(kCView)) {
    inst = (nsISupports*) new nsView();
  } else if (mClassID.Equals(kCScrollingView)) {
    inst = (nsISupports*) (nsView*) new nsScrollingView();
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }

  // add a reference count, 
  NS_ADDREF(inst);
  nsresult res = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return res;  
}  

nsresult nsViewFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_VIEW nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsViewFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
