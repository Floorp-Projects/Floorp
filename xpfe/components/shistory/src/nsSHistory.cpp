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
 */

#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsSHistory.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"



#ifdef XXX_NS_DEBUG       // XXX: we'll need a logging facility for debugging
#define WEB_TRACE(_bit,_args)            \
   PR_BEGIN_MACRO                         \
     if (WEB_LOG_TEST(gLogModule,_bit)) { \
       PR_LogPrint _args;                 \
     }                                    \
   PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif

NS_IMPL_ISUPPORTS1(nsSHistory, nsISHistory)

nsSHistory::nsSHistory() : mListRoot(nsnull), mIndex(-1), mLength(0)
{
NS_INIT_REFCNT();
}


nsSHistory::~nsSHistory()
{
  //NS_IF_RELEASE(mListRoot);  
}

/* Add an entry to the History list at mIndex and 
 * increment the index to point to the new entry
 */
NS_IMETHODIMP
nsSHistory::AddEntry(nsISHEntry * aSHEntry)
{
	nsresult rv;

	NS_PRECONDITION(aSHEntry != nsnull, "null ptr");
    if (! aSHEntry)
      return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISHTransaction>  txn;
    rv = nsComponentManager::CreateInstance(NS_SHTRANSACTION_PROGID,
	                                      nsnull,
										  nsISHTransaction::GetIID(),
										  getter_AddRefs(txn));
	nsCOMPtr<nsISHTransaction> parent;

    if (NS_SUCCEEDED(rv) && txn) {		
		if (mListRoot) {
           GetTransactionAtIndex(mIndex, getter_AddRefs(parent));
		}
		// Set the ShEntry and parent for the transaction. setting the 
		// parent will properly set the parent child relationship
		rv = txn->Create(aSHEntry, parent);
		if (NS_SUCCEEDED(rv)) {
            mLength++;
			mIndex++;
		    // If this is the very first transaction, initialize the list
		    if (!mListRoot)
			   mListRoot = txn;
                      PrintHistory();
		}
	}
   return NS_OK;
}

/* Get length of the history list */
NS_IMETHODIMP
nsSHistory::GetLength(PRInt32 * aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	*aResult = mLength;
	return NS_OK;
}

/* Get index of the history list */
NS_IMETHODIMP
nsSHistory::GetIndex(PRInt32 * aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	*aResult = mIndex;
	return NS_OK;
}

/* Get the root (the very first) entry in the history list */
NS_IMETHODIMP
nsSHistory::GetRootEntry(nsISHEntry ** aResult)
{
    nsresult rv;

      /* GetSHEntry ensures aResult is valid */
      if (mListRoot) {
        rv = mListRoot->GetSHEntry(aResult);
        return rv;
      }
      return NS_ERROR_FAILURE;
}

/* Get the entry prior to the current index */
NS_IMETHODIMP
nsSHistory::GetPreviousEntry(PRBool aModifyIndex, nsISHEntry ** aResult)
{
    nsresult rv;

    /* GetEntryAtIndex ensures aResult is valid */
      rv = GetEntryAtIndex((mIndex-1), aModifyIndex, aResult);
      return rv;
}

/* Get the entry next to the current index */
NS_IMETHODIMP
nsSHistory::GetNextEntry(PRBool aModifyIndex, nsISHEntry ** aResult)
{
    nsresult rv;

      /* GetEntryAtIndex ensures aResult is valid */
      rv = GetEntryAtIndex((mIndex+1), aModifyIndex, aResult);
    return rv;
}


/* Get the entry at a given index */
NS_IMETHODIMP
nsSHistory::GetEntryAtIndex(PRInt32 aIndex, PRBool aModifyIndex, nsISHEntry** aResult)
{
    nsresult rv;
      nsCOMPtr<nsISHTransaction> txn;

      /* GetTransactionAtIndex ensures aResult is valid and validates aIndex */
      rv = GetTransactionAtIndex(aIndex, getter_AddRefs(txn));
      if (NS_SUCCEEDED(rv) && txn) {
         //Get the Entry from the transaction
         rv = txn->GetSHEntry(aResult);
         if (NS_SUCCEEDED(rv) && (*aResult)) {
                 // Set mIndex to the requested index, if asked to do so..
                 if (aModifyIndex) {
                         mIndex = aIndex;
                 }
         } //entry
      }  //Transaction
      return rv;
}

/* Get the transaction at a given index */
NS_IMETHODIMP
nsSHistory::GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult)
{
     nsresult rv;
     NS_ENSURE_ARG_POINTER(aResult);

     if ((mLength <= 0) || (aIndex < 0) || (aIndex >= mLength))
	   return NS_ERROR_FAILURE;

     if (!mListRoot) 
         return NS_ERROR_FAILURE;

     if (aIndex == 0)
	 {
	    *aResult = mListRoot;
	    NS_ADDREF(*aResult);
	    return NS_OK;
	 } 
	 PRInt32   cnt=0;
	 nsCOMPtr<nsISHTransaction>  tempPtr;
       
       rv = GetRootTransaction(getter_AddRefs(tempPtr));
       if (!NS_SUCCEEDED(rv) || !tempPtr)
               return NS_ERROR_FAILURE;

     while(1) {
       nsCOMPtr<nsISHTransaction> ptr;
	   rv = tempPtr->GetChild(getter_AddRefs(ptr));
	   if (NS_SUCCEEDED(rv) && ptr) {
          cnt++;
		  if (cnt == aIndex) {
			  *aResult = ptr;
			  NS_ADDREF(*aResult);
			  break;
		  }
		  else {
            tempPtr = ptr;
            continue;
		  }
	   }  //NS_SUCCEEDED
	   else 
		   return NS_ERROR_FAILURE;
       }  // while 
  
   return NS_OK;
}

NS_IMETHODIMP
nsSHistory::PrintHistory()
{

      nsCOMPtr<nsISHTransaction>   txn;
      PRInt32 index = 0;
      nsresult rv;

      if (!mListRoot) 
              return NS_ERROR_FAILURE;

      txn = mListRoot;
    
      while (1) {
        nsCOMPtr<nsISHEntry>  entry;
              rv = txn->GetSHEntry(getter_AddRefs(entry));
              if (!NS_SUCCEEDED(rv) && !entry)
                      return NS_ERROR_FAILURE;

              nsCOMPtr<nsISupports> layoutHistoryState;
              nsCOMPtr<nsIURI>  uri;
              PRUnichar *  title;
              char * titleCStr=nsnull;
              nsXPIDLCString  url;

              entry->GetLayoutHistoryState(getter_AddRefs(layoutHistoryState));
              entry->GetUri(getter_AddRefs(uri));
              entry->GetTitle(&title);

              nsString titlestr(title);
              titleCStr = titlestr.ToNewCString();
              
              uri->GetSpec(getter_Copies(url));

              printf("**** SH Transaction #%d, Entry = %x\n", index, entry.get());
              printf("\t\t URL = %s\n", url);
              printf("\t\t Title = %s\n", titleCStr);
              printf("\t\t layout History Data = %x\n", layoutHistoryState);
      
              Recycle(title);
              Recycle(titleCStr);

              nsCOMPtr<nsISHTransaction> child;
              rv = txn->GetChild(getter_AddRefs(child));
              if (NS_SUCCEEDED(rv) && child) {
                      txn = child;
                      index++;
                      continue;
              }
              else
                      break;
      }
      
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::GetRootTransaction(nsISHTransaction ** aResult)
{
    nsCOMPtr<nsISHEntry>   entry;

    NS_ENSURE_ARG_POINTER(aResult);
    *aResult=mListRoot;
      NS_IF_ADDREF(*aResult);
      return NS_OK;
}


NS_IMETHODIMP
NS_NewSHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsresult rv = NS_OK;

  nsSHistory* result = new nsSHistory();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;


  rv = result->QueryInterface(aIID, aResult);

  if (NS_FAILED(rv)) {
    delete result;
    *aResult = nsnull;
    return rv;
  }

  return rv;
}
