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

// Local Includes 
#include "nsSHistory.h"

// Helper Classes
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsIGenericFactory.h"
#include "nsILayoutHistoryState.h"
#include "nsIDocShellLoadInfo.h"
#include "nsXPIDLString.h"
#include "nsISHContainer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellLoadInfo.h"

//*****************************************************************************
//***    nsSHistory: Object Management
//*****************************************************************************

nsSHistory::nsSHistory() : mListRoot(nsnull), mIndex(-1), mLength(0)
{
   NS_INIT_REFCNT();
}


nsSHistory::~nsSHistory()
{
}

//*****************************************************************************
//    nsSHistory: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsSHistory)
NS_IMPL_RELEASE(nsSHistory)

NS_INTERFACE_MAP_BEGIN(nsSHistory)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHistory)
   NS_INTERFACE_MAP_ENTRY(nsISHistory)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsSHistory: nsISHistory
//*****************************************************************************

/* Add an entry to the History list at mIndex and 
 * increment the index to point to the new entry
 */
NS_IMETHODIMP
nsSHistory::AddEntry(nsISHEntry * aSHEntry, PRBool aPersist)
{
   NS_ENSURE_ARG(aSHEntry);

   nsCOMPtr<nsISHTransaction> currentTxn;

   if(mListRoot)
      GetTransactionAtIndex(mIndex, getter_AddRefs(currentTxn));

   PRBool currentPersist = PR_TRUE;
   if(currentTxn)
      currentTxn->GetPersist(&currentPersist);

   if(!currentPersist)
      {
      NS_ENSURE_SUCCESS(currentTxn->SetSHEntry(aSHEntry),
         NS_ERROR_FAILURE);
      return NS_OK;
      }

	nsCOMPtr<nsISHTransaction> txn(do_CreateInstance(NS_SHTRANSACTION_PROGID));
   NS_ENSURE_TRUE(txn, NS_ERROR_FAILURE);

   // Set the ShEntry and parent for the transaction. setting the 
	// parent will properly set the parent child relationship
   txn->SetPersist(aPersist);
	NS_ENSURE_SUCCESS(txn->Create(aSHEntry, currentTxn), NS_ERROR_FAILURE);
   
   // A little tricky math here...  Basically when adding an object regardless of
   // what the length was before, it should always be set back to the current and
   // lop off the forward.
   mLength = (++mIndex + 1);

   // If this is the very first transaction, initialize the list
   if(!mListRoot)
      mListRoot = txn;
   PrintHistory();

   return NS_OK;
}

/* Get size of the history list */
NS_IMETHODIMP
nsSHistory::GetCount(PRInt32 * aResult)
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
	   rv = tempPtr->GetNext(getter_AddRefs(ptr));
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
		      if (!txn)
			     break;
              nsCOMPtr<nsISHEntry>  entry;
              rv = txn->GetSHEntry(getter_AddRefs(entry));
              if (!NS_SUCCEEDED(rv) && !entry)
                      return NS_ERROR_FAILURE;

              nsCOMPtr<nsILayoutHistoryState> layoutHistoryState;
              nsCOMPtr<nsIURI>  uri;
              PRUnichar *  title;
              char * titleCStr=nsnull;
              nsXPIDLCString  url;

              entry->GetLayoutHistoryState(getter_AddRefs(layoutHistoryState));
              entry->GetURI(getter_AddRefs(uri));
              entry->GetTitle(&title);

              nsString titlestr(title);
              titleCStr = titlestr.ToNewCString();
              
			  if (uri)
                 uri->GetSpec(getter_Copies(url));

              #if 0
              printf("**** SH Transaction #%d, Entry = %x\n", index, entry.get());
              printf("\t\t URL = %s\n", url);
              printf("\t\t Title = %s\n", titleCStr);
              printf("\t\t layout History Data = %x\n", layoutHistoryState);
              #endif
      
              Recycle(title);
              Recycle(titleCStr);

              nsCOMPtr<nsISHTransaction> next;
              rv = txn->GetNext(getter_AddRefs(next));
              if (NS_SUCCEEDED(rv) && next) {
                      txn = next;
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

//*****************************************************************************
//    nsSHistory: nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP
nsSHistory::GetCanGoBack(PRBool * aCanGoBack)
{
   NS_ENSURE_ARG_POINTER(aCanGoBack);
   *aCanGoBack = PR_FALSE;

   PRInt32 index = -1;
   NS_ENSURE_SUCCESS(GetIndex(&index), NS_ERROR_FAILURE);
   if(index > 0)
      *aCanGoBack = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetCanGoForward(PRBool * aCanGoForward)
{
    NS_ENSURE_ARG_POINTER(aCanGoForward);
   *aCanGoForward = PR_FALSE;

   PRInt32 index = -1;
   PRInt32 count = -1;

   NS_ENSURE_SUCCESS(GetIndex(&index), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(GetCount(&count), NS_ERROR_FAILURE);

   if((index >= 0) && (index < (count - 1)))
      *aCanGoForward = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GoBack()
{
	PRBool canGoBack = PR_FALSE;

	GetCanGoBack(&canGoBack);
	if (!canGoBack)  // Can't go back
		return NS_ERROR_UNEXPECTED;
    return GotoIndex(mIndex-1);
}


NS_IMETHODIMP
nsSHistory::GoForward()
{
	PRBool canGoForward = PR_FALSE;

	GetCanGoForward(&canGoForward);
	if (!canGoForward)  // Can't go forward
		return NS_ERROR_UNEXPECTED;
    return GotoIndex(mIndex+1);

}

NS_IMETHODIMP
nsSHistory::Reload(PRInt32 aReloadType)
{

	return LoadEntry(mIndex, PR_TRUE, aReloadType);

}

NS_IMETHODIMP
nsSHistory::Stop()
{
	//Not implemented
   return NS_OK;
}


NS_IMETHODIMP
nsSHistory::SetDocument(nsIDOMDocument* aDocument,
   const PRUnichar* aContentType)
{
	// Not implemented
  return NS_OK;

}

NS_IMETHODIMP
nsSHistory::GetDocument(nsIDOMDocument** aDocument)
{

	// Not implemented
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::GetCurrentURI(PRUnichar** aCurrentURI)
{
  // Not implemented
  return NS_OK;
}



NS_IMETHODIMP
nsSHistory::SetSessionHistory(nsISHistory* aSessionHistory)
{
   // Not implemented
   return NS_OK;
}

	
NS_IMETHODIMP
nsSHistory::GetSessionHistory(nsISHistory** aSessionHistory)
{
  // Not implemented
   return NS_OK;
}


NS_IMETHODIMP
nsSHistory::LoadURI(const PRUnichar* aURI)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::AddChildSHEntry(nsISHEntry * aCloneRef, nsISHEntry * aNewEntry)
{

  //XXX Not yet implemented
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetSHEForChild(PRInt32 aChildOffset, nsISHEntry ** aResult)
{

    // XXX Not yet implemented
	return NS_OK;

}

NS_IMETHODIMP
nsSHistory::GotoIndex(PRInt32 aIndex)
{

	return LoadEntry(aIndex, PR_FALSE, nsIDocShellLoadInfo::loadHistory);
}

NS_IMETHODIMP
nsSHistory::LoadEntry(PRInt32 aIndex, PRBool aReloadFlag, long aLoadType)
{
   nsCOMPtr<nsIDocShell> docShell;
   nsCOMPtr<nsISHEntry> shEntry;
   PRInt32 oldIndex = mIndex;

   nsCOMPtr<nsISHEntry> prevEntry;
   GetEntryAtIndex(mIndex, PR_FALSE, getter_AddRefs(prevEntry));
   mIndex = aIndex;
   
   nsCOMPtr<nsISHEntry> nextEntry;
   GetEntryAtIndex(mIndex, PR_FALSE, getter_AddRefs(nextEntry));

   nsCOMPtr<nsIURI> nexturi;
   nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
   if (oldIndex != aIndex) {
      PRBool result = CompareSHEntry(prevEntry, nextEntry, mRootDocShell, getter_AddRefs(docShell), getter_AddRefs(shEntry));
      if (!result)  
	   mIndex = oldIndex;

      if (!docShell || !shEntry || !mRootDocShell)
	    return NS_ERROR_FAILURE;
      

      shEntry->GetURI(getter_AddRefs(nexturi));       
   }	
   else
      nextEntry->GetURI(getter_AddRefs(nexturi));

   mRootDocShell->CreateLoadInfo (getter_AddRefs(loadInfo));
   // This is not available yet
   loadInfo->SetLoadType(aLoadType);
   loadInfo->SetSHEntry(nextEntry);
   // Time to initiate a document load
   return docShell->LoadURI(nexturi, loadInfo);
    
}


PRBool
nsSHistory::CompareSHEntry(nsISHEntry * aPrevEntry, nsISHEntry * aNextEntry, nsIDocShell * aParent,
					nsIDocShell ** aDSResult, nsISHEntry ** aSHEResult)
{
	NS_ENSURE_ARG_POINTER(aDSResult);
	NS_ENSURE_ARG_POINTER(aSHEResult);

	if (!aPrevEntry || !aNextEntry || !aParent)
	   return PR_FALSE;

	nsresult rv;
	PRBool result = PR_FALSE;
   	nsCOMPtr<nsIURI>   prevURI, nextURI;
	nsXPIDLCString     prevUriSpec, nextUriSpec;
	// Not reference counted on purpose
	nsIDocShell * docshell = aParent;
	nsCOMPtr<nsISHEntry> prevEntry = aPrevEntry;
	nsCOMPtr<nsISHEntry> nextEntry = aNextEntry;

    prevEntry->GetURI(getter_AddRefs(prevURI));
	nextEntry->GetURI(getter_AddRefs(nextURI));
    if (!prevURI || !nextURI)
	  return PR_FALSE;

	prevURI->GetSpec(getter_Copies(prevUriSpec));
	nextURI->GetSpec(getter_Copies(nextUriSpec));
	   
	if (!prevUriSpec || !nextUriSpec)
		   return PR_FALSE;

    if (prevUriSpec != nextUriSpec) {
       *aDSResult = docshell;
	   *aSHEResult = nextEntry;
	   return PR_TRUE;
    }

    /* compare the child frames */
    PRInt32  cnt=0, pcnt=0, ncnt=0, dsCount=0;
	nsCOMPtr<nsISHContainer>  prevContainer(do_QueryInterface(prevEntry));
	nsCOMPtr<nsISHContainer>  nextContainer(do_QueryInterface(nextEntry));
	//XXX Not ref counted on purpose. Is this correct.?
	// How about AddRef in the QueryInterface?
	nsIDocShellTreeNode *  dsTreeNode = nsnull;
	
	rv = docshell->QueryInterface(NS_GET_IID(nsIDocShellTreeNode), (void  **) &dsTreeNode);

	if (!NS_SUCCEEDED(rv) || !dsTreeNode)
		return PR_FALSE;

    prevContainer->GetChildCount(&pcnt);
    nextContainer->GetChildCount(&ncnt);
    dsTreeNode->GetChildCount(&dsCount);

    //XXX What to do if the children count don't match
    
    for (PRInt32 i=0; i<ncnt; i++){
	  nsCOMPtr<nsISHEntry> pChild, nChild;
      nsIDocShellTreeItem * dsTreeItemChild=nsnull;
	  

      prevContainer->GetChildAt(i, getter_AddRefs(pChild));
	  nextContainer->GetChildAt(i, getter_AddRefs(nChild));
	  dsTreeNode->GetChildAt(i, &dsTreeItemChild);

	  // XXX How about AddRef in QueryInterface? Is this OK?
	  nsIDocShell *  dsChild = nsnull;
	  
	  rv = dsTreeItemChild->QueryInterface(NS_GET_IID(nsIDocShell), (void **) dsChild);

	  result = CompareSHEntry(pChild, nChild, dsChild, aDSResult, aSHEResult);
	  if (result)  // We have found the docshell in which loadUri is to be called.
		  break;
	}     
    return result;
}




NS_IMETHODIMP
nsSHistory::SetRootDocShell(nsIDocShell * aDocShell)
{
  mRootDocShell = aDocShell;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetRootDocShell(nsIDocShell ** aDocShell)
{
   NS_ENSURE_ARG_POINTER(aDocShell);

   *aDocShell = mRootDocShell;
   //Not refcounted. May this method should not be available for public
  // NS_IF_ADDREF(*aDocShell);
   return NS_OK;
}

