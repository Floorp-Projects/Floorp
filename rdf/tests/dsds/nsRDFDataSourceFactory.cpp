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
#include "nsRDFDataSourceDS.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"

#ifdef NS_DEBUG
#include <stdio.h>
#endif

static NS_DEFINE_CID(kRDFDataSourceCID, NS_RDFDATASOURCEDATASOURCE_CID);
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
  if (aClass.Equals(kRDFDataSourceCID))
    rv = NS_NewGenericFactory(&fact, NS_NewRDFDataSourceDataSource);
  else
    rv = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv))
    *aFactory = fact;

#ifdef DEBUG_alecf
  printf("nsRDFDataSource's NSGetFactory!\n");
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

  rv = compMgr->RegisterComponent(kRDFDataSourceCID,
                                  "Generic DataSource DataSource",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "datasource",
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

    rv = compMgr->UnregisterComponent(kRDFDataSourceCID, aPath);

    return rv;
}
