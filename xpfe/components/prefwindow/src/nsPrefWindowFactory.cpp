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

#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "nsCOMPtr.h"

// include files for components this factory creates...
#include "nsPrefWindow.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
//static NS_DEFINE_CID(kPrefWindowCID,             NS_PREFWINDOW_CID);

static PRInt32 gLockCount = 0;

//========================================================================================
class nsPrefWindowFactory : public nsIFactory
//========================================================================================
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	nsPrefWindowFactory(/*const nsCID &aClass, const char* aClassName, const char* aProgID*/); 

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
	NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsPrefWindowFactory();   

// DATA

protected:

//	nsCID mClassID;
//	char* mClassName;
//	char* mProgID;
};   

//----------------------------------------------------------------------------------------
nsPrefWindowFactory::nsPrefWindowFactory(
	/*const nsCID &aClass,
	const char* aClassName,
	const char* aProgID*/)
//----------------------------------------------------------------------------------------
//  : mClassID(aClass)
//  , mClassName(nsCRT::strdup(aClassName))
//  , mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();
}   

//----------------------------------------------------------------------------------------
nsPrefWindowFactory::~nsPrefWindowFactory()   
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
//	nsCRT::free(mClassName);
//	nsCRT::free(mProgID);
}   

NS_IMPL_ISUPPORTS(nsPrefWindowFactory, nsIFactory::GetIID());

//----------------------------------------------------------------------------------------
nsresult nsPrefWindowFactory::CreateInstance(
	nsISupports* /* aOuter */,
	const nsIID &aIID,
	void** aResult)  
//----------------------------------------------------------------------------------------
{  
	if (aResult == nsnull)  
		return NS_ERROR_NULL_POINTER;  
	*aResult = nsnull;  
  
//	if (!mClassID.Equals(nsPrefWindow::GetCID())) 
//		return NS_NOINTERFACE;

	*aResult = nsPrefWindow::Get();
	return NS_OK;
}  // nsPrefWindowFactory::CreateInstanc

//----------------------------------------------------------------------------------------
nsresult nsPrefWindowFactory::LockFactory(PRBool aLock)  
//----------------------------------------------------------------------------------------
{  
	if (aLock) 
		PR_AtomicIncrement(&gLockCount); 
	else
		PR_AtomicDecrement(&gLockCount); 
	return NS_OK;
}  // nsPrefWindowFactory::LockFactory

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSGetFactory(
	nsISupports* /* aServMgr */,
	const nsCID &aClass,
	const char *aClassName,
	const char *aProgID,
	nsIFactory **aFactory)
//----------------------------------------------------------------------------------------
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	// If we decide to implement multiple factories, then we need to
	// check the class type here and create the appropriate factory.
	*aFactory = new nsPrefWindowFactory(/*aClass, aClassName, aProgID*/);
	if (!aFactory)
		return NS_ERROR_OUT_OF_MEMORY;
	return (*aFactory)->QueryInterface(nsIFactory::GetIID(), (void**)aFactory);
		// they want a Factory Interface so give it to them
} // NSGetFactory

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* /* aServMgr */) 
//----------------------------------------------------------------------------------------
{
    return PRBool(!nsPrefWindow::InstanceExists() && gLockCount == 0);
} // NSCanUnload

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* /*aServMgr*/, const char* path)
//----------------------------------------------------------------------------------------
{
	nsresult rv;
//	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
//	if (NS_FAILED(rv))
//		return rv;
	NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv))
		return rv;
#ifdef NS_DEBUG
	printf("Registering the PrefsWindow\n");
#endif
	rv = compMgr->RegisterComponent(
		nsPrefWindow::GetCID(),
		"nsPrefWindow", // classname
		NS_PREFWINDOW_PROGID, // progid
        path,
        PR_TRUE, // replace
        PR_TRUE // persist
        );
	return rv;
} // NSRegisterSelf

//----------------------------------------------------------------------------------------
extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* /*aServMgr*/, const char* path)
//----------------------------------------------------------------------------------------
{
	nsresult rv;
//	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
//	if (NS_FAILED(rv))
//		return rv;
	NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv))
		return rv;
	rv = compMgr->UnregisterComponent(nsPrefWindow::GetCID(), path);
	return rv;
} // NSUnregisterSelf
