/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsSessionHistory.h"
#include "nsAppShellCIDs.h"
#include "nsVoidArray.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "prmem.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"

// Interface ID for nsIHistoryEntry
#define NS_IHISTORY_ENTRY_IID \
{ 0x68e73d53, 0x12eb, 0x11d3, { 0xbd, 0xc0, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44 } \
}

/* Define Class IDs */
static NS_DEFINE_CID(kWebShellCID,         NS_WEB_SHELL_CID);



static nsHistoryEntry *  GenerateTree(const char * aStickyURL, nsIWebShell * aStickyContainer, nsIWebShell * aContainer,
                                      nsHistoryEntry *aParent, nsISessionHistory * aSHist, const char * aReferrer);

#define APP_DEBUG 0

class nsHistoryEntry
{
public:
//  static const nsIID& GetIID() { static nsIID iid = NS_IHISTORY_ENTRY_IID; return iid; }

  //ctor
  nsHistoryEntry();
  ~nsHistoryEntry();

   //nsISupports
//   NS_DECL_ISUPPORTS

  /**
   * Create the History data structures for the current URL. This method
   * will also generate the history tree for the URL if it contains frames
   */
  nsresult Create(const char * aURL, nsIWebShell * aWebShell, const char * referrer, 
                  nsHistoryEntry * aParent, nsISessionHistory * aSHist);

  /**
   * Load the entry in the content Area
   */
  PRBool  Load(nsIWebShell * aPrevEntry, PRBool aIsReload);

  /**
   * Compare the history object with the content Area
   */
  PRBool  Compare(nsIWebShell * aPrevEntry, PRBool aIsReload);

  /**
   * Destroy the  historyentry
   */
  nsresult DestroyChildren();

  /**
   * Get the Title of the page 
   */
  nsresult GetTitle(PRUnichar ** aName);

  /**
   * Set the title of the page 
   */
  nsresult SetTitle(const PRUnichar * aName);

  /**
   * Get the URL  of the page 
   */
  nsresult GetURL(char ** aURL);

  /**
   * Set the URL  of the page 
   */
  nsresult SetURL(const char * aURL);

  /**
   * Get the referrer  of the page 
   */
  nsresult GetReferrer(char ** aReferrer);

  /**
   * Set the referrer  of the page 
   */
  nsresult SetReferrer(const char * aReferrer);

  /**
   * Get the webshell  of the page 
   */
  nsresult GetWebShell(nsIWebShell *& aName);

  /**
   * Set the webshell  of the page 
   */
  nsresult SetWebShell(nsIWebShell * aName);

  /**
   * Get the History State of the page 
   */
  nsresult GetHistoryState(nsISupports ** aResult);

  /**
   * Set the History State of the page 
   */
  nsresult SetHistoryState(nsISupports * aState);

  /**
   *  Add a child
   */
  nsresult AddChild(nsHistoryEntry * aChild);

  /**
   *  Get the child count
   */
  PRInt32  GetChildCnt();

  /**
   *  Get the child at a given index
   */
  nsresult GetChildAt(PRInt32 aIndex, nsHistoryEntry *& aResult);

  /**
   *  Set parent
   */
  nsresult SetParent(nsHistoryEntry * aParent);

  /**
   *  Get parent
   */
  nsresult GetParent(nsHistoryEntry *& aResult);

  /**
   *  Set the SessionHistory ID 
   */
  nsresult SetSessionHistoryID(nsISessionHistory * aSessionHistory);

  /**
   * Get a handle to the history entry for a Webshell 
   */
  
  nsHistoryEntry * GetHistoryForWS(nsIWebShell * aWS);
  
  /**
   *  Get the handle to the root of the  document structure. If the
   * page contains a frame set, this will return handle to the
   * topmost url that contains the outermost frameset.
   */
  nsHistoryEntry *  GetRootDoc();

  
  nsIWebShell *       mWS;    //Webshell corresponding to this history entry
  nsString *          mURL;   // URL for this history entry
  char *              mReferrer;
  nsString *          mTitle;  // Name of the document
  nsVoidArray         mChildren;  //  children list
  PRInt32             mChildCount; // # of children
  nsHistoryEntry *    mParent;
  nsISupports *       mHistoryState;  // History state as saved by the layout

  // WARNING: Weak reference
  nsISessionHistory * mHistoryList;  // The handle to the session History to 
                                     // which this entry belongs
};

MOZ_DECL_CTOR_COUNTER(nsHistoryEntry);

nsHistoryEntry::nsHistoryEntry()
{
  MOZ_COUNT_CTOR(nsHistoryEntry);
   mChildCount = 0;
   mWS  = nsnull;
   mHistoryList = nsnull;
   mParent = nsnull;
   mURL = nsnull;
   mReferrer = nsnull;
   mTitle = nsnull;
   mHistoryState = nsnull; 
//  NS_INIT_REFCNT();
}

nsHistoryEntry::~nsHistoryEntry()
{
  MOZ_COUNT_DTOR(nsHistoryEntry);
   delete mTitle;
   delete mURL;
   if (mReferrer)
     nsCRT::free(mReferrer);
   NS_IF_RELEASE(mWS);
   // mHistoryList is a weak reference. Hence no release.
   mHistoryList = nsnull;
   NS_IF_RELEASE(mHistoryState);

  DestroyChildren();
}

//NS_IMPL_ADDREF(nsHistoryEntry)
//NS_IMPL_RELEASE(nsHistoryEntry)

nsresult
nsHistoryEntry::DestroyChildren() {

  PRInt32 i, n;

  n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsHistoryEntry* child = (nsHistoryEntry *) mChildren.ElementAt(i);
    child->SetParent(nsnull);
    delete child;
    mChildCount--;
  }
  mChildren.Clear();
  return NS_OK;

}


nsresult
nsHistoryEntry::GetURL(char** aURL)
{
  //GetURlForIndex error checks aURL
  if (mURL)
    *aURL= mURL->ToNewCString();
  return NS_OK;
}

nsresult
nsHistoryEntry::SetURL(const char* aURL)
{

  if (mURL)
    delete mURL;
  mURL = new nsString(aURL);
  return NS_OK;
}

nsresult
nsHistoryEntry::GetReferrer(char** aReferrer)
{
  *aReferrer = mReferrer ? nsCRT::strdup(mReferrer) : nsnull;
  return NS_OK;
}

nsresult
nsHistoryEntry::SetReferrer(const char* aReferrer)
{

  delete mReferrer;
  mReferrer = nsCRT::strdup(aReferrer);
  return aReferrer ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsHistoryEntry::GetWebShell(nsIWebShell *& aResult)
{
  aResult = mWS;
  NS_IF_ADDREF(mWS);
  return NS_OK;
}

nsresult
nsHistoryEntry::SetWebShell(nsIWebShell * aWebShell)
{
  NS_IF_RELEASE(mWS);
  mWS = aWebShell;
  NS_IF_ADDREF(aWebShell);
  return NS_OK;
}

nsresult
nsHistoryEntry::GetHistoryState(nsISupports ** aResult)
{
  *aResult = mHistoryState;
    NS_IF_ADDREF(mHistoryState);
  return NS_OK;
}

nsresult
nsHistoryEntry::SetHistoryState(nsISupports * aState)
{
  NS_IF_RELEASE(mHistoryState);
  mHistoryState = aState;
  NS_IF_ADDREF(aState);
  return NS_OK;
}

nsresult
nsHistoryEntry::GetTitle(PRUnichar** aTitle)
{
	//GetTitleForindex error checks aTitle
  if (mTitle)
    *aTitle = mTitle->ToNewUnicode();
  return NS_OK;
}

nsresult
nsHistoryEntry::SetTitle(const PRUnichar* aTitle)
{

  if (mTitle)
    delete mTitle;
  if (aTitle)
    mTitle =  new nsString(aTitle);
  return NS_OK;
}

PRInt32 
nsHistoryEntry::GetChildCnt()
{
  return mChildCount;
}

nsresult
nsHistoryEntry::GetChildAt(PRInt32 aIndex, nsHistoryEntry *& aResult)
{
  if (PRUint32(aIndex) >= PRUint32(mChildren.Count())) {
    aResult = nsnull;
  }
  else {
    aResult = (nsHistoryEntry*) mChildren.ElementAt(aIndex);
//    NS_IF_ADDREF(aResult);
  }
  return NS_OK;
}


nsresult
nsHistoryEntry::SetSessionHistoryID(nsISessionHistory * aHistoryListID)
{
  // WARNING: Weak Reference
   mHistoryList = aHistoryListID;
   return NS_OK;
}


nsresult
nsHistoryEntry::SetParent(nsHistoryEntry* aParent)
{
//  NS_IF_RELEASE(mParent);
  mParent = aParent;
//  NS_IF_ADDREF(aParent);
  return NS_OK;
}



nsresult
nsHistoryEntry::GetParent(nsHistoryEntry *& aParent)
{

  aParent = mParent;
//  NS_IF_ADDREF(mParent);
  return NS_OK;
}

nsresult
nsHistoryEntry::AddChild(nsHistoryEntry* aChild)
{
  NS_PRECONDITION(nsnull != aChild, "null ptr");
  if (nsnull == aChild) {
    return NS_ERROR_NULL_POINTER;
  }
  mChildren.AppendElement(aChild);
  mChildCount++;
  aChild->SetParent(this);
//  NS_ADDREF(aChild);

  return NS_OK;
}

  


/* Create a historyentry for the given webshell. If the
 * webshell has children, this method recursively goes down
 * the tree and creates entries for the children too.
 */

nsresult 
nsHistoryEntry::Create(const char * aURL, nsIWebShell * aWebShell, const char * aReferrer, 
                       nsHistoryEntry * aParent, nsISessionHistory * aSHist) 
{

   // save the webshell's URL and referrer in the history entry
   SetURL(aURL);
   if (aReferrer)
     SetReferrer(aReferrer);

   //Save the webshell id
   SetWebShell(aWebShell);
   
   if (APP_DEBUG) printf("SessionHistory::Create Creating Historyentry %x  for webshell %x, url = %s parent entry = %x\n", (unsigned int)this, (unsigned int)aWebShell, aURL, (unsigned int) aParent);

   if (aParent)
     aParent->AddChild(this);

   // Save the handle to the window's history list   
   SetSessionHistoryID(aSHist);

   return NS_OK;
}

static nsHistoryEntry *  
GenerateTree(const char * aStickyUrl, nsIWebShell * aStickyContainer, nsIWebShell * aContainer, 
             nsHistoryEntry * aParent, nsISessionHistory * aSHist, const char * aReferrer) 
{

   nsHistoryEntry * hEntry = nsnull;
   nsAutoString urlAStr;

   hEntry = new nsHistoryEntry;

   if (!hEntry) {
      NS_ASSERTION(PR_FALSE, "Could not create a History Entry. Out of memory");
      return (nsHistoryEntry *)nsnull; 
   }

   if (aStickyContainer == aContainer) {
	   hEntry->Create(aStickyUrl, aContainer, aReferrer, aParent, aSHist);
   }
   else {
      // Get the  webshell's url.
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aContainer);
      nsCOMPtr<nsIURI> uri;
      docShell->GetCurrentURI(getter_AddRefs(uri));
      if (uri) {
          nsXPIDLCString spec;
          uri->GetSpec(getter_Copies(spec));
	      hEntry->Create(spec, aContainer, aReferrer, aParent, aSHist);
      }
   }
   
  // If the webshell has children, go thro' the child list and create 
  // the history tree for the children recursively.

  PRInt32  cnt = 0;
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aContainer));
  docShellAsNode->GetChildCount(&cnt);
  if (cnt > 0) {
    for (int i=0; i<cnt; i++) {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
      docShellAsNode->GetChildAt(i, getter_AddRefs(docShellAsItem));
      nsCOMPtr<nsIWebShell> childWS(do_QueryInterface(docShellAsItem));
      if (childWS) {
        GenerateTree(aStickyUrl, aStickyContainer, childWS, hEntry, aSHist, aReferrer);
      }
    }
  }
  return hEntry;
}



/* Get the historyentry corresponding to a WebShell */
nsHistoryEntry *
nsHistoryEntry::GetHistoryForWS(nsIWebShell * aWebShell) {
  nsCOMPtr<nsIWebShell> cWS = nsnull;
  nsHistoryEntry * result = nsnull;


  /* Get the webshell  for the current entry */
  GetWebShell(*getter_AddRefs(cWS));

  if ((cWS.get()) == aWebShell) {
     return this;
  }

  PRInt32   cnt = GetChildCnt();
  if (cnt > 0) {
    for(int i=0; i<cnt; i++) {

      nsHistoryEntry * child = nsnull;
      GetChildAt(i, child);
      if (child) {
        result = child->GetHistoryForWS(aWebShell);
      }
      if (result)
        break;    
    }
  }

  return (nsHistoryEntry *) result;

}  /* Get subtree for WS */


/* Load the history item in the window */
PRBool 
nsHistoryEntry::Load(nsIWebShell * aPrevEntry, PRBool aIsReload) {

   nsHistoryEntry * cur=nsnull;
   PRBool urlChanged = PR_FALSE;
   int i = 0; 
   //nsIWebShell * pWS = nsnull, *cWS=nsnull;
   nsIWebShell *prev=nsnull;
   PRBool result = PR_FALSE;
   nsAutoString  cSURL,  pSURL;
   char * cURL=nsnull;   

   cur = this;
   prev = aPrevEntry;
   nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(prev));

   if (!cur || !prev) {
     return NS_ERROR_NULL_POINTER;
   }

   nsCOMPtr<nsIDocShell> prevShell;
   if (prev)
     prevShell = do_QueryInterface(prev);
   if (prevShell) {
     nsCOMPtr<nsIURI> pURI;
     if (NS_SUCCEEDED(prevShell->GetCurrentURI(getter_AddRefs(pURI))) && pURI) {
       nsXPIDLCString spec;
       if (NS_SUCCEEDED(pURI->GetSpec(getter_Copies(spec))))
         pSURL.Assign(spec);
     }
   }

   if (cur) {
     cur->GetURL(&cURL);
     cSURL = cURL;
   }
     
   //Compare the URLs
   {
     if ((pSURL).Equals(cURL))
       urlChanged = PR_FALSE;
     else
       urlChanged = PR_TRUE;
   }  // compareURLs

   if (urlChanged || aIsReload) {
      if (prevShell) {
         PRBool isInSHist=PR_FALSE, isLoadingDoc=PR_FALSE;
         prev->GetIsInSHist(isInSHist);
         mHistoryList->GetLoadingFlag(&isLoadingDoc);
         nsCOMPtr<nsIURI> newURI;
         NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(newURI), cSURL), NS_ERROR_FAILURE);
     
         if ((isInSHist && isLoadingDoc) || aIsReload) {
            if (APP_DEBUG) printf("SessionHistory::Load Loading URL %s in webshell %x\n", cSURL.ToNewCString(), (unsigned int) prev);

            /* Get the child count of the current page and previous page */
            PRInt32  pcount=0, ccount=0;
            docShellAsNode->GetChildCount(&pcount);
            ccount = cur->GetChildCnt();

            nsCOMPtr<nsISupports>  historyObject;
            nsLoadType   loadType = (nsLoadType)nsIChannel::LOAD_NORMAL;
            if (!aIsReload) {
              loadType = (nsLoadType) nsISessionHistory::LOAD_HISTORY;			  
              GetHistoryState(getter_AddRefs(historyObject));
			}


			PRUnichar * uniURL = cSURL.ToNewUnicode();
            prev->SetURL(uniURL);
            nsAutoString referrer(mReferrer);
         prev->SessionHistoryInternalLoadURL(uniURL, loadType, historyObject,
                          mReferrer ? referrer.GetUnicode() : nsnull);
                           
			Recycle(uniURL);

            if (aIsReload && (pcount > 0)) {
              /* If this is a reload, on a page with frames, you want to return
               * true so that consecutive calls by the frame children in to 
               * session History will get compared properly. We must fall into
               * this case only for top level webshells with frame children.
               */
               if (APP_DEBUG)  printf("Returning from Load(). Located a webshell with frame children\n");
			   Recycle(cURL);
               return PR_TRUE;
            }
			Recycle(cURL);
			return PR_TRUE;
         }
         else if (!isInSHist && isLoadingDoc) {
		   PRUnichar * uniURL = cSURL.ToNewUnicode();
		   prev->SetURL(uniURL);
		   Recycle(uniURL);


		   if (APP_DEBUG) printf("Changing URL  to %s in webshell\n", cSURL.ToNewCString());
		   Recycle(cURL);
		   return PR_TRUE;

         }
      }
   }
   else if (!urlChanged ) {
       /* Mark the changed flag to false. This is used in the end to determine
        * whether we are done with the whole loading process for this history
        */
       if (APP_DEBUG) printf("SessionHistory::Load URLs in webshells %x & %x match \n", (unsigned int) mWS, (unsigned int) prev);	   
   }

   /* Make sure the child windows are in par */
   PRInt32  cnt=0, ccnt=0, pcnt=0;
   ccnt = cur->GetChildCnt();
   docShellAsNode->GetChildCount(&pcnt);

   /* If the current entry to be loaded and the one on page don't have
    * the same # of children, maybe the one on screen is is in the process of
    * building. Don't compare the children.
    */
  cnt = ccnt;
  if (pcnt < ccnt)
    cnt = pcnt;
    
   for (i=0; i<cnt; i++){
      nsHistoryEntry *cChild=nsnull;
      cur->GetChildAt(i, cChild);    // historyentry
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
      docShellAsNode->GetChildAt(i, getter_AddRefs(docShellAsItem));   //webshell
      nsCOMPtr<nsIWebShell> pChild(do_QueryInterface(docShellAsItem));
      result = cChild->Load(pChild, PR_FALSE);
      if (result)
         break;
   }
   
   if (ccnt != pcnt)
     result = PR_TRUE;
	 
 
//   NS_IF_RELEASE(aPrevEntry);

     Recycle(cURL);
     return result;
}  /* Load */


/* Compare the history item with the content area */
PRBool 
nsHistoryEntry::Compare(nsIWebShell * aPrevEntry, PRBool aIsReload) {

   nsHistoryEntry * cur=nsnull;
   PRBool urlChanged = PR_FALSE;
   int i = 0; 
   //nsIWebShell * pWS = nsnull, *cWS=nsnull;
   nsIWebShell *prev=nsnull;
   PRBool result = PR_FALSE;
   char * cURL=nsnull;   
   nsAutoString  cSURL,  pSURL;

   cur = this;
   prev = aPrevEntry;
   nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(prev));

//   NS_ADDREF(aPrevEntry);

   if (!cur || !prev) {
     return NS_ERROR_NULL_POINTER;
   }
   if (prev) {
     nsCOMPtr<nsIDocShell> prevShell = do_QueryInterface(prev);
     nsCOMPtr<nsIURI> pURI;
     if (NS_SUCCEEDED(prevShell->GetCurrentURI(getter_AddRefs(pURI)))) {
       nsXPIDLCString spec;
       if (NS_SUCCEEDED(pURI->GetSpec(getter_Copies(spec))))
         pSURL.Assign(spec);
     }
   }
   if (cur) {
     cur->GetURL(&cURL);
     cSURL = (cURL);
   }
   //Compare the URLs
   {
     if (pSURL.Equals(cURL))
       urlChanged = PR_FALSE;
     else
       urlChanged = PR_TRUE;
   }  // compareURLs

   if (urlChanged /*|| aIsReload*/) {
	   if (APP_DEBUG) 
		   printf("SessionHistory::Compare URLs in webshells %x & %x don't match \n", (unsigned int) mWS, (unsigned int) prev);
	   Recycle (cURL);
	   return PR_TRUE;

   }
   else if (!urlChanged ) {
       /* Mark the changed flag to false. This is used in the end to determine
        * whether we are done with the whole loading process for this history
        */
       if (APP_DEBUG) printf("SessionHistory::Compare URLs in webshells %x & %x match \n", (unsigned int) mWS, (unsigned int) prev);	   
   }

   /* Make sure the child windows are in par */

   PRInt32  cnt=0, ccnt=0, pcnt=0;
   ccnt = cur->GetChildCnt();
   docShellAsNode->GetChildCount(&pcnt);

   /* If the current entry to be loaded and the one on page don't have
    * the same # of children, maybe the one on screen is is in the process of
    * building. Don't compare the children.
    */
  cnt = ccnt;
  if (pcnt < ccnt)
    cnt = pcnt;
    
   for (i=0; i<cnt; i++){
      nsHistoryEntry *cChild=nsnull;
      cur->GetChildAt(i, cChild);    // historyentry
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem;
      docShellAsNode->GetChildAt(i, getter_AddRefs(docShellAsItem));   //webshell
      nsCOMPtr<nsIWebShell> pChild(do_QueryInterface(docShellAsItem));
      result = cChild->Compare(pChild, PR_FALSE);
      if (result)
         break;
   }
   if (ccnt != pcnt)
     result = PR_TRUE;
 

//   NS_IF_RELEASE(aPrevEntry);
     Recycle(cURL);
     return result;
}  /* Compare */



nsHistoryEntry *
nsHistoryEntry::GetRootDoc(void) {

  nsHistoryEntry * top= this;

  for (;;) {
    nsHistoryEntry* parent;
    top->GetParent(parent);
    if (parent == nsnull)
      break;
    top = parent;
  }
//     NS_ADDREF(top);
     return top;
}

MOZ_DECL_CTOR_COUNTER(nsSessionHistory);

nsSessionHistory::nsSessionHistory()
{
  MOZ_COUNT_CTOR(nsSessionHistory);

  mHistoryLength = 0;
  mHistoryCurrentIndex = -1;
  mIsLoadingDoc = 0;
  mHistoryEntryInLoad = (nsHistoryEntry *) nsnull; 
  NS_INIT_REFCNT();
}


nsSessionHistory::~nsSessionHistory()
{
  MOZ_COUNT_DTOR(nsSessionHistory);

  // Delete all HistoryEntries

  for(int i=0; i<mHistoryLength; i++) {
     nsHistoryEntry * hEntry = (nsHistoryEntry *)mHistoryEntries[i];
     if (hEntry)
        delete hEntry;
     //NS_IF_RELEASE(hEntry);
  }
  mHistoryLength=0;
  mHistoryCurrentIndex=-1;

}

//ISupports...
NS_IMPL_ISUPPORTS1(nsSessionHistory, nsISessionHistory);

/**
  * Called to a new page is visited either through link click or
  * by typing in the urlbar.
  */
NS_IMETHODIMP
nsSessionHistory::Add(const char * aURL, const char * aReferrer, nsIWebShell * aWebShell)
{
  //nsresult  rv = NS_OK;
   nsHistoryEntry * hEntry = nsnull;

   if (!aWebShell) {
      return NS_ERROR_NULL_POINTER;
   }
   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(aWebShell));
   nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
   docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));

   nsCOMPtr<nsIWebShell> parent(do_QueryInterface(parentAsItem));

   if (!parent) {
     if(mIsLoadingDoc) {
        /* We are currently loading a history entry. Pass it to
		 * Load() to check if the URLs match. Load() will take care
		 * of differences in URL
		 */
        
        nsresult ret = mHistoryEntryInLoad->Compare(aWebShell, PR_FALSE);
        if (!ret) {
          /* The URL in the webshell exactly matches with the
		       * one in history. Clear all flags and return.
           */
   
          mIsLoadingDoc =  PR_FALSE;      
          mHistoryEntryInLoad = (nsHistoryEntry *)nsnull;
        }
        return NS_OK;     
      }  // mIsLoadingDoc

      hEntry = new nsHistoryEntry();
      if (!hEntry) {
         NS_ASSERTION(PR_FALSE, "nsSessionHistory::add Low memory");
         return NS_ERROR_OUT_OF_MEMORY;
      }

      hEntry->Create(aURL, aWebShell, aReferrer, nsnull, this);

      /* Set the flag in webshell that indicates that it has been
       * added to session History 
       */
      aWebShell->SetIsInSHist(PR_TRUE);

      // Add the URL  to the history
      if ((mHistoryLength - (mHistoryCurrentIndex+1)) > 0) {
        /* We are somewhere in the middle of the history and a
         * new page was visited. Purge all entries from the current index
         * till the end of the list and append the current page to the
         * list
         */

         for(int i=mHistoryLength-1; i>mHistoryCurrentIndex; i--) {
            nsHistoryEntry * hEntry2 = (nsHistoryEntry *)mHistoryEntries.ElementAt(i);
            //NS_IF_RELEASE(hEntry2);
            delete hEntry2;
            mHistoryEntries.RemoveElementAt(i);
            mHistoryLength--;
         }
     }

     mHistoryEntries.AppendElement((void *)hEntry);
     mHistoryLength++;
     mHistoryCurrentIndex++;
	 if (APP_DEBUG) printf("nsSessionHistory::Add CurrentIndex = %d, HistoryLength = %d \n", mHistoryCurrentIndex, mHistoryLength);
     return NS_OK;
   }   // (!mParent)

   else { 
      /* This is a frame webshell. Check if it is a new frame. If so,
       * append to the existing history entry. Else, create a 
       * new tree to record the change in URL 
       */
     
      PRBool inSHist = PR_TRUE;
   
      aWebShell->GetIsInSHist(inSHist);

      if (!inSHist) {

         nsHistoryEntry * curEntry=nsnull, * newEntry=nsnull, 
                        * parentEntry=nsnull;

         if (!mIsLoadingDoc) {
           /*This is a newly created frame. Just add it to the current entry */

           // Get a handle to the parent of the new frame WS;
           curEntry = (nsHistoryEntry *) mHistoryEntries.ElementAt(mHistoryCurrentIndex);   
           // Get the history entry corresponding to the parentWS
           if (curEntry)
              parentEntry = curEntry->GetHistoryForWS(parent);

           // Create a new HistoryEntry for the frame & init values
           newEntry = new nsHistoryEntry();
           if (!newEntry) {
             NS_ASSERTION(PR_FALSE, "nsSessionHistory::add Low memory");
             return NS_ERROR_OUT_OF_MEMORY;
           }
           newEntry->Create(aURL, aWebShell, aReferrer, parentEntry, this);
           aWebShell->SetIsInSHist(PR_TRUE);

         }  // !mIsLoadingDoc
         else {

          /* We are in the middle of loading a page. Pass it on to Load
           * to  check if we are loading the right page
           */
          PRBool ret=PR_TRUE;
          
          nsIWebShell * root=nsnull;
          aWebShell->GetRootWebShell(root);
          if (!root) {
			      NS_ASSERTION(0,"nsSessionHistory::add Couldn't get root webshell");
            return NS_OK;
          }
		      ret = mHistoryEntryInLoad->Load(root, PR_FALSE);
          if (!ret) {
            /* The page in the webshel matches exactly with the one in history.
             * Clear all flags and return.
             */
            
               mIsLoadingDoc =  PR_FALSE;      
               mHistoryEntryInLoad = (nsHistoryEntry *)nsnull;
            }  //!ret
            aWebShell->SetIsInSHist(PR_TRUE);
			      if (root)
			         NS_RELEASE(root);
          }   
       }  // !InSHist
       else  {
          /* THis page is in history. May be the frame page changed. */
          if (!mIsLoadingDoc) {
             /* We are not in the middle of loading a history entry. Add this
              * to the history entry
              */
             nsIWebShell * root = nsnull;
             nsHistoryEntry * newEntry = nsnull;

             aWebShell->GetRootWebShell(root);
  
             if (root)   
                newEntry = GenerateTree(aURL, aWebShell, root, nsnull, this, aReferrer);
             if (newEntry) {
                if ((mHistoryLength - (mHistoryCurrentIndex+1)) > 0) {
                /* We are somewhere in the middle of the history and a
                 * new page was visited. Purge all entries from the current 
                 * index till the end of the list and append the current 
                 * page to the list
                 */

                for(int i=mHistoryLength-1; i>mHistoryCurrentIndex; i--) {
                   nsHistoryEntry * hiEntry = (nsHistoryEntry *)mHistoryEntries.ElementAt(i);
                   //NS_IF_RELEASE(hiEntry);
                   delete hiEntry;
                   mHistoryEntries.RemoveElementAt(i);
                   mHistoryLength--;
                 }
              }
              mHistoryEntries.AppendElement((void *)newEntry);
              mHistoryLength++;
              mHistoryCurrentIndex++;        
              return NS_OK;
             }  //newEntry
			       if (root)
				       NS_RELEASE(root);
           }  // (!mIsLoadingDoc)
           else   {
        
              /* We are in the middle of loading a history entry.
               * Send it to Load() for Comparison purposes
               */
              nsIWebShell * root=nsnull;
              aWebShell->GetRootWebShell(root);
              if (!root) {
                NS_ASSERTION(0, "nsSessionHistory::add, Couldn't get root webshell");
                return NS_OK;   
              }
              nsresult ret = mHistoryEntryInLoad->Load(root, PR_FALSE);
              if (!ret) {
               /* The page in webshell matches exactly with the one in history.
                * Clear all flags and return.
                */
            
                  mIsLoadingDoc =  PR_FALSE;      
                  mHistoryEntryInLoad = (nsHistoryEntry *)nsnull;

              }  //!ret
			  if (root)
				 NS_RELEASE(root);
            }  //else  for (!mIsloadingDoc)
      }  // else  for (!InSHist)
   }
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::UpdateStatus(nsIWebShell * aWebShell, PRInt32 aStatus) {
	if (!aWebShell)
		return NS_ERROR_NULL_POINTER;

    nsresult res = (nsresult) aStatus;
	if (!mIsLoadingDoc) {
	  if (!NS_SUCCEEDED(res)) {
		  /* Disabling this feature for  now */
#if 0
		 /* if this was a fresh page load and if it failed,
	      * remove the entry for in in Session History  
	      */
         nsHistoryEntry * cEntry = (nsHistoryEntry *) mHistoryEntries.ElementAt(mHistoryCurrentIndex);
		 mHistoryEntries.RemoveElementAt(mHistoryCurrentIndex);
	     delete cEntry;
	     mHistoryCurrentIndex--;
		 mHistoryLength--;
	     return NS_OK;
#endif /* 0 */
	  }
	}  // (!mIsLoadingDoc)
	else {
        /* We are currently in the middle of loading a history entry.
		 * Not sure if we are done with all the subframes etc...
		 */
		if (!NS_SUCCEEDED(res)) {
		  /* 
		   * But if the History page load failed for some reason,
		   * Clear the loading flags, leave the current index as it is for now.
		   */
           mIsLoadingDoc= PR_FALSE;
		   mHistoryEntryInLoad = (nsHistoryEntry *) nsnull;
		   return NS_OK;
		}
        if (!aWebShell || !mHistoryEntryInLoad) {
           return NS_ERROR_NULL_POINTER;
		}
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(aWebShell));
        nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
        docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));

        nsCOMPtr<nsIWebShell> parent(do_QueryInterface(parentAsItem));

        if (!parent) {
          /* Pass the document to Load() to check if the URLs match. 
		   * Load() will take care of differences in URL
		   */
        
          nsresult ret = mHistoryEntryInLoad->Compare(aWebShell, PR_FALSE);
          if (!ret) {
            /* The URL in the webshell exactly matches with the
		     * one in history. Clear all flags and return.
             */
   
            mIsLoadingDoc =  PR_FALSE;      
            mHistoryEntryInLoad = (nsHistoryEntry *)nsnull;
		  }
          return NS_OK;     
		}  // (!parent)
		else { 
          /* This is a frame webshell */
          PRBool inSHist = PR_TRUE;   
          aWebShell->GetIsInSHist(inSHist);

          if (inSHist) {
            /* This page is in history. May be the frame page changed. 
             * Send it to Load() for Comparison purposes
             */
              nsIWebShell * root=nsnull;
              aWebShell->GetRootWebShell(root);
              if (!root) {
			      NS_ASSERTION(0,"nsSessionHistory::add Couldn't get root webshell");
                 return NS_OK;
			  }
		      PRBool ret = mHistoryEntryInLoad->Compare(root, PR_FALSE);              
              if (!ret) {
                /* The page in webshell matches exactly with the one in history.
                 * Clear all flags and return.
                 */            
                mIsLoadingDoc =  PR_FALSE;      
                mHistoryEntryInLoad = (nsHistoryEntry *)nsnull;

              }  //!ret
			  if (root)
				 NS_RELEASE(root);            
		  }  //inSHist
		} // else for (!parent)
	} // else for (!mIsLoadingDoc)

return NS_OK;

}


NS_IMETHODIMP
nsSessionHistory::Goto(PRInt32 aGotoIndex, nsIWebShell * prev, PRBool aIsReload)
{
   PRBool result = PR_FALSE;

   if ((aGotoIndex < 0) ||  (aGotoIndex >= mHistoryLength))
      return NS_ERROR_FAILURE;
   if (!prev)
	   return NS_ERROR_NULL_POINTER;

   nsHistoryEntry * hCurrentEntry = nsnull;

   //get a handle to the current page to be passed to nsHistoryEntry
   // as the previous page.
   hCurrentEntry = (nsHistoryEntry *) mHistoryEntries.ElementAt(aGotoIndex);

   // Set the flag that indicates that we are currently
   // traversing with it the history
   mIsLoadingDoc = PR_TRUE;
   mHistoryEntryInLoad = hCurrentEntry;

  //Load the page
   char * url;
   hCurrentEntry->GetURL(&url);
   if (APP_DEBUG && url) printf("nsSessionHistory::Goto, Trying to load URL %s\n", url);
   Recycle (url);
  

   // Save the history state for the current index before loading the next one
   int indix = 0;
   GetCurrentIndex(&indix);
   if (indix >= 0) {
     nsCOMPtr<nsISupports>  historyState;
     nsresult rv = prev->CaptureHistoryState(getter_AddRefs(historyState));
	 if (NS_SUCCEEDED(rv) && historyState)
		 SetHistoryObjectForIndex(indix, historyState);
   }

   // Set the new current index
   mHistoryCurrentIndex = aGotoIndex;
   if (hCurrentEntry != nsnull) {
       result = hCurrentEntry->Load(prev, aIsReload);
   }
    if (!result) {
       mIsLoadingDoc = PR_FALSE;
       mHistoryEntryInLoad = (nsHistoryEntry *) nsnull;
    }

   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::Reload(nsIWebShell * aPrev, nsLoadFlags aReloadFlags)
{
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aPrev));
   NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(webNav->Reload(nsIWebNavigation::reloadNormal), 
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GoBack(nsIWebShell * aPrev)
{
   //nsHistoryEntry * hEntry=nsnull;

   if (mHistoryCurrentIndex <= 0) 
      return NS_ERROR_FAILURE;
   
   //Error check on aPrev done in Goto()
   Goto((mHistoryCurrentIndex -1), aPrev, PR_FALSE);
   return NS_OK;

}

NS_IMETHODIMP
nsSessionHistory::GoForward(nsIWebShell * aPrev)
{
   if (mHistoryCurrentIndex == mHistoryLength-1)
      return NS_ERROR_FAILURE;
   //Error check on aPrev done in Goto()
   Goto((mHistoryCurrentIndex+1), aPrev, PR_FALSE);
   return NS_OK;

}

NS_IMETHODIMP
nsSessionHistory::SetLoadingFlag(PRBool aFlag)
{
  mIsLoadingDoc = aFlag;
  return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::GetLoadingFlag(PRBool *aFlag)
{
  if (!aFlag)
	return NS_ERROR_NULL_POINTER;

  *aFlag = mIsLoadingDoc;
  return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::CanGoForward(PRBool * aResult)
{
   if (!aResult)
  	 return NS_ERROR_NULL_POINTER;

   if (mHistoryCurrentIndex == mHistoryLength-1)
      *aResult = PR_FALSE;
   else
      *aResult = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::CanGoBack(PRBool * aResult)
{
   if (!aResult)
	 return NS_ERROR_NULL_POINTER;

   if (mHistoryCurrentIndex == 0)
      *aResult = PR_FALSE;
   else
      *aResult = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GetHistoryLength(PRInt32 * aResult)
{
  if (!aResult)
     return NS_ERROR_NULL_POINTER;

   *aResult=mHistoryLength;
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::GetCurrentIndex(PRInt32 * aResult)
{
   if (!aResult)
     return NS_ERROR_NULL_POINTER;

   *aResult = mHistoryCurrentIndex;
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::GetURLForIndex(PRInt32 aIndex, char** aURL)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
  {
     if (APP_DEBUG) printf("nsSessionHistory::GetURLForIndex Returning error in GetURL for Index\n");
     return NS_ERROR_FAILURE;
  }
  if (!aURL)
	  return NS_ERROR_NULL_POINTER;

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);
  
  if (hist)
    hist->GetURL(aURL);

  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::SetURLForIndex(PRInt32 aIndex, const char* aURL)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
     return NS_ERROR_FAILURE; 
  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  hist->SetURL(aURL);
  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GetTitleForIndex(PRInt32 aIndex,  PRUnichar** aTitle)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
  {
     if (APP_DEBUG) printf("nsSessionHistory::GetTitleForIndex Returning error in GetURL for Index\n");
     return NS_ERROR_FAILURE;
  }

  if (!aTitle)
	  return NS_ERROR_NULL_POINTER;

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);
  
  if (hist)
    hist->GetTitle(aTitle);

  return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::SetTitleForIndex(PRInt32 aIndex, const PRUnichar* aTitle)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
     return NS_ERROR_FAILURE; 

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  hist->SetTitle(aTitle);
  nsString title(aTitle);
  if(APP_DEBUG) printf("Setting title %s for index %d\n", title.ToNewCString(), aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GetHistoryObjectForIndex(PRInt32 aIndex, nsISupports** aState)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
  {
     if (APP_DEBUG) printf("nsSessionHistory::GetHistoryObjectForIndex Returning error \n");
     return NS_ERROR_FAILURE;
  }

  if (!aState)
	  return NS_ERROR_NULL_POINTER;

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  
  if (hist)
    hist->GetHistoryState(aState);

  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::SetHistoryObjectForIndex(PRInt32 aIndex, nsISupports* aState)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
     return NS_ERROR_FAILURE; 

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  hist->SetHistoryState(aState);
  return NS_OK;
}

