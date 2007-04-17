/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __nsFormHistory__
#define __nsFormHistory__

#include "nsIFormHistory.h"
#include "nsIFormSubmitObserver.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"

#include "nsServiceManagerUtils.h"
#include "nsToolkitCompsCID.h"
#ifdef MOZ_MORKREADER
#include "nsMorkReader.h"
#endif

class nsIAutoCompleteSimpleResult;
class nsIAutoCompleteResult;
class nsFormHistory;
template <class E> class nsTArray;

#define NS_IFORMHISTORYPRIVATE_IID \
{0xc4a47315, 0xaeb5, 0x4039, {0x9f, 0x34, 0x45, 0x11, 0xb3, 0xa7, 0x58, 0xdd}}

class nsIFormHistoryPrivate : public nsISupports
{
 public:
#ifdef MOZILLA_1_8_BRANCH
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORMHISTORYPRIVATE_IID)
#else
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORMHISTORYPRIVATE_IID)
#endif

  mozIStorageConnection* GetStorageConnection() { return mDBConn; }

 protected:
  nsCOMPtr<mozIStorageConnection> mDBConn;
};

#ifndef MOZILLA_1_8_BRANCH
NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormHistoryPrivate, NS_IFORMHISTORYPRIVATE_IID)
#endif

class nsFormHistory : public nsIFormHistory2,
                      public nsIFormHistoryPrivate,
                      public nsIObserver,
                      public nsIFormSubmitObserver,
                      public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFORMHISTORY2
  NS_DECL_NSIOBSERVER
  
  // nsIFormSubmitObserver
  NS_IMETHOD Notify(nsIDOMHTMLFormElement* formElt, nsIDOMWindowInternal* window, nsIURI* actionURL, PRBool* cancelSubmit);

  nsFormHistory();
  nsresult Init();

  static nsFormHistory* GetInstance()
    {
      if (!gFormHistory) {
        nsCOMPtr<nsIFormHistory2> fh = do_GetService(NS_FORMHISTORY_CONTRACTID);
      }
      return gFormHistory;
    }

  nsresult AutoCompleteSearch(const nsAString &aInputName,
			      const nsAString &aInputValue,
                              nsIAutoCompleteSimpleResult *aPrevResult,
			      nsIAutoCompleteResult **aNewResult);

 private:
  ~nsFormHistory();

 protected:
  // Database I/O
  nsresult OpenDatabase();
  nsresult CloseDatabase();
  nsresult GetDatabaseFile(nsIFile** aFile);

  static PRBool FormHistoryEnabled();
  static nsFormHistory *gFormHistory;
  static PRBool gFormHistoryEnabled;
  static PRBool gPrefsInitialized;

  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsCOMPtr<mozIStorageService> mStorageService;
  nsCOMPtr<mozIStorageStatement> mDBGetMatchingField;
  nsCOMPtr<mozIStorageStatement> mDBFindEntry;
  nsCOMPtr<mozIStorageStatement> mDBFindEntryByName;
  nsCOMPtr<mozIStorageStatement> mDBSelectEntries;
  nsCOMPtr<mozIStorageStatement> mDBInsertNameValue;

  // dummy statement (see StartCache)
  nsresult StartCache();
  nsresult StopCache();
  nsCOMPtr<mozIStorageConnection> mDummyConnection;
  nsCOMPtr<mozIStorageStatement> mDummyStatement;
};

#ifdef MOZ_MORKREADER
class nsFormHistoryImporter : public nsIFormHistoryImporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFORMHISTORYIMPORTER

private:
  // Enumerator callback to add a single row to the FormHistory.
  static PLDHashOperator PR_CALLBACK
  AddToFormHistoryCB(const nsCSubstring &aRowID,
                     const nsTArray<nsCString> *aValues,
                     void *aData);
};
#endif

#endif // __nsFormHistory__
