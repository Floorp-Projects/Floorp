/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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

#include "nsRDFCID.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIRDFDataSource.h"
#include "nsISearchService.h"

static NS_DEFINE_CID(kLocalSearchServiceCID,         NS_RDFFINDDATASOURCE_CID);
static NS_DEFINE_CID(kInternetSearchServiceCID,      NS_RDFSEARCHDATASOURCE_CID);
static NS_DEFINE_CID(kComponentManagerCID,           NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,             NS_GENERICFACTORY_CID);

NS_IMETHODIMP	NS_NewLocalSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult);
NS_IMETHODIMP	NS_NewInternetSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult);



////////////////////////////////////////////////////////////////////////
// Component Exports



extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
	NS_PRECONDITION(aFactory != nsnull, "null ptr");
	if (! aFactory)
		return NS_ERROR_NULL_POINTER;

	nsIGenericFactory::ConstructorProcPtr constructor;

	if (aClass.Equals(kInternetSearchServiceCID)) {
		constructor = NS_NewInternetSearchService;
	}
	else if (aClass.Equals(kLocalSearchServiceCID)) {
		constructor = NS_NewLocalSearchService;
	}
	else {
		*aFactory = nsnull;
		return NS_NOINTERFACE; // XXX
	}

	nsresult rv;
	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIGenericFactory> factory;
	rv = compMgr->CreateInstance(kGenericFactoryCID,
				     nsnull,
				     nsIGenericFactory::GetIID(),
				     getter_AddRefs(factory));

	if (NS_FAILED(rv)) return rv;

	rv = factory->SetConstructor(constructor);
	if (NS_FAILED(rv)) return rv;

	*aFactory = factory;
	NS_ADDREF(*aFactory);
	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kLocalSearchServiceCID, "LocalSearch",
					NS_LOCALSEARCH_SERVICE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kLocalSearchServiceCID, "LocalSearch",
					NS_LOCALSEARCH_DATASOURCE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kInternetSearchServiceCID, "InternetSearch",
					NS_INTERNETSEARCH_SERVICE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kInternetSearchServiceCID, "InternetSearch",
					NS_INTERNETSEARCH_DATASOURCE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kLocalSearchServiceCID, aPath);
	rv = compMgr->UnregisterComponent(kInternetSearchServiceCID, aPath);

	return NS_OK;
}
