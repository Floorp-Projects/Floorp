/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 * 
 */

// Local Includes 
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsUrlbarHistory.h"

// Helper Classes
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsISimpleEnumerator.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFCUtilsCID, NS_RDFCONTAINERUTILS_CID);

static const char * const ignoreArray[] = {
		"http://",
	    "ftp://",
		"gopher://",
		"www.",
		"http://www.",
		"https://",
		"https://www.",
		"keyword:",
		"://",
		"//",
		"\\",
		":\\",
		"file:///",
		".com",
		".org",
		".net",
		".",
		"?",
		"&",
		"="
	};
static nsIRDFResource * kNC_CHILD;
static nsIRDFResource * kNC_URLBARHISTORY;
static nsIRDFService * gRDFService;
static nsIRDFContainerUtils * gRDFCUtils;

static nsIPref * gPrefs;

//*****************************************************************************
//***    nsUrlbarHistory: Object Management
//*****************************************************************************

nsUrlbarHistory::nsUrlbarHistory():mLength(0)
{
   PRInt32 cnt = sizeof(ignoreArray)/sizeof(char *);
   for(PRInt32 i=0; i< cnt; i++) 
     mIgnoreArray.AppendElement((void *) new nsString(NS_ConvertASCIItoUCS2(ignoreArray[i])));
   
   nsresult res;

   res = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService),
	                                            (nsISupports **)&gRDFService);
   res = nsServiceManager::GetService(kRDFCUtilsCID, NS_GET_IID(nsIRDFContainerUtils),
	                                            (nsISupports **)&gRDFCUtils);
   if (gRDFService) {
	   //printf("$$$$ Got RDF SERVICE $$$$\n");
     res = gRDFService->GetDataSource("rdf:localstore", getter_AddRefs(mDataSource));

	 res = gRDFService->GetResource("http://home.netscape.com/NC-rdf#child", &kNC_CHILD);
	 res = gRDFService->GetResource("nc:urlbar-history", &kNC_URLBARHISTORY);
   }
   // Get the pref service
   res = nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref),
	                                    (nsISupports **) &gPrefs);
#if DEBUG_radha
   if (gPrefs)
	   printf("***** Got the pref service *****\n");
#endif
}


nsUrlbarHistory::~nsUrlbarHistory()
{
	//Entries are now in RDF
    //ClearHistory();
	PRInt32 cnt = sizeof(ignoreArray)/sizeof(char *);
    for(PRInt32 j=0; j< cnt; j++)  {
		nsString * ignoreEntry = (nsString *) mIgnoreArray.ElementAt(j);
	    delete ignoreEntry;
	}
	mIgnoreArray.Clear();
	if (gRDFService)
    {
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
    if (gRDFCUtils)
    {
        nsServiceManager::ReleaseService(kRDFCUtilsCID, gRDFCUtils);
        gRDFCUtils = nsnull;
    }
	mDataSource = nsnull;
	NS_IF_RELEASE(kNC_URLBARHISTORY);
	NS_IF_RELEASE(kNC_CHILD);
    if (gPrefs) {
		nsServiceManager::ReleaseService(kPrefServiceCID, gPrefs);
	    gPrefs = nsnull;
	}
}

//*****************************************************************************
//    nsUrlbarHistory: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsUrlbarHistory)
NS_IMPL_RELEASE(nsUrlbarHistory)

NS_INTERFACE_MAP_BEGIN(nsUrlbarHistory)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIUrlbarHistory)
   NS_INTERFACE_MAP_ENTRY(nsIUrlbarHistory)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsUrlbarHistory: nsISHistory
//*****************************************************************************
	NS_IMETHODIMP
nsUrlbarHistory::ClearHistory()
{
  /* Get the elements in the data source through the container
   */
  nsCOMPtr<nsIRDFContainer> container;
  gRDFCUtils->MakeSeq(mDataSource,
                      kNC_URLBARHISTORY,
                      getter_AddRefs(container));
 
  NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);

  /* Remove all the elements, back to front, to avoid O(n^2) updates
   * from RDF.
   */
  PRInt32 count = 0;
  container->GetCount(&count);
  for (PRInt32 i = count; i >= 1; --i) {
    nsCOMPtr<nsIRDFNode> dummy;
    container->RemoveElementAt(i, PR_TRUE, getter_AddRefs(dummy));
  }

#ifdef DEBUG
  container->GetCount(&count);
  NS_ASSERTION(count == 0, "count != 0 after clearing history");
#endif

  return NS_OK;
}


/* Get size of the history list */
NS_IMETHODIMP
nsUrlbarHistory::GetCount(PRInt32 * aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsIRDFContainer> container;
  gRDFCUtils->MakeSeq(mDataSource, 
                      kNC_URLBARHISTORY,
                      getter_AddRefs(container));

  NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);
  return container->GetCount(aResult);
}


NS_IMETHODIMP
nsUrlbarHistory::PrintHistory()
{
	for (PRInt32 i=0; i<mLength; i++) {
       nsString * entry = nsnull;
	   entry = (nsString *) mArray.ElementAt(i);
	   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
	   char * cEntry;
	   cEntry = ToNewCString(*entry);
	   printf("Entry at index %d is %s\n", i, cEntry);
	   Recycle(cEntry);
	}

  return NS_OK;
}
