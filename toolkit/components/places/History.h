/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifndef mozilla_places_History_h_
#define mozilla_places_History_h_

#include "mozilla/IHistory.h"
#include "mozIAsyncHistory.h"
#include "Database.h"

#include "mozilla/dom/Link.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "nsURIHashKey.h"
#include "nsTObserverArray.h"
#include "nsDeque.h"
#include "nsIObserver.h"
#include "mozIStorageConnection.h"

namespace mozilla {
namespace places {

struct VisitData;

#define NS_HISTORYSERVICE_CID \
  {0x0937a705, 0x91a6, 0x417a, {0x82, 0x92, 0xb2, 0x2e, 0xb1, 0x0d, 0xa8, 0x6c}}

class History : public IHistory
              , public mozIAsyncHistory
              , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IHISTORY
  NS_DECL_MOZIASYNCHISTORY
  NS_DECL_NSIOBSERVER

  History();

  /**
   * Notifies about the visited status of a given URI.
   *
   * @param aURI
   *        The URI to notify about.
   */
  void NotifyVisited(nsIURI* aURI);

  /**
   * Obtains the statement to use to check if a URI is visited or not.
   */
  mozIStorageAsyncStatement* GetIsVisitedStatement();

  /**
   * Adds an entry in moz_places with the data in aVisitData.
   *
   * @param aVisitData
   *        The visit data to use to populate a new row in moz_places.
   */
  nsresult InsertPlace(const VisitData& aVisitData);

  /**
   * Updates an entry in moz_places with the data in aVisitData.
   *
   * @param aVisitData
   *        The visit data to use to update the existing row in moz_places.
   */
  nsresult UpdatePlace(const VisitData& aVisitData);

  /**
   * Loads information about the page into _place from moz_places.
   *
   * @param _place
   *        The VisitData for the place we need to know information about.
   * @return true if the page was recorded in moz_places, false otherwise.
   */
  bool FetchPageInfo(VisitData& _place);

  /**
   * Get the number of bytes of memory this History object is using,
   * including sizeof(*this))
   */
  PRInt64 SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf);

  /**
   * Obtains a pointer to this service.
   */
  static History* GetService();

  /**
   * Obtains a pointer that has had AddRef called on it.  Used by the service
   * manager only.
   */
  static History* GetSingleton();

  template<int N>
  already_AddRefed<mozIStorageStatement>
  GetStatement(const char (&aQuery)[N])
  {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_TRUE(dbConn, nsnull);
    return mDB->GetStatement(aQuery);
  }

private:
  virtual ~History();

  /**
   * Obtains a read-write database connection.
   */
  mozIStorageConnection* GetDBConn();

  /**
   * The database handle.  This is initialized lazily by the first call to
   * GetDBConn(), so never use it directly, or, if you really need, always
   * invoke GetDBConn() before.
   */
  nsRefPtr<mozilla::places::Database> mDB;

  /**
   * A read-only database connection used for checking if a URI is visited.
   *
   * @note this should only be accessed by GetIsVisistedStatement and Shutdown.
   */
  nsCOMPtr<mozIStorageConnection> mReadOnlyDBConn;

  /**
   * An asynchronous statement to query if a URI is visited or not.
   *
   * @note this should only be accessed by GetIsVisistedStatement and Shutdown.
   */
  nsCOMPtr<mozIStorageAsyncStatement> mIsVisitedStatement;

  /**
   * Remove any memory references to tasks and do not take on any more.
   */
  void Shutdown();

  static History* gService;

  // Ensures new tasks aren't started on destruction.
  bool mShuttingDown;

  typedef nsTObserverArray<mozilla::dom::Link* > ObserverArray;

  class KeyClass : public nsURIHashKey
  {
  public:
    KeyClass(const nsIURI* aURI)
    : nsURIHashKey(aURI)
    {
    }
    KeyClass(const KeyClass& aOther)
    : nsURIHashKey(aOther)
    {
      NS_NOTREACHED("Do not call me!");
    }
    ObserverArray array;
  };

  /**
   * Helper function for nsTHashtable::EnumerateEntries call in SizeOf().
   */
  static PLDHashOperator SizeOfEnumerator(KeyClass* aEntry, void* aArg);

  nsTHashtable<KeyClass> mObservers;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_History_h_
