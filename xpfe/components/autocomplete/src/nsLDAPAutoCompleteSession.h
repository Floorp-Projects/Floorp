/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIAutoCompleteSession.h"
#include "nsILDAPConnection.h"
#include "nsILDAPOperation.h"
#include "nsILDAPMessageListener.h"
#include "nsILDAPAutoCompleteSession.h"
#include "nsILDAPAutoCompFormatter.h"
#include "nsILDAPURL.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsIConsoleService.h"
#include "nsVoidArray.h"
#include "nsIAuthPrompt.h"

// 964665d0-1dd1-11b2-aeae-897834fb00b9
//
#define NS_LDAPAUTOCOMPLETESESSION_CID \
{ 0x964665d0, 0x1dd1, 0x11b2, \
 { 0xae, 0xae, 0x89, 0x78, 0x34, 0xfb, 0x00, 0xb9 }}

class nsLDAPAutoCompleteSession : public nsILDAPMessageListener, 
                                  public nsILDAPAutoCompleteSession
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTOCOMPLETESESSION
    NS_DECL_NSILDAPMESSAGELISTENER
    NS_DECL_NSILDAPAUTOCOMPLETESESSION

    nsLDAPAutoCompleteSession();
    virtual ~nsLDAPAutoCompleteSession();

  protected:
    enum SessionState { 
        UNBOUND = nsILDAPAutoCompFormatter::STATE_UNBOUND,
        INITIALIZING = nsILDAPAutoCompFormatter::STATE_INITIALIZING, 
        BINDING = nsILDAPAutoCompFormatter::STATE_BINDING, 
        BOUND = nsILDAPAutoCompFormatter::STATE_BOUND, 
        SEARCHING = nsILDAPAutoCompFormatter::STATE_SEARCHING 
    } mState;
    PRUint32 mEntriesReturned;                    // # of entries returned?
    nsCOMPtr<nsILDAPConnection> mConnection;      // connection used for search
    nsCOMPtr<nsILDAPOperation> mOperation;        // current ldap op
    nsCOMPtr<nsIAutoCompleteListener> mListener;  // callback 
    nsCOMPtr<nsIAutoCompleteResults> mResults;    // being built up
    nsCOMPtr<nsISupportsArray> mResultsArray;     // cached, to avoid re-gets
    nsString mSearchString;                       // autocomplete this string
    nsCString mFilterTemplate;                    // search filter template
    nsCOMPtr<nsILDAPURL> mServerURL;        // URL for the directory to search
    PRInt32 mMaxHits;                       // return at most this many entries
    PRUint32 mMinStringLength;              // strings < this size are ignored
    PRUint32 mCjkMinStringLength;           // ignore CJK strings < this size
    char **mSearchAttrs;        // outputFormat search attrs for SearchExt call
    PRUint32 mSearchAttrsSize;              // size of above array
    nsCOMPtr<nsIAuthPrompt> mAuthPrompter;  // used to prompt for the password
    PRUint32 mVersion;                      // version of LDAP to use

// XXX hack until nsUTF8String exists
#define nsUTF8String nsCString
    nsUTF8String mLogin;                       // authenticate as this user

    // used to format the ldap message into an nsIAutoCompleteItem
    //
    nsCOMPtr<nsILDAPAutoCompFormatter> mFormatter;

    // stopgap until nsLDAPService works
    nsresult InitConnection();             

    // check that we bound ok and start then call StartLDAPSearch
    nsresult OnLDAPBind(nsILDAPMessage *aMessage); 

    // add to the results set
    nsresult OnLDAPSearchEntry(nsILDAPMessage *aMessage); 

    // all done; call OnAutoComplete
    nsresult OnLDAPSearchResult(nsILDAPMessage *aMessage); 

    // kick off a search
    nsresult StartLDAPSearch();

    // check if the LDAP message received is current
    nsresult IsMessageCurrent(nsILDAPMessage *aMessage, PRBool *aIsCurrent);

    // finish a search by calling mListener->OnAutoComplete, resetting state,
    // and freeing resources.  if aACStatus == 
    // nsIAutoCompleteStatus::failureItems, then the formatter is called with
    // aResult and aEndState to create an autocomplete item with the error
    // info in it.  See nsILDAPAutoCompFormatter.idl for more info on this.
    void FinishAutoCompleteLookup(AutoCompleteStatus aACStatus, 
                                  const nsresult aResult,
                                  enum SessionState aEndState);

    // create and initialize the results array
    nsresult CreateResultsArray(void);

};

