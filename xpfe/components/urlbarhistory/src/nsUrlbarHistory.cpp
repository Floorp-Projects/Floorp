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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// Local Includes 
#include "nsUrlbarHistory.h"

// Helper Classes
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsIGenericFactory.h"
#include "nsString.h"


//*****************************************************************************
//***    nsUrlbarHistory: Object Management
//*****************************************************************************

nsUrlbarHistory::nsUrlbarHistory():mLength(0)
{
   NS_INIT_REFCNT();
}


nsUrlbarHistory::~nsUrlbarHistory()
{
	PRInt32 i = mLength;
	for (i=0; i<mLength; i++) {
      nsString * entry = nsnull;
	  entry = (nsString *)mArray.ElementAt(i);
	  delete entry;
	  mLength--;
	}
	mArray.Clear();
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
