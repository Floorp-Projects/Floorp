/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/** Wrapper class for Global History implementation. This class
 *  primarily passes on the data to the nsIHistoryDataSource interface
 */

#include "nsIGlobalHistory.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIHistoryDataSource.h"
#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "plstr.h"



/* Define Class IDs */
static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

/* Define Interface IDs */

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_IID(kIGlobalHistoryIID,   NS_IGLOBALHISTORY_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,   NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,      NS_IRDFSERVICE_IID);


class nsGlobalHistory : public nsIGlobalHistory
{
public:
  nsGlobalHistory(void);

  // nsISupports methods 
  NS_DECL_ISUPPORTS

  // nsIGlobalHistory
  NS_IMETHOD AddPage (const char* aURI, const char* aRefererURI, PRTime aDate);
  NS_IMETHOD SetPageTitle (const char* aURI, const PRUnichar* aTitle);
  NS_IMETHOD RemovePage (const char* aURI);
  NS_IMETHOD GetLastVisitDate (const char* aURI, uint32 *aDate);
  NS_IMETHOD CompleteURL (const char* aPrefix, char** aPreferredCompletions);
  NS_IMETHOD Init(void);

private:
   virtual ~nsGlobalHistory();
   nsIHistoryDataSource * mGHistory;

};


nsGlobalHistory::nsGlobalHistory()
{
  NS_INIT_REFCNT();
  mGHistory = nsnull;
}

nsGlobalHistory::~nsGlobalHistory()
{
  mGHistory = nsnull;
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(nsGlobalHistory, kIGlobalHistoryIID);


NS_IMETHODIMP
nsGlobalHistory::Init(void)
{

  /* Create a RDF Data source for history */
  nsIRDFService* rdf;
  if (NS_SUCCEEDED(nsServiceManager::GetService(kRDFServiceCID,
                                                nsIRDFService::GetIID(),
                                                (nsISupports**) &rdf))) {
    nsCOMPtr<nsIRDFDataSource> ds;
    static const char* kHistoryDataSourceURI = "rdf:history";
    if (NS_SUCCEEDED(rdf->GetDataSource(kHistoryDataSourceURI,
                                        getter_AddRefs(ds)))) {
      ds->QueryInterface(nsIHistoryDataSource::GetIID(), (void**) &mGHistory);
    }
    nsServiceManager::ReleaseService(kRDFServiceCID, rdf);
  }
  
  return NS_OK;	
}

NS_IMETHODIMP
nsGlobalHistory::AddPage(const char * aURI, const char * aRefererURI, PRTime aDate)
{
  nsresult rv = NS_OK;
      if (NS_FAILED(rv = mGHistory->AddPage(aURI, /* XXX referrer? */ (char *) nsnull, PR_Now()))) {
          NS_ERROR("unable to add page to history");
          return rv;
      }
      else
        printf("Added page %s to the rdf:history datasource\n", aURI);
  return rv;

}

NS_IMETHODIMP
nsGlobalHistory::SetPageTitle (const char* aURI, const PRUnichar* aTitle)
{
  nsresult  rv=nsnull;
  if (NS_FAILED(rv = mGHistory->SetPageTitle(aURI, aTitle))) {
        NS_ERROR("unable to set doc title");
        return rv;
  }
  return rv;

}


NS_IMETHODIMP
nsGlobalHistory::RemovePage (const char* aURI)
{
   nsresult  rv = NS_OK;
   mGHistory->RemovePage(aURI);
   return rv;
	
}

NS_IMETHODIMP
nsGlobalHistory::GetLastVisitDate (const char* aURI, uint32 *aResult)
{

    if (nsnull == aResult)
        return NS_ERROR_NULL_POINTER;

    mGHistory->GetLastVisitDate(aURI, aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::CompleteURL (const char* aPrefix, char** aResult)
{
    if (nsnull == aResult)
      return NS_ERROR_NULL_POINTER;
    mGHistory->CompleteURL(aPrefix, aResult);

    return NS_OK;
}


NS_EXPORT nsresult NS_NewGlobalHistory(nsIGlobalHistory** aResult)
{
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = new nsGlobalHistory();
  if (nsnull == *aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}



//----------------------------------------------------------------------

// Factory code for creating nsGlobalHistory

class nsGlobalHistoryFactory : public nsIFactory
{
public:
  nsGlobalHistoryFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);  
protected:
  virtual ~nsGlobalHistoryFactory();
};


nsGlobalHistoryFactory::nsGlobalHistoryFactory()
{
  NS_INIT_REFCNT();
}

nsresult
nsGlobalHistoryFactory::LockFactory(PRBool lock)
{

  return NS_OK;
}

nsGlobalHistoryFactory::~nsGlobalHistoryFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsGlobalHistoryFactory, kIFactoryIID);


nsresult
nsGlobalHistoryFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsGlobalHistory* inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsGlobalHistory);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}


nsresult
NS_NewGlobalHistoryFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsGlobalHistoryFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
