/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*-
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

#ifndef __nsProxiedServiceManager_h_
#define __nsProxiedServiceManager_h_

#include "nsIServiceManager.h"
#include "nsProxyObjectManager.h"

////////////////////////////////////////////////////////////////////////////////
// NS_WITH_PROXIED_SERVICE: macro to make using services that need to be proxied
//									 before using them easier. 
// Now you can replace this:
// {
//		nsresult rv;
//		NS_WITH_SERVICE(nsIMyService, pIMyService, kMyServiceCID, &rv);
//		if(NS_FAILED(rv))
//			return;
//		NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
//		if(NS_FAILED(rv))
//			return;
//		nsIMyService pIProxiedObject = NULL;
//		rv = pIProxyObjectManager->GetProxyObject(pIProxyQueue, 
//																nsIMyService::GetIID(), 
//																pIMyService, PROXY_SYNC,
//																(void**)&pIProxiedObject);
//		pIProxiedObject->DoIt(...);  // Executed on same thread as pIProxyQueue
//		...
//		pIProxiedObject->Release();  // Must be done as not managed for you.
//		}
//	with this:
//		{
//		nsresult rv;
//		NS_WITH_PROXIED_SERVICE(nsIMyService, pIMyService, kMyServiceCID, 
//										pIProxyQueue, &rv);
//		if(NS_FAILED(rv))
//			return;
//		pIMyService->DoIt(...);  // Executed on the same thread as pIProxyQueue
//		}
// and the automatic destructor will take care of releasing the service and
// the proxied object for you. 
// 
// Note that this macro requires you to link with the xpcom DLL to pick up the
// static member functions from nsServiceManager.

#define NS_WITH_PROXIED_SERVICE(T, var, cid, Q, rvAddr)		\
	nsProxiedService _serv##var(cid, T::GetIID(), Q, rvAddr);			\
	T* var = (T*)(nsISupports*)_serv##var;

////////////////////////////////////////////////////////////////////////////////
// nsProxiedService
////////////////////////////////////////////////////////////////////////////////

class nsProxiedService : public nsService
{
protected:
	nsISupports* mProxiedService;
public:
  nsProxiedService(const nsCID &aClass, const nsIID &aIID, 
			nsIEventQueue* pIProxyQueue, nsresult*rv): nsService(aClass, aIID, rv),
			mProxiedService(nsnull)
	{
	static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

	if(NS_FAILED(*rv))
		return;
	NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
						kProxyObjectManagerCID, rv);
	if(NS_FAILED(*rv))
		{
		nsServiceManager::ReleaseService(mCID, mService);
		mService = nsnull;
		return;
		}

	*rv = pIProxyObjectManager->GetProxyObject(pIProxyQueue,
															aIID,
															mService,
															PROXY_SYNC,
															(void**)&mProxiedService);
	if(NS_FAILED(*rv))
		{
		nsServiceManager::ReleaseService(mCID, mService);
		mService = nsnull;
		return;
		}
	}
	
	~nsProxiedService()
		{
		if(mProxiedService)
			NS_RELEASE(mProxiedService);
		// Base class will free mService
		}

	nsISupports* operator->() const
		{
		NS_PRECONDITION(mProxiedService != 0, "Your code should test the error result from the constructor.");
		return mProxiedService;
		}

	PRBool operator==(const nsISupports* other)
		{
		return ((mProxiedService == other) || (mService == other));
		}

	operator nsISupports*() const
		{
		return mProxiedService;
		}
};



#endif //__nsProxiedServiceManager_h_