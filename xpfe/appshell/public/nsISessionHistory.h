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

#ifndef nsISessionHistory_h_
#define nsISessionHistory_h_

#include "nsISupports.h"
#include "nsIFactory.h"
#ifdef NECKO
#include "nsIChannel.h"
#else
#include "nsILoadAttribs.h"
#endif // NECKO

//Forward declarations
class nsHistoryEntry;
class nsISessionHistory;
class nsIWebShell;

// Interface ID for nsISessionistory
#define NS_ISESSION_HISTORY_IID \
{ 0x68e73d51, 0x12eb, 0x11d3, { 0xbd, 0xc0, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44 } \
}

//Class ID for nsISessionHistory
#define NS_SESSION_HISTORY_CID \
{ 0x68e73d52, 0x12eb, 0x11d3, { 0xbd, 0xc0, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44 } \
}

#define LOAD_HISTORY 10

class nsISessionHistory : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISESSION_HISTORY_IID; return iid; }

  /**
   * Get the history entry of the current document on screen
   */
  //  NS_IMETHOD getCurrentHistoryEntry(nsHistoryEntry *& aHistoryEntry) = 0;

  /**
   * Go forward in history 
   */
  NS_IMETHOD GoForward(nsIWebShell * aPrev) = 0;

  /**
   * Go Back in History
   */
  NS_IMETHOD GoBack(nsIWebShell * aPrev) = 0;

  /**
   * Reload the current history entry
   */
#ifdef NECKO
  NS_IMETHOD Reload(nsIWebShell * aPrev, nsLoadFlags aReloadFlags) = 0;
#else
  NS_IMETHOD Reload(nsIWebShell * aPrev, nsURLReloadType aReloadType) = 0;
#endif

  /**
   * whether you can go forward in History
   */
  NS_IMETHOD canForward(PRBool &aResult) = 0;

  /**
   * whether you can go back in History
   */
  NS_IMETHOD canBack(PRBool &aResult) = 0;

  /**
   * Add a new URL to the History List
   */
  NS_IMETHOD add(nsIWebShell * aWebShell) = 0;

  /**
   * Goto to a particular point in history 
   */
  NS_IMETHOD Goto(PRInt32 aHistoryIndex, nsIWebShell * prev, PRBool aIsReloading) = 0;
  /**
   * Get the length of the History list
   */
  NS_IMETHOD getHistoryLength(PRInt32 & aResult) = 0;

  /**
   * Get the index of the current document in the history list
   */
  NS_IMETHOD getCurrentIndex(PRInt32 & aResult) = 0;

  /**
   * Make a clone of the Session History. This will 
   * make a deep copy of the Session history data structures. 
   * This is used when you create a navigator window from the
   * current browser window
   */
  // NS_IMETHOD cloneHistory(nsISessionHistory * aSessionHistory) = 0;
  /**
   * Set the flag whether a history entry is in the middle of loading a
   * doc. See comments below for details
   */
  NS_IMETHOD SetLoadingFlag(PRBool aFlag) = 0;

  /**
   * Get the flag whether a history entry is in the middle of loading a
   * doc. See comments below for details
   */
  NS_IMETHOD GetLoadingFlag(PRBool &aFlag) = 0;

  /**
   * Set the historyentry that is in the middle of loading a doc
   */
  NS_IMETHOD SetLoadingHistoryEntry(nsHistoryEntry * aHistoryEntry) = 0;

  /**
   * Get the URL of the index
   */
  NS_IMETHOD GetURLForIndex(PRInt32 aIndex, const PRUnichar ** aURL) = 0;


  /**
   * Set the URL of the index
   */
  NS_IMETHOD SetURLForIndex(PRInt32 aIndex, const PRUnichar * aURL) = 0;

  /**
   * Get the title of the index
   */
  NS_IMETHOD GetTitleForIndex(PRInt32 aIndex, const PRUnichar ** aTitle) = 0;

 /**
   * Set the Title of the index
   */
  NS_IMETHOD SetTitleForIndex(PRInt32 aIndex, const PRUnichar * aTitle) = 0;

  /**
   * Get the History object of the index
   */
  NS_IMETHOD GetHistoryObjectForIndex(PRInt32 aIndex, nsISupports ** aState) = 0;


  /**
   * Set the History state of the index
   */
  NS_IMETHOD SetHistoryObjectForIndex(PRInt32 aIndex, nsISupports * aState) = 0;
};



extern "C" NS_APPSHELL nsresult
NS_NewSessionHistoryFactory(nsIFactory ** aFactory);


#endif /* nsISessionHistory_h_ */
