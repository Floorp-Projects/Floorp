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
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsIAutoCompleteResults.h"

static char * ignoreArray[] = {
		"http://",
	    "ftp://",
		"www.",
		"http://www.",
        "keyword:"
	};
//*****************************************************************************
//***    nsUrlbarHistory: Object Management
//*****************************************************************************

nsUrlbarHistory::nsUrlbarHistory():mLength(0)
{
   NS_INIT_REFCNT();
   PRInt32 cnt = sizeof(ignoreArray)/sizeof(char *);
   for(PRInt32 i=0; i< cnt; i++) 
     mIgnoreArray.AppendElement((void *) new nsString(NS_ConvertASCIItoUCS2(ignoreArray[i])));
   
}


nsUrlbarHistory::~nsUrlbarHistory()
{
    ClearHistory();
	PRInt32 cnt = sizeof(ignoreArray)/sizeof(char *);
    for(PRInt32 j=0; j< cnt; j++)  {
		nsString * ignoreEntry = (nsString *) mIgnoreArray.ElementAt(j);
	    delete ignoreEntry;
	}
	mIgnoreArray.Clear();
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
  // This procedure should clear all of the entries out of the structure, but
  // not delete the structure itself.
  // 
  // This loop takes each existing entry in the list and replaces it with
  // a blank string, decrementing the size of the list by one each time.
  // Eventually, we should end up with a zero sized list.
 

  for (PRInt32 i=0; i<=mLength; i++) 
  {
     nsString * entry = nsnull;
     entry = (nsString *)mArray.ElementAt(i);
     delete entry;
     mLength--;
  }
  mLength = 0;   // just to make sure
  mArray.Clear();
  return NS_OK;
}


/* Add an entry to the History list at mIndex and 
 * increment the index to point to the new entry
 */
NS_IMETHODIMP
nsUrlbarHistory::AddEntry(const char * aURL)
{
   NS_ENSURE_ARG(aURL);   

   PRInt32 i = mArray.Count();
   nsCString newEntry(aURL);
   for (i=0; i<mArray.Count(); i++)
   {
       nsString * entry = (nsString *)mArray.ElementAt(i);
	   if ( (*entry).EqualsWithConversion(newEntry)) //Entry already in the list
		   return NS_OK;

   }
   nsString * entry = nsnull;
   entry = new nsString(NS_ConvertASCIItoUCS2(aURL));
   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
   if (entry->Length() == 0) {
	   //Don't add null strings in to the list
       delete entry;
	   return NS_OK;
   }
   nsresult rv = mArray.AppendElement((void *) entry);
   if (NS_SUCCEEDED(rv))
	   mLength++;             //Increase the count 

   return rv;
}

/* Get size of the history list */
NS_IMETHODIMP
nsUrlbarHistory::GetCount(PRInt32 * aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	*aResult = mLength;
	return NS_OK;
}


/* Get the entry at a given index */
NS_IMETHODIMP
nsUrlbarHistory::GetEntryAtIndex(PRInt32 aIndex, char ** aResult)
{
   
	NS_ENSURE_ARG_POINTER(aResult);
	NS_ENSURE_TRUE((aIndex>=0 && aIndex<mLength), NS_ERROR_FAILURE);
   
	nsString * entry = nsnull;
	entry = (nsString *) mArray.ElementAt(aIndex);
    NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
	*aResult = entry->ToNewCString();

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
	/*
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

    nsAutoString searchAutoStr(searchStr);

	PRInt32 cnt = mArray.Count();
	for(PRInt32 i=cnt-1; i>=0; i--)
	{
       nsString * item = nsnull;
	   PRUnichar * match = nsnull;
	   PRInt32 index = -1;
	   item = (nsString*) mArray.ElementAt(i);
	   

	   if (item) {
		// printf("SearchCache::Comparing %s with %s\n", searchAutoStr.ToNewCString(),item->ToNewCString());
	     index = item->Find(searchStr, PR_TRUE);
	     match = item->ToNewUnicode();
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
		       index = item->Find(searchSubStr, PR_TRUE);
		       if (match)
			     Recycle(match);
		       match = item->ToNewUnicode();
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
	   Recycle(match);
	}  //for    
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
