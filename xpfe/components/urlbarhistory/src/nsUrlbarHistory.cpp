/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 0 -*-
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
#include "nsIAutoCompleteResults.h"
#include "nsISimpleEnumerator.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFCUtilsCID, NS_RDFCONTAINERUTILS_CID);

static char * ignoreArray[] = {
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
#define PREF_AUTOCOMPLETE_ENABLED "browser.urlbar.autocomplete.enabled"



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
   NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSession)
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



NS_IMETHODIMP
nsUrlbarHistory::OnStartLookup(const PRUnichar *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    nsresult rv = NS_OK;
 
    if (!listener)
        return NS_ERROR_NULL_POINTER;
      
	PRBool enabled  = PR_FALSE;
	if (gPrefs) {
       rv = gPrefs->GetBoolPref(PREF_AUTOCOMPLETE_ENABLED, &enabled);      
	}

	if (!enabled) {// urlbar autocomplete is not enabled
		listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
		return NS_OK;
	}
 //   printf("Autocomplete is enabled\n");   

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
        results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID);
		NS_ENSURE_TRUE(results, NS_ERROR_FAILURE);
        rv = SearchCache(uSearchString, results);       
    }
    else
        results = previousSearchResult;
      */
	
	results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID);
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
    if (searchStrLen < prevSearchStrLen ||
        Compare(nsDependentString(searchStr),
                nsDependentString(prevSearchString, prevSearchStrLen),
                nsCaseInsensitiveStringComparator())!= 0)
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

	        nsAutoString itemValue;
            resultItem->GetValue(itemValue);

			if (itemValue.IsEmpty())
				continue;
		    if (Compare(nsDependentString(searchStr, searchStrLen),
                        itemValue,
                        nsCaseInsensitiveStringComparator()) == 0)
			    continue;
			
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

    //printf("******** In SearchCache *******\n");
    nsAutoString searchAutoStr(searchStr);
	
	PRInt32  protocolIndex=-1;
    nsAutoString searchProtocol, searchPath, resultAutoStr;
    PRInt32 searchPathIndex, searchStrLength;

    // Get the length of the search string
    searchStrLength = searchAutoStr.Length();
    // Check if there is any protocol present in the 
    // search string.
    GetHostIndex(searchStr, &searchPathIndex);
    if (searchPathIndex > 0) {
        // There was a protocol in the search string. Strip off
        // the protocol from the rest of the url  
        searchAutoStr.Left(searchProtocol, searchPathIndex);
        searchAutoStr.Mid(searchPath, searchPathIndex, searchStrLength);
    }
    else {
        // There was no protocol in the search string. 
        searchPath = searchAutoStr;
    }
	//printf("Search String is %s path = %s protocol = %s \n",
	//       NS_LossyConvertUCS2toASCII(searchAutoStr).get(),
	//       NS_LossyConvertUCS2toASCII(searchPath).get(),
	//       NS_LossyConvertUCS2toASCII(searchProtocol).get());
	   
	if (!gRDFCUtils || !kNC_URLBARHISTORY)
        return NS_ERROR_FAILURE;


	nsCOMPtr<nsIRDFContainer> container;
    /* Get the elements in the  data source
     * through the container
     */
	rv = gRDFCUtils->MakeSeq(mDataSource,
                            kNC_URLBARHISTORY,
                            getter_AddRefs(container));

    NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);
 
    //Get the elements from the container
    container->GetElements(getter_AddRefs(entries));

    NS_ENSURE_TRUE(entries, NS_ERROR_FAILURE);
    
    PRBool moreElements = PR_FALSE;
    while (NS_SUCCEEDED(entries->HasMoreElements(&moreElements)) && moreElements) {
       nsCOMPtr<nsISupports>  entry;
       nsCOMPtr<nsIRDFLiteral>  literal;
       nsAutoString rdfAutoStr;
       nsAutoString rdfProtocol, rdfPath;
       PRInt32 rdfLength, rdfPathIndex, index = -1;
       const PRUnichar * match = nsnull;
       const PRUnichar * rdfValue = nsnull;   

       rv = entries->GetNext(getter_AddRefs(entry));
       if (entry) {
         literal = do_QueryInterface(entry);
         literal->GetValueConst(&rdfValue);
       }      
       if (!rdfValue)
          continue;
       rdfAutoStr = rdfValue;
       rdfLength = rdfAutoStr.Length();        

       // Get the index of the hostname in the rdf string
       GetHostIndex (rdfValue, &rdfPathIndex);

       if (rdfPathIndex > 0) {
          // RDf string has a protocol in it, Strip it off
          // from the rest of the url;
          rdfAutoStr.Left(rdfProtocol, rdfPathIndex);
          rdfAutoStr.Mid(rdfPath, rdfPathIndex, rdfLength);
       }
       else {
          // There was no protocol.
          rdfPath = rdfAutoStr;
       }
       //printf("RDFString is %s path = %s protocol = %s\n",
	//       NS_LossyConvertUCS2toASCII(rdfAutoStr).get(),
	//       NS_LossyConvertUCS2toASCII(rdfPath).get(),
	//       NS_LossyConvertUCS2toASCII(rdfProtocol).get());
       // We have all the data we need. Let's do the comparison
       // We compare the path first and compare the protocol next
       index = rdfPath.Find(searchPath, PR_TRUE);       
       if (index == 0) {
           // The paths match. Now let's compare protocols
           if (searchProtocol.Length() && rdfProtocol.Length()) {
              // Both the strings have a protocol part. Compare them.
              protocolIndex = rdfProtocol.Find(searchProtocol);
              if (protocolIndex == 0) {
                  // Both protocols match. We found a result item
                  match = rdfAutoStr.get();
              } 
           } 
           else if (searchProtocol.Length() && (rdfProtocol.Length() <= 0)) {
               /* The searchString has a protocol but the rdf string doesn't
                * Check if the searchprotocol is the default "http://". If so,
                * prepend the searchProtocol to the rdfPath and provide that as 
                * the result. Otherwise we don't have a match
                */
               // XXX I guess  hard-coded "http://" is OK, since netlib considers
               // all urls to be char *
               if ((searchProtocol.Find("http://", PR_TRUE)) == 0) {
                  resultAutoStr = searchProtocol + rdfPath;
                  match = resultAutoStr.get();
               }
           }
           else if ((searchProtocol.Length() <=0) && rdfProtocol.Length() ||
                    (searchProtocol.Length() <=0) && (rdfProtocol.Length() <= 0)) {
               /* Provide the rdfPath (no protocol, just the www.xyz.com/... part) 
                * as the result for the following 2 cases.
                * a) searchString has no protocol but rdfString has protocol
                * b) Both searchString and rdfString don't have a protocol
                */ 
               match = rdfPath.get();
           }           
       }  // (index == 0)

      
	   
       if (match) {		   
           /* We have a result item.
            * First make sure that the value is not already
            * present in the results array. If we have  
            * www.mozilla.org and http://www.mozilla.org as unique addresses
            * in the history and due to the algorithm we follow above to find a match,
            * when the user user types, http://www.m, we would have 2 matches, both being,
            * http://www.mozilla.org. So, before adding an entry as a result,
            * make sure it is not already present in the resultarray
            */
           PRBool itemPresent = PR_FALSE;
           rv = CheckItemAvailability(match, results, &itemPresent);
           if (itemPresent) {
               continue;
           }
           //Create an AutoComplete Item 
           nsCOMPtr<nsIAutoCompleteItem> newItem(do_CreateInstance(NS_AUTOCOMPLETEITEM_CONTRACTID));
           NS_ENSURE_TRUE(newItem, NS_ERROR_FAILURE);
           
           newItem->SetValue(nsDependentString(match));
           nsCOMPtr<nsISupportsArray> array;
           rv = results->GetItems(getter_AddRefs(array));
           if (NS_SUCCEEDED(rv))
           { 
               // printf("Appending element %s to the results array\n", ToNewCString(*item)); // leaks
               array->AppendElement((nsISupports*)newItem);
           }
           /* The result may be much more than what was asked for. For example
            * the user types http://www.moz and we had http://www.mozilla.org/sidebar
            * as an entry in the history. This will match our selection criteria above
            * above and will be provided as result. But the user may actually want to 
            * go to http://www.mozilla.org. If we are such a situation, offer
            * http://www.mozilla.org (The first part of the result string http://www.mozilla.org/sidebar)
            * as a result option(probably the default result).
            */
            rv = VerifyAndCreateEntry(searchStr, match, results);
        }   

    }    // while 
    return rv;
}


NS_IMETHODIMP
nsUrlbarHistory::GetHostIndex(const PRUnichar * aPath, PRInt32 * aReturn)
{
    if (!aPath || !aReturn)
        return NS_ERROR_FAILURE;

    PRInt32 slashIndex=-1;    
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    
    rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUCS2toUTF8(aPath).get());
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURL> pathURL(do_QueryInterface(uri));

    if (pathURL) {
       char *  host=nsnull, *preHost = nsnull, * filePath = nsnull;
       pathURL->GetHost(&host);
       pathURL->GetFilePath(&filePath);
       pathURL->GetPreHost(&preHost);
       nsAutoString path(aPath);
       if (preHost) 
           slashIndex  = path.Find(preHost, PR_TRUE);
       else if (host)
           slashIndex = path.Find(host,PR_TRUE);
       else if (filePath)
           slashIndex = path.Find(filePath, PR_TRUE);
       else
           slashIndex = 0;
       
       nsCRT::free(preHost);
       nsCRT::free(host);
       nsCRT::free(filePath);
       //printf("$$$$ Scheme for uri = %s, preHost = %s, filepath = %s, Host = %s HostIndex = %d\n", pathScheme, preHost, filePath, pathHost, *aReturn);
    }

    *aReturn = slashIndex;

    return NS_OK;
}

NS_IMETHODIMP
nsUrlbarHistory::CheckItemAvailability(const PRUnichar * aItem, nsIAutoCompleteResults * aArray, PRBool * aResult)
{
    if (!aItem || !aArray)
        return PR_FALSE;
    
    nsresult rv;
    *aResult = PR_FALSE;

    nsCOMPtr<nsISupportsArray> array;
    rv = aArray->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 nbrOfItems=0;
        PRUint32 i;
        
        rv = array->Count(&nbrOfItems);
        // If there is no item found in the array, return false
        if (nbrOfItems <= 0)
            return PR_FALSE;
        
        nsCOMPtr <nsIAutoCompleteItem> resultItem;
        for (i = 0; i < nbrOfItems; i ++)
        {          
            rv = array->QueryElementAt(i, NS_GET_IID(nsIAutoCompleteItem),
                                       getter_AddRefs(resultItem));
            if (NS_FAILED(rv))
                return NS_ERROR_FAILURE;

            nsAutoString itemValue;
            resultItem->GetValue(itemValue);
            // Using nsIURI to do comparisons didn't quite work out.
            // So use nsCRT methods
            if (Compare(itemValue, nsDependentString(aItem),
                        nsCaseInsensitiveStringComparator()) == 0)
            {
                //printf("In CheckItemAvailability. Item already found\n");
                *aResult = PR_TRUE;
                break;
            }
        }  // for
    }
    return NS_OK;
}

NS_IMETHODIMP
nsUrlbarHistory::VerifyAndCreateEntry(const PRUnichar * aSearchItem, const PRUnichar * aMatchStr, nsIAutoCompleteResults * aResultArray)
{
    if (!aSearchItem || !aMatchStr || !aResultArray)
        return NS_ERROR_FAILURE;

    PRInt32 searchStrLen = 0;

    if (aSearchItem)
        searchStrLen = nsCRT::strlen(aSearchItem);
    nsresult rv;

    nsXPIDLCString filePath;
    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (!ioService) return NS_ERROR_FAILURE;
    ioService->ExtractUrlPart(NS_ConvertUCS2toUTF8(aSearchItem).get(), nsIIOService::url_Path, 0, 0, getter_Copies(filePath));
        
        // Don't bother checking for hostname if the search string
        // already has a filepath;
        if (filePath && (nsCRT::strlen(filePath) > 1)) {
            return NS_OK;
        }
          
   ioService->ExtractUrlPart(NS_ConvertUCS2toUTF8(aMatchStr).get(), nsIIOService::url_Path, 0, 0, getter_Copies(filePath));

        // If the match string doesn't have a filepath
        // we need to do nothing here,  return.
        if (!filePath || (filePath && (nsCRT::strlen(filePath) <=1)))
            return NS_OK;
        
        // Find the position of the filepath in the result string
        nsAutoString matchAutoStr(aMatchStr);
        PRInt32 slashIndex = matchAutoStr.Find("/", PR_FALSE, searchStrLen);
        // Extract the host name
        nsAutoString hostName;
        matchAutoStr.Left(hostName, slashIndex);
        //printf("#### Host Name is %s\n", NS_LossyConvertUCS2toASCII(hostName).get());
        // Check if this host is already present in the result array
        // If not add it to the result array
        PRBool itemAvailable = PR_TRUE;
        CheckItemAvailability(hostName.get(), aResultArray, &itemAvailable);
        if (!itemAvailable) {
            // Insert the host name to the result array at the top
            //Create an AutoComplete Item 
		    nsCOMPtr<nsIAutoCompleteItem> newItem(do_CreateInstance(NS_AUTOCOMPLETEITEM_CONTRACTID));
            NS_ENSURE_TRUE(newItem, NS_ERROR_FAILURE);            
            newItem->SetValue(hostName);
            nsCOMPtr<nsISupportsArray> array;
            rv = aResultArray->GetItems(getter_AddRefs(array));
            // Always insert the host entry at the top of the array
            if (NS_SUCCEEDED(rv)) {
                array->InsertElementAt(newItem, 0);
            }
        }       
    return NS_OK;
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
