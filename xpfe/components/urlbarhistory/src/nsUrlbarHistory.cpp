/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsUrlbarHistory.h"

// Helper Classes
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsString.h"
#include "nsIAutoCompleteResults.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static char * ignoreArray[] = {
		"http://",
	    "ftp://",
		"www.",
		"http://www.",
        "keyword:"
	};
static nsIRDFResource * kNC_CHILD;
static nsIRDFResource * kNC_URLBARHISTORY;
static nsIRDFService * gRDFService;


//*****************************************************************************
//***    nsUrlbarHistory: Object Management
//*****************************************************************************

nsUrlbarHistory::nsUrlbarHistory():mLength(0)
{
   NS_INIT_REFCNT();
   PRInt32 cnt = sizeof(ignoreArray)/sizeof(char *);
   for(PRInt32 i=0; i< cnt; i++) 
     mIgnoreArray.AppendElement((void *) new nsString(NS_ConvertASCIItoUCS2(ignoreArray[i])));
   
   nsresult res;

   //nsIRDFService* rdfService;
   res = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService),
	                                            (nsISupports **)&gRDFService);
   if (gRDFService) {
	   //printf("$$$$ Got RDF SERVICE $$$$\n");
     res = gRDFService->GetDataSource("rdf:localstore", getter_AddRefs(mDataSource));

	 res = gRDFService->GetResource("http://home.netscape.com/NC-rdf#child", &kNC_CHILD);
	 res = gRDFService->GetResource("nc:urlbar-history", &kNC_URLBARHISTORY);
   }

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
	mDataSource = nsnull;
	NS_IF_RELEASE(kNC_URLBARHISTORY);
	NS_IF_RELEASE(kNC_CHILD);
}

//*****************************************************************************
//    nsUrlbarHistory: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsUrlbarHistory)
NS_IMPL_RELEASE(nsUrlbarHistory)

NS_INTERFACE_MAP_BEGIN(nsUrlbarHistory)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIUrlbarHistory)
   NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSession)
   NS_INTERFACE_MAP_ENTRY(nsIUrlbarHistory)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsUrlbarHistory: nsISHistory
//*****************************************************************************
	NS_IMETHODIMP
nsUrlbarHistory::ClearHistory()
{
     nsCOMPtr<nsISimpleEnumerator>    entries;
  	 nsresult rv = mDataSource->GetTargets(kNC_URLBARHISTORY,
                                    kNC_CHILD,
                                    PR_TRUE,
									getter_AddRefs(entries));
     NS_ENSURE_TRUE(entries, NS_ERROR_FAILURE);

	 PRBool moreElements = PR_FALSE;

     while (NS_SUCCEEDED(entries->HasMoreElements(&moreElements)) && (moreElements == PR_TRUE)) {
	  // printf("nsUrlbarHistory::ClearHistory Entries has more elements\n");
       nsCOMPtr<nsISupports> baseNode;
	   nsCOMPtr<nsIRDFNode> node;
    
	   entries->GetNext(getter_AddRefs(baseNode));
       if (baseNode) {
		 //printf("Got a node\n");
         node = do_QueryInterface(baseNode);
		 if (node) {
		    rv = mDataSource->Unassert(kNC_URLBARHISTORY,
			                        kNC_CHILD,
									node);
}
   }
	 } // while
	   return NS_OK;
   }


/* Get size of the history list */
NS_IMETHODIMP
nsUrlbarHistory::GetCount(PRInt32 * aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	PRInt32   ubhCount = 0;

	printf("In nsUrlbarHistory::GetCount\n");
	nsCOMPtr<nsISimpleEnumerator>    entries;
  	(void)mDataSource->GetTargets(kNC_URLBARHISTORY,
                                    kNC_CHILD,
                                    PR_TRUE,
									getter_AddRefs(entries));
    NS_ENSURE_TRUE(entries, NS_ERROR_FAILURE);

	PRBool moreElements = PR_FALSE;

    while (NS_SUCCEEDED(entries->HasMoreElements(&moreElements)) && (moreElements == PR_TRUE)) {
		nsCOMPtr<nsISupports>   entry;
		entries->GetNext(getter_AddRefs(entry));
		ubhCount ++;
	 }  // while
   
	printf("In nsUrlbarHistory:: Out of the while loop\n");

	*aResult = ubhCount;
    return NS_OK;
}


NS_IMETHODIMP
nsUrlbarHistory::PrintHistory()
{
	for (PRInt32 i=0; i<mLength; i++) {
       nsString * entry = nsnull;
	   entry = (nsString *) mArray.ElementAt(i);
	   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
	   char * cEntry;
	   cEntry = entry->ToNewCString();
	   printf("Entry at index %d is %s\n", i, cEntry);
	   Recycle(cEntry);
	}

  return NS_OK;
}



NS_IMETHODIMP
nsUrlbarHistory::OnStartLookup(const PRUnichar *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    nsresult rv = NS_OK;
 
    if (!listener)
        return NS_ERROR_NULL_POINTER;
      

    if (uSearchString[0] == 0)
    {
        listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
        return NS_OK;
    }
 
    // Check if it is one of the generic strings "http://, www., ftp:// etc..
    PRInt32 cnt = mIgnoreArray.Count();
	for(PRInt32 i=0; i<cnt; i++) {
       nsString * match = (nsString *)mIgnoreArray.ElementAt(i);
	   
	   if (match) {
          PRInt32 index = match->Find(uSearchString, PR_TRUE);
		  if (index == 0) {
			  listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
		      return NS_OK;
		  }
	   }  // match
	}  //for
    
    nsCOMPtr<nsIAutoCompleteResults> results;
	/* Don't call SearchPreviousResults. It is buggy 
    if (NS_FAILED(SearchPreviousResults(uSearchString, previousSearchResult)))
    {
        results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_PROGID);
		NS_ENSURE_TRUE(results, NS_ERROR_FAILURE);
        rv = SearchCache(uSearchString, results);       
    }
    else
        results = previousSearchResult;
      */
	
	results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_PROGID);
	NS_ENSURE_TRUE(results, NS_ERROR_FAILURE);
    rv = SearchCache(uSearchString, results);    

    AutoCompleteStatus status = nsIAutoCompleteStatus::failed;
    if (NS_SUCCEEDED(rv))
    {
        PRBool addedDefaultItem = PR_FALSE;

        results->SetSearchString(uSearchString);
        results->SetDefaultItemIndex(-1);

        nsCOMPtr<nsISupportsArray> array;
        rv = results->GetItems(getter_AddRefs(array));
        if (NS_SUCCEEDED(rv))
        {
            PRUint32 nbrOfItems;
            rv = array->Count(&nbrOfItems);
            if (NS_SUCCEEDED(rv)) {
                if (nbrOfItems > 1)
                {
                    results->SetDefaultItemIndex(addedDefaultItem ? 1 : 0);
                    status = nsIAutoCompleteStatus::matchFound;
                }
                else {
                    if (nbrOfItems == 1)
                    {
                        results->SetDefaultItemIndex(0);
                        status = nsIAutoCompleteStatus::matchFound;
                    }
                    else
                        status = nsIAutoCompleteStatus::noMatch;
				}
			}  // NS_SUCCEEDED(rv)
        }
    listener->OnAutoComplete(results, status);
	}

    return NS_OK;
}

NS_IMETHODIMP
nsUrlbarHistory::SearchPreviousResults(const PRUnichar *searchStr, nsIAutoCompleteResults *previousSearchResult)
{
    if (!previousSearchResult)
        return NS_ERROR_NULL_POINTER;
        
    nsXPIDLString prevSearchString;
    PRUint32 searchStrLen = nsCRT::strlen(searchStr);
    nsresult rv;
    nsAutoString   searchAutoStr(searchStr);

    rv = previousSearchResult->GetSearchString(getter_Copies(prevSearchString));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    
    if (!(const PRUnichar*)prevSearchString)
        return NS_ERROR_FAILURE;
    
    PRUint32 prevSearchStrLen = nsCRT::strlen(prevSearchString);
    if (searchStrLen < prevSearchStrLen || nsCRT::strncasecmp(searchStr, prevSearchString, prevSearchStrLen != 0))
        return NS_ERROR_ABORT;

    nsCOMPtr<nsISupportsArray> array;
    rv = previousSearchResult->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 nbrOfItems;
        PRUint32 i;
        
        rv = array->Count(&nbrOfItems);
        if (NS_FAILED(rv) || nbrOfItems <= 0)
            return NS_ERROR_FAILURE;
        
	    nsCOMPtr<nsISupports> item;
	    nsCOMPtr<nsIAutoCompleteItem> resultItem;
        

	    for (i = 0; i < nbrOfItems; i ++)
	    {
	        rv = array->QueryElementAt(i, nsIAutoCompleteItem::GetIID(), getter_AddRefs(resultItem));
	        if (NS_FAILED(rv))
                return NS_ERROR_FAILURE;

	        PRUnichar *  itemValue=nsnull;
            resultItem->GetValue(&itemValue);
			nsAutoString itemAutoStr(itemValue);

            //printf("SearchPreviousResults::Comparing %s with %s \n", searchAutoStr.ToNewCString(), itemAutoStr.ToNewCString());
			if (!itemValue)
				continue;
		    if (nsCRT::strncasecmp(searchStr, itemValue, searchStrLen) == 0)
			{
			    Recycle(itemValue);
			    continue;
			}
			
	    }
	    return NS_OK;
    }

    return NS_ERROR_ABORT;
}


NS_IMETHODIMP
nsUrlbarHistory::SearchCache(const PRUnichar* searchStr, nsIAutoCompleteResults* results)
{
    nsresult rv = NS_OK;
	nsCOMPtr<nsISimpleEnumerator>  entries;
    PRUnichar * rdfValue = nsnull;
    nsAutoString searchAutoStr(searchStr);
	//char * searchCSTR = searchAutoStr.ToNewCString();
	   PRUnichar * match = nsnull;
	   PRInt32 index = -1;
	   
//	printf("In SEARCHCACHE searching for %s\n", searchCSTR);
	if (!gRDFService || !kNC_URLBARHISTORY || !kNC_CHILD || !mDataSource)
		return NS_ERROR_FAILURE;

	rv = mDataSource->GetTargets(kNC_URLBARHISTORY,
                                 kNC_CHILD,
                                 PR_TRUE,
								 getter_AddRefs(entries));
     NS_ENSURE_TRUE(entries, NS_ERROR_FAILURE);

	 PRBool moreElements = PR_FALSE;
     while (NS_SUCCEEDED(entries->HasMoreElements(&moreElements)) && moreElements) {
	    nsCOMPtr<nsISupports>  entry;
		nsCOMPtr<nsIRDFLiteral>  literal;
		nsAutoString rdfAStr;

		rv = entries->GetNext(getter_AddRefs(entry));
		if (entry) {
           literal = do_QueryInterface(entry);
           literal->GetValue(&rdfValue);
		}
		if (rdfValue) {
		    rdfAStr = (rdfValue);
			index = rdfAStr.Find(searchStr, PR_TRUE);
			match = rdfAStr.ToNewUnicode();
			//  printf("SearchCache Round I-found item %s in rdf\n", rdfAStr.ToNewCString());
	   }
	   if (index < 0) {
		   // strip off any http:// ftp:// and see if that matches.
		   char * searchCString = nsnull;
		   searchCString = searchAutoStr.ToNewCString();
           if (searchCString) {
             char * searchSubStr = PL_strstr(searchCString, "//");
			 if (searchSubStr) {
			   searchSubStr++;searchSubStr++;
		  //     printf("SearchCache::Comparing %s with %s\n", searchSubStr, item->ToNewCString());
		        index = rdfAStr.Find(searchSubStr, PR_TRUE);
		       if (match)
			     Recycle(match);
		        match = rdfAStr.ToNewUnicode();
			    //printf("SearchCache Round II-found item %s in rdf\n", rdfAStr.ToNewCString());
			 }
		   }
		   Recycle(searchCString);
	   }

	   if (index >=0) {
           // Item found. Create an AutoComplete Item 
		   nsCOMPtr<nsIAutoCompleteItem> newItem(do_CreateInstance(NS_AUTOCOMPLETEITEM_PROGID));
		   NS_ENSURE_TRUE(newItem, NS_ERROR_FAILURE);
           
           newItem->SetValue(match);
           nsCOMPtr<nsISupportsArray> array;
           rv = results->GetItems(getter_AddRefs(array));
           if (NS_SUCCEEDED(rv))
		   { 
			  // printf("Appending element %s to the results array\n", item->ToNewCString());
                array->AppendElement((nsISupports*)newItem);
		   }		  
	   }
	   Recycle(rdfValue);
	   Recycle(match);
	}  //while

    return rv;
}


NS_IMETHODIMP
nsUrlbarHistory::OnStopLookup()
{
    return NS_OK;
}

NS_IMETHODIMP
nsUrlbarHistory::OnAutoComplete(const PRUnichar *searchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
	return NS_OK;
}
