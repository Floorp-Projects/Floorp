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
#include "nsIGenericFactory.h"
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
	nsresult rv = NS_OK;

	if (aResult == NULL) {  
		return NS_ERROR_NULL_POINTER;  
	}  

	*aResult = NULL;  

	// views aren't reference counted, so have to be treated specially.
	// their lifetimes are managed by the view manager they are associated with.
	nsIView* view = nsnull;
	if (mClassID.Equals(kCView)) {
		view = new nsView();
	} else if (mClassID.Equals(kCScrollingView)) {
		view = new nsScrollingView();
	}
	if (nsnull == view)
		return NS_ERROR_OUT_OF_MEMORY;  
	rv = view->QueryInterface(aIID, aResult);
	if (NS_FAILED(rv))
		view->Destroy();
	
	return rv;  
}  

nsresult nsViewFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewManager)

// return the proper factory to the caller
extern "C" NS_VIEW nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
	nsresult rv = NS_OK;

	do {
		if (nsnull == aFactory) {
			rv = NS_ERROR_NULL_POINTER;
			break;
		}

		*aFactory = nsnull;

		if (aClass.Equals(kCViewManager)) {
			nsIGenericFactory* factory = nsnull;
			rv = NS_NewGenericFactory(&factory, &nsViewManagerConstructor);
			if (NS_SUCCEEDED(rv))
				*aFactory = factory;
		} else if (aClass.Equals(kCView) || aClass.Equals(kCScrollingView)) {
			nsViewFactory* factory = new nsViewFactory(aClass);
			if (nsnull == factory) {
				rv = NS_ERROR_OUT_OF_MEMORY;
				break;
			}
			NS_ADDREF(factory);
			*aFactory = factory;
		}
	} while (0);
	
	return rv;
}
