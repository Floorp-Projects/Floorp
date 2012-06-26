/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIGenericFactory.h"
#include "nsRDFDataSourceDS.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"

#ifdef DEBUG
#include <stdio.h>
#endif

static NS_DEFINE_CID(kRDFDataSourceCID, NS_RDFDATASOURCEDATASOURCE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char* aClassName,
             const char* aContractID,
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
  nsCOMPtr<nsIComponentManager> compMgr =
           do_GetService(kComponentManagerCID, aServMgr, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kRDFDataSourceCID,
                                  "Generic DataSource DataSource",
                                  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "datasource",
                                  aPath, true, true);

  return rv;

}

nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  nsCOMPtr<nsIComponentManager> compMgr =
         do_GetService(kComponentManagerCID, aServMgr, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kRDFDataSourceCID, aPath);

  return rv;
}
