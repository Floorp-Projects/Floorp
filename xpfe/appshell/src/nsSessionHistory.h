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

#ifndef __nsSessionHistory_h
#define __nsSessionHistory_h

#include "nsISessionHistory.h"
#include "nsVoidArray.h"
#include "nsAppShellCIDs.h"

// Advance declarations
class nsHistoryEntry;

class nsSessionHistory: public nsISessionHistory
{

public:
   nsSessionHistory();

   //nsISupports
   NS_DECL_ISUPPORTS

   NS_DEFINE_STATIC_CID_ACCESSOR( NS_SESSIONHISTORY_CID )

   NS_DECL_NSISESSIONHISTORY

protected:

   virtual ~nsSessionHistory();

private:
  PRInt32       mHistoryLength;
  PRInt32       mHistoryCurrentIndex;
  nsVoidArray   mHistoryEntries;

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

#endif
