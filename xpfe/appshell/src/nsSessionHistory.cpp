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


#include "nsISessionHistory.h"
#include "nsVoidArray.h"
//#include "nsIDocumentLoaderObserver.h"
#include "nsIWebShell.h"
#include "prmem.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsCRT.h"
#include "nscore.h"

// Interface ID for nsIHistoryEntry
#define NS_IHISTORY_ENTRY_IID \
{ 0x68e73d53, 0x12eb, 0x11d3, { 0xbd, 0xc0, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44 } \
}

/* Define Class IDs */
static NS_DEFINE_IID(kWebShellCID,         NS_WEB_SHELL_CID);


/* Define Interface IDs */
static NS_DEFINE_IID(kIFactoryIID,            NS_IFACTORY_IID);
static NS_DEFINE_IID(kISessionHistoryIID,     NS_ISESSION_HISTORY_IID);
static NS_DEFINE_IID(kIHistoryEntryIID,       NS_IHISTORY_ENTRY_IID);


// Advance declarations
class nsHistoryEntry;

static nsHistoryEntry *  GenerateTree(nsIWebShell * aWebShell,nsHistoryEntry *aparent, nsISessionHistory * aSHist);

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
  nsresult Create(nsIWebShell * aWebShell, nsHistoryEntry * aParent, nsISessionHistory * aSHist);

  /**
   * Load the entry in the content Area
   */
  PRBool  Load(nsIWebShell * aPrevEntry, PRBool aIsReload);

  /**
   * Destroy the  historyentry
   */
  nsresult DestroyChildren();

  /**
   * Get the Title of the page 
   */
  nsresult GetTitle(const PRUnichar ** aName);

  /**
   * Set the title of the page 
   */
  nsresult SetTitle(const PRUnichar * aName);

  /**
   * Get the URL  of the page 
   */
  nsresult GetURL(const PRUnichar ** aURL);

  /**
   * Set the URL  of the page 
   */
  nsresult SetURL(const PRUnichar * aURL);

  /**
   * Get the webshell  of the page 
   */
  nsresult GetWebShell(nsIWebShell *& aName);

  /**
   * Set the webshell  of the page 
   */
  nsresult SetWebShell(nsIWebShell * aName);

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
  nsString *          mTitle;  // Name of the document
  nsVoidArray         mChildren;  //  children list
  PRInt32             mChildCount; // # of children
  nsHistoryEntry *    mParent;     
  nsISessionHistory * mHistoryList;  // The handle to the session History to 
                                    // which this entry belongs
};

nsHistoryEntry::nsHistoryEntry()
{
   mChildCount = 0;
   mWS  = nsnull;
   mHistoryList = nsnull;
   mParent = nsnull;
   mURL = nsnull;
   mTitle = nsnull;
   
 
//  NS_INIT_REFCNT();
}

nsHistoryEntry::~nsHistoryEntry()
{
   if (mTitle)
     delete mTitle;
   if (mURL)
	 delete mURL;
   NS_IF_RELEASE(mWS);
   NS_IF_RELEASE(mHistoryList);

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
nsHistoryEntry::GetURL(const PRUnichar** aURL)
{
  *aURL = mURL->GetUnicode();
  return NS_OK;
}

nsresult
nsHistoryEntry::SetURL(const PRUnichar* aURL)
{

  if (mURL)
    delete mURL;
  mURL =  new nsString(aURL);
  return NS_OK;
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
  mWS = aWebShell;
  return NS_OK;
}

nsresult
nsHistoryEntry::GetTitle(const PRUnichar** aTitle)
{
  if (mTitle)
    *aTitle = mTitle->GetUnicode();
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
	NS_IF_RELEASE(mHistoryList);
   mHistoryList = aHistoryListID;
   NS_ADDREF(aHistoryListID);
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
nsHistoryEntry::Create(nsIWebShell * aWebShell, nsHistoryEntry * aParent, nsISessionHistory * aSHist) {

   const PRUnichar * url = nsnull;


   // Get the  webshell's url.
   aWebShell->GetURL(&url);
   // save the webshell's URL in the history entry
   SetURL(url);


   //Save the webshell id
   SetWebShell(aWebShell);
   
   if (APP_DEBUG) printf("SessionHistory::Create Creating Historyentry %x  for webshell %x, parent entry = %x\n", (unsigned int)this, (unsigned int)aWebShell, (unsigned int) aParent);

   if (aParent)
     aParent->AddChild(this);

   // Save the handle to the window's history list   
   SetSessionHistoryID(aSHist);

   return NS_OK;
}

static nsHistoryEntry *  
GenerateTree(nsIWebShell * aWebShell, nsHistoryEntry * aParent, nsISessionHistory * aSHist) {

   nsHistoryEntry * hEntry = nsnull;

   hEntry = new nsHistoryEntry;

   if (!hEntry) {
      NS_ASSERTION(PR_FALSE, "Could not create a History Entry. Out of memory");
      return (nsHistoryEntry *)nsnull; 
   }

   hEntry->Create(aWebShell, aParent, aSHist);

  // If the webshell has children, go thro' the child list and create 
  // the history tree for the children recursively.

  PRInt32  cnt = 0;
  aWebShell->GetChildCount(cnt);
  if (cnt > 0) {
    for (int i=0; i<cnt; i++) {
      nsIWebShell * childWS = nsnull;
      //nsHistoryEntry * hChild = nsnull;

      aWebShell->ChildAt(i, childWS); 
      if (childWS) {
        GenerateTree(childWS, hEntry, aSHist);
      }
    }
  }
  return hEntry;

}



/* Get the historyentry corresponding to a WebShell */
nsHistoryEntry *
nsHistoryEntry::GetHistoryForWS(nsIWebShell * aWebShell) {
  nsIWebShell * cWS = nsnull;
  nsHistoryEntry * result = nsnull;


  /* Get the webshell  for the current entry */
  GetWebShell(cWS);

  if (cWS == aWebShell) {
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
   nsString*  cSURL=nsnull, * pSURL=nsnull;
   const PRUnichar *  pURL=nsnull, * cURL=nsnull;   

   cur = this;
   prev = aPrevEntry;

   if (prev) {
     prev->GetURL(&pURL);
     pSURL = new nsString(pURL);

   }
   if (cur) {
     cur->GetURL(&cURL);
     cSURL = new nsString(cURL);
   }

     
//   NS_ADDREF(aPrevEntry);

   if (!cur || !prev) {
     return NS_ERROR_NULL_POINTER;
   }

   //Compare the URLs
   {
     if ((*pSURL) == cURL)
       urlChanged = PR_FALSE;
     else
       urlChanged = PR_TRUE;
   }  // compareURLs

 
   /* The URL to be loaded in it */
   cur->GetURL(&cURL);

   if (urlChanged || aIsReload) {
      if (prev) {
         PRBool isInSHist=PR_FALSE, isLoadingDoc=PR_FALSE;
         prev->GetIsInSHist(isInSHist);
         mHistoryList->GetLoadingFlag(isLoadingDoc);
     
         if ((isInSHist && isLoadingDoc) || aIsReload) {
            if (APP_DEBUG) printf("SessionHistory::Load Loading URL %s in webshell %x\n", cSURL->ToNewCString(), (unsigned int) prev);

            /* Get the child count of the webshell for a later use */
            PRInt32  ccount=0;
            prev->GetChildCount(ccount);

            prev->LoadURL(cURL, nsnull, PR_FALSE, (nsLoadFlags) (aIsReload?nsIChannel::LOAD_NORMAL:LOAD_HISTORY));
            if (aIsReload && (ccount > 0)) {
              /* If this is a reload, on a page with frames, you want to return
               * true so that consecutive calls by the frame children in to 
               * session History will get compared properly. We must fall into
               * this case only for top level webshells with frame children.
               */
               if (APP_DEBUG)  printf("Returning from Load(). Located a webshell with frame children\n");
               return PR_TRUE;
            }
         }
         else if (!isInSHist && isLoadingDoc) {
           prev->SetURL(cURL);
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
   prev->GetChildCount(pcnt);

   /* If the current entry to be loaded and the one on page don't have
    * the same # of children, maybe the one on screen is is in the process of
    * building. Don't compare the children.
    */
  cnt = ccnt;
  if (pcnt < ccnt)
    cnt = pcnt;
    
   for (i=0; i<cnt; i++){
      nsHistoryEntry *cChild=nsnull;
      nsIWebShell *  pChild=nsnull;

      cur->GetChildAt(i, cChild);    // historyentry
      prev->ChildAt(i, pChild);   //webshell

      result = cChild->Load(pChild, PR_FALSE);
      if (result)
         break;
   }
   if (ccnt != pcnt)
     result = PR_TRUE;
 

//   NS_IF_RELEASE(aPrevEntry);

     return result;
}  /* Load */


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


class nsSessionHistory: public nsISessionHistory
{

public:
   nsSessionHistory();

   //nsISupports
   NS_DECL_ISUPPORTS

  //nsISessionHistory interfaces


  /**
   * Go forward in history 
   */
  NS_IMETHOD GoForward(nsIWebShell * prev);

  /**
   * Go Back in History
   */
  NS_IMETHOD GoBack(nsIWebShell * prev);

  /**
   * Reload the current history entry
   */
#ifdef NECKO
  NS_IMETHOD Reload(nsIWebShell * aPrev, nsLoadFlags aReloadFlags);
#else
  NS_IMETHOD Reload(nsIWebShell * aPrev, nsURLReloadType aReloadType);
#endif
  
  /**
   * whether you can go forward in History
   */
  NS_IMETHOD canForward(PRBool &aResult);

  /**
   * whether you can go back in History
   */
  NS_IMETHOD canBack(PRBool &aResult);

  /**
   * Add a new URL to the History List
   */
  NS_IMETHOD add(nsIWebShell * aWebShell);

  /**
   * Go to a particular point in the history list
   */
  NS_IMETHOD  Goto(PRInt32 aHistoryIndex, nsIWebShell * prev, PRBool aIsReload);

  /**
   * Get the length of the History list
   */
  NS_IMETHOD getHistoryLength(PRInt32 & aResult);

  /**
   * Get the index of the current document in the history list
   */
  NS_IMETHOD getCurrentIndex(PRInt32 & aResult);

  /**
   * Make a clone of the Session History. This will 
   * make a deep copy of the Session history data structures. 
   * This is used when you create a navigator window from the
   * current browser window
   */
//  NS_IMETHOD cloneHistory(nsISessionHistory * aSessionHistory);

  /**
   * Set the flag whether a history entry is in the middle of loading a
   * doc. See comments below for details
   */
  NS_IMETHOD SetLoadingFlag(PRBool aFlag);

  /**
   * Get the flag whether a history entry is in the middle of loading a
   * doc. See comments below for details
   */
  NS_IMETHOD GetLoadingFlag(PRBool &aFlag);

  /**
   * Set the historyentry that is in the middle of loading a doc
   */
  NS_IMETHOD SetLoadingHistoryEntry(nsHistoryEntry * aHistoryEntry);


  /**
   * Get the URL of the index
   */
  NS_IMETHOD GetURLForIndex(PRInt32 aIndex, const PRUnichar ** aURL);


  /**
   * Set the URL of the index
   */
  NS_IMETHOD SetURLForIndex(PRInt32 aIndex, const PRUnichar * aURL);


  /**
   * Get the title of the index
   */
  NS_IMETHOD GetTitleForIndex(PRInt32 aIndex, const PRUnichar ** aTitle);


  /**
   * Set the Title of the index
   */

  NS_IMETHOD SetTitleForIndex(PRInt32 aIndex, const PRUnichar * aTitle);
protected:

   virtual ~nsSessionHistory();

private:
  PRInt32       mHistoryLength;
  PRInt32       mHistoryCurrentIndex;
  nsVoidArray   mHistoryEntries;
//  PRBool        mIsBackOrForward;

  /** Following  member is used to identify whether we are in the
   *  middle of loading a history document. The mIsLoadingDoc flag is 
   *  used to determine whether the document that is curently  loaded 
   *  in the window s'd go to the  end of the historylist or to be 
   *  handed over to the current history entry (mIndexOfHistoryInLoad)
   *  that is in the process of loading. The current history entry 
   *  being loaded uses this new historyentry to decide whether the 
   *  document is completely in par with the one in history. If not, it
   *  will initiate further loads. When the document currently loaded is
   *  completely on par with the one in history, it will clear the
   *  mIsLoadingDoc flag. Any new URL loaded from then on, will go to the
   *  end of the history list. Note: these members  are static.
   */ 
   PRBool            mIsLoadingDoc;
   nsHistoryEntry *  mHistoryEntryInLoad;


};


nsSessionHistory::nsSessionHistory()
{
  mHistoryLength = 0;
  mHistoryCurrentIndex = -1;
  mIsLoadingDoc = 0;
  mHistoryEntryInLoad = (nsHistoryEntry *) nsnull; 
  NS_INIT_REFCNT();
}


nsSessionHistory::~nsSessionHistory()
{

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
NS_IMPL_ISUPPORTS(nsSessionHistory, kISessionHistoryIID);

/**
  * Called to a new page is visited either through link click or
  * by typing in the urlbar.
  */
NS_IMETHODIMP
nsSessionHistory::add(nsIWebShell * aWebShell)
{
  //nsresult  rv = NS_OK;
   nsHistoryEntry * hEntry = nsnull;
   nsIWebShell * parent = nsnull;

   if (!aWebShell) {
      return NS_ERROR_NULL_POINTER;
   }

   aWebShell->GetParent(parent);

   if (!parent) {
     if(mIsLoadingDoc) {
        /* We are currently loading a history entry. Pass it to
		 * Load() to check if the URLs match. Load() will take care
		 * of differences in URL
		 */
        
        nsresult ret = mHistoryEntryInLoad->Load(aWebShell, PR_FALSE);
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
      hEntry->Create(aWebShell, nsnull, this);

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
           nsIWebShell * parentWS = nsnull;
           aWebShell->GetParent(parentWS);

           curEntry = (nsHistoryEntry *) mHistoryEntries.ElementAt(mHistoryCurrentIndex);   
           // Get the history entry corresponding to the parentWS
           if (curEntry)
              parentEntry = curEntry->GetHistoryForWS(parentWS);

           // Create a new HistoryEntry for the frame & init values
           newEntry = new nsHistoryEntry();
           if (!newEntry) {
             NS_ASSERTION(PR_FALSE, "nsSessionHistory::add Low memory");
             return NS_ERROR_OUT_OF_MEMORY;
           }
           newEntry->Create(aWebShell, parentEntry, this);
           aWebShell->SetIsInSHist(PR_TRUE);

		   if (parentWS)
			  NS_RELEASE(parentWS);
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
                newEntry = GenerateTree(root, nsnull, this);
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
   if (parent)
	  NS_RELEASE(parent);
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::Goto(PRInt32 aGotoIndex, nsIWebShell * prev, PRBool aIsReload)
{
   PRBool result = PR_FALSE;

   if ((aGotoIndex < 0) ||  (aGotoIndex >= mHistoryLength))
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
   const PRUnichar * url;
   hCurrentEntry->GetURL(&url);
   nsString  urlString(url);
   if (APP_DEBUG) printf("nsSessionHistory::Goto, Trying to load URL %s\n", urlString.ToNewCString());
  
    if (hCurrentEntry != nsnull)
       result = hCurrentEntry->Load(prev, aIsReload);
    if (!result) {
       mIsLoadingDoc = PR_FALSE;
       mHistoryEntryInLoad = (nsHistoryEntry *) nsnull;
    }

#if 0
  if (aIsReload) {
     prev->LoadURL(urlString.GetUnicode(), nsnull, PR_FALSE); 
  }
  else {
    if (hCurrentEntry != nsnull)
       result = hCurrentEntry->Load(prev);
  }
   
  if (!result) {
     mIsLoadingDoc = PR_FALSE;
     mHistoryEntryInLoad = (nsHistoryEntry *) nsnull;
  }
#endif  /* 0 */


  
   mHistoryCurrentIndex = aGotoIndex;

   return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsSessionHistory::Reload(nsIWebShell * aPrev, nsLoadFlags aReloadFlags)
#else
nsSessionHistory::Reload(nsIWebShell * aPrev, nsURLReloadType aReloadType)
#endif
{

  // Call goto with the Reload flag set to true
  Goto(mHistoryCurrentIndex, aPrev, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GoBack(nsIWebShell * aPrev)
{
   //nsHistoryEntry * hEntry=nsnull;

   if (mHistoryCurrentIndex <= 0) 
      return NS_ERROR_FAILURE;

   Goto((mHistoryCurrentIndex -1), aPrev, PR_FALSE);
   return NS_OK;

}

NS_IMETHODIMP
nsSessionHistory::GoForward(nsIWebShell * aPrev)
{
   if (mHistoryCurrentIndex == mHistoryLength-1)
      return NS_ERROR_FAILURE;

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
nsSessionHistory::GetLoadingFlag(PRBool &aFlag)
{
  aFlag = mIsLoadingDoc;
  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::SetLoadingHistoryEntry(nsHistoryEntry *  aHistoryEntry)
{
  mHistoryEntryInLoad  = aHistoryEntry;
  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::canForward(PRBool & aResult)
{
   if (mHistoryCurrentIndex == mHistoryLength-1)
      aResult = PR_FALSE;
   else
      aResult = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::canBack(PRBool & aResult)
{
   if (mHistoryCurrentIndex == 0)
      aResult = PR_FALSE;
   else
      aResult = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::getHistoryLength(PRInt32 & aResult)
{
   aResult=mHistoryLength;
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::getCurrentIndex(PRInt32 & aResult)
{
   aResult = mHistoryCurrentIndex;
   return NS_OK;
}


NS_IMETHODIMP
nsSessionHistory::GetURLForIndex(PRInt32 aIndex, const PRUnichar** aURL)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
  {
     if (APP_DEBUG) printf("nsSessionHistory::GetURLForIndex Returning error in GetURL for Index\n");
     return NS_ERROR_FAILURE;
  }

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  
  if (hist)
    hist->GetURL(aURL);

  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::SetURLForIndex(PRInt32 aIndex, const PRUnichar* aURL)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
     return NS_ERROR_FAILURE; 

  hist = (nsHistoryEntry *) mHistoryEntries.ElementAt(aIndex);

  hist->SetURL(aURL);
  return NS_OK;
}

NS_IMETHODIMP
nsSessionHistory::GetTitleForIndex(PRInt32 aIndex, const PRUnichar** aTitle)
{

  nsHistoryEntry * hist=nsnull;

  if (aIndex < 0 || aIndex >= mHistoryLength)
  {
     if (APP_DEBUG) printf("nsSessionHistory::GetTitleForIndex Returning error in GetURL for Index\n");
     return NS_ERROR_FAILURE;
  }

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



NS_EXPORT nsresult NS_NewSessionHistory(nsISessionHistory** aResult)
{
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = new nsSessionHistory();
  if (nsnull == *aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

#if 0
NS_IMETHODIMP
cloneHistory(nsISessionHistory * aSessionHistory) {

   return NS_OK;
}
#endif  /* 0 */

//----------------------------------------------------------------------

// Factory code for creating nsSessionHistory

class nsSessionHistoryFactory : public nsIFactory
{
public:
  nsSessionHistoryFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);  
protected:
  virtual ~nsSessionHistoryFactory();
};


nsSessionHistoryFactory::nsSessionHistoryFactory()
{
  NS_INIT_REFCNT();
}

nsresult
nsSessionHistoryFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}

nsSessionHistoryFactory::~nsSessionHistoryFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsSessionHistoryFactory, kIFactoryIID);


nsresult
nsSessionHistoryFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsSessionHistory* inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }


  NS_NEWXPCOM(inst, nsSessionHistory);
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


extern "C" NS_APPSHELL nsresult
NS_NewSessionHistoryFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;

  nsIFactory* inst = new nsSessionHistoryFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
