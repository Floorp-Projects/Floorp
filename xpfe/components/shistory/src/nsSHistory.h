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

#ifndef nsSHistory_h
#define nsSHistory_h

// Helper Classes
#include "nsCOMPtr.h"

//Interfaces Needed
#include "nsISHistory.h"
#include "nsISHTransaction.h"
#include "nsIWebNavigation.h"

class nsIDocShell;

class nsSHistory: public nsISHistory,
                  public nsIWebNavigation
{
public:
	nsSHistory();

	NS_DECL_ISUPPORTS
	NS_DECL_NSISHISTORY
	NS_DECL_NSIWEBNAVIGATION

protected:
   virtual ~nsSHistory();

   // Could become part of nsIWebNavigation
   NS_IMETHOD PrintHistory();
   NS_IMETHOD GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult);
   PRBool CompareSHEntry(nsISHEntry * prevEntry, nsISHEntry * nextEntry, nsIDocShell * rootDocShell,
	                     nsIDocShell ** aResultDocShell, nsISHEntry ** aResultSHEntry);
   NS_IMETHOD LoadEntry(PRInt32 aIndex, PRBool aReloadFlag, long aLoadType);
	
protected:
   nsCOMPtr<nsISHTransaction> mListRoot;
	PRInt32 mIndex;
	PRInt32 mLength;
	// Weak reference. Do not refcount this.
	nsIDocShell *  mRootDocShell;
};


#endif   /* nsSHistory */
