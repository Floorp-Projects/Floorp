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
nsSHistory::Add(nsISHEntry * aSHEntry)
{
	nsresult rv;

	NS_PRECONDITION(aSHEntry != nsnull, "null ptr");
    if (! aSHEntry)
      return NS_ERROR_NULL_POINTER;

    //nsISHTransaction * txn=nsnull;
	nsCOMPtr<nsISHTransaction>  txn;
    rv = nsComponentManager::CreateInstance(NS_SHTRANSACTION_PROGID,
	                                      nsnull,
										  nsISHTransaction::GetIID(),
										  getter_AddRefs(txn));
	nsCOMPtr<nsISHTransaction> parent;

    if (NS_SUCCEEDED(rv) && txn) {		
		if (mListRoot) {
           GetTransactionForIndex(mIndex, getter_AddRefs(parent));
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
			   //NS_ADDREF(txn);

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

NS_IMETHODIMP
nsSHistory::GetRootEntry(nsISHTransaction ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

	*aResult = mListRoot;
	NS_IF_ADDREF(*aResult);
	return NS_OK;   
}

NS_IMETHODIMP
nsSHistory::GetTransactionForIndex(PRInt32 aIndex, nsISHTransaction ** aResult)
{
   nsresult rv;
   NS_ENSURE_ARG_POINTER(aResult);

   if (mLength <= 0)
	   return NS_ERROR_FAILURE;

   if (mListRoot) {
     if (aIndex == 0)
	 {
	    *aResult = mListRoot;
	    NS_ADDREF(*aResult);
	    return NS_OK;
	 } 
	 PRInt32   cnt=0;
	 nsCOMPtr<nsISHTransaction>  tempPtr;
	 GetRootEntry(getter_AddRefs(tempPtr));

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
			  // XXX Not sure if this will maintain reference
            tempPtr = ptr;
            continue;
		  }
	   }  //NS_SUCCEEDED
	   else 
		   return NS_ERROR_FAILURE;
	 }  // while
   } // mListRoot
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
