/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIGenericFactory.h"
#include "nsRDFDOMDataSource.h"
#include "nsRDFDOMResourceFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"

#ifdef NS_DEBUG
#include <stdio.h>
#endif

static NS_DEFINE_CID(kRDFDOMDataSourceCID, NS_RDF_DOMDATASOURCE_CID);
static NS_DEFINE_CID(kRDFDOMResourceFactoryCID, NS_RDF_DOMRESOURCEFACTORY_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char* aClassName,
             const char* aProgID,
             nsIFactory **aFactory)
{
  nsresult rv=NS_OK;
  nsIGenericFactory* fact;
  if (aClass.Equals(kRDFDOMDataSourceCID))
    rv = NS_NewGenericFactory(&fact, NS_NewRDFDOMDataSource);
  if (aClass.Equals(kRDFDOMDataSourceCID))
    rv = NS_NewGenericFactory(&fact, NS_NewRDFDOMResourceFactory);
  else
    rv = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv))
    *aFactory = fact;

#ifdef DEBUG_alecf
  printf("nsRDFDOMDataSource's NSGetFactory!\n");
#endif
  return rv;
}

nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  NS_WITH_SERVICE1(nsIComponentManager,
                   compMgr,
                   aServMgr,
                   kComponentManagerCID,
                   &rv);
  
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kRDFDOMDataSourceCID,
                                  "Generic DataSource DataSource",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "domds",
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kRDFDOMResourceFactoryCID,
                                  "DOM element resource factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "dom",
                                  aPath, PR_TRUE, PR_TRUE);
  return rv;

}

nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kRDFDOMDataSourceCID, aPath);
    rv = compMgr->UnregisterComponent(kRDFDOMResourceFactoryCID, aPath);

    return rv;
}
