/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsSHEntry_h
#define nsSHEntry_h

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"

// Interfaces needed
#include "nsIDOMDocument.h"
#include "nsIInputStream.h"
#include "nsILayoutHistoryState.h"
#include "nsISHEntry.h"
#include "nsISHContainer.h"
#include "nsIURI.h"
#include "nsIEnumerator.h"
#include "nsIHistoryEntry.h"

class nsSHEntry : public nsIHistoryEntry,
                  public nsISHEntry,
                  public nsISHContainer
{
public: 
   nsSHEntry();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIHISTORYENTRY
  NS_DECL_NSISHENTRY
  NS_DECL_NSISHCONTAINER

protected:
   virtual ~nsSHEntry();
private:
   
	 
  nsCOMPtr<nsIURI>                mURI;
  nsCOMPtr<nsIURI>                mReferrerURI;
	nsCOMPtr<nsIDOMDocument>        mDocument;
	nsString                        mTitle;
	nsCOMPtr<nsIInputStream>        mPostData;
	nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
	nsVoidArray                     mChildren;
	PRUint32                        mLoadType;  
	PRUint32                        mID;
  PRBool                          mIsFrameNavigation;
  PRBool                          mSaveLayoutState;
  PRPackedBool                    mExpired;
  nsCOMPtr<nsISupports>           mCacheKey;
  nsISHEntry *                    mParent;  // weak reference
};


#endif /* nsSHEntry_h */
