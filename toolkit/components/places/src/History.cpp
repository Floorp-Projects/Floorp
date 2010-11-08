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
 * Mozilla Foundation.
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

#ifdef MOZ_IPC
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsXULAppAPI.h"
#endif

#ifdef MOZ_IPC
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
#endif

#include "History.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "Helpers.h"

#include "mozilla/storage.h"
#include "mozilla/dom/Link.h"
#include "nsDocShellCID.h"
#include "nsIEventStateManager.h"
#include "mozilla/Services.h"

using namespace mozilla::dom;

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// Global Defines

#define URI_VISITED "visited"
#define URI_NOT_VISITED "not visited"
#define URI_VISITED_RESOLUTION_TOPIC "visited-status-resolution"
// Observer event fired after a visit has been registered in the DB.
#define URI_VISIT_SAVED "uri-visit-saved"

////////////////////////////////////////////////////////////////////////////////
//// Step

class Step : public AsyncStatementCallback
{
public:
  /**
   * Executes statement asynchronously using this as a callback.
   * 
   * @param aStmt
   *        Statement to execute asynchronously
   */
  NS_IMETHOD ExecuteAsync(mozIStorageStatement* aStmt);

  /**
   * Called once after query is completed.  If your query has more than one
   * result set to process, you will want to override HandleResult to process
   * each one.
   *
   * @param aResultSet
   *        Results from ExecuteAsync
   *        Unlike HandleResult, this *can be NULL* if there were no results.
   */
  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet);

  /**
   * By default, stores the last result set received in mResultSet.
   * For queries with only one result set, you don't need to override.
   *
   * @param aResultSet
   *        Results from ExecuteAsync
   */
  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet);

  /**
   * By default, this calls Callback with any saved results from HandleResult.
   * For queries with only one result set, you don't need to override.
   *
   * @param aReason
   *        SQL status code
   */
  NS_IMETHOD HandleCompletion(PRUint16 aReason);

private:
  // Used by HandleResult to cache results until HandleCompletion is called.
  nsCOMPtr<mozIStorageResultSet> mResultSet;
};

NS_IMETHODIMP
Step::ExecuteAsync(mozIStorageStatement* aStmt)
{
  nsCOMPtr<mozIStoragePendingStatement> handle;
  nsresult rv = aStmt->ExecuteAsync(this, getter_AddRefs(handle));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
Step::Callback(mozIStorageResultSet* aResultSet)
{
  return NS_OK;
}

NS_IMETHODIMP
Step::HandleResult(mozIStorageResultSet* aResultSet)
{
  mResultSet = aResultSet;
  return NS_OK;
}

NS_IMETHODIMP
Step::HandleCompletion(PRUint16 aReason)
{
  if (aReason == mozIStorageStatementCallback::REASON_FINISHED) {
    nsCOMPtr<mozIStorageResultSet> resultSet = mResultSet;
    mResultSet = NULL;
    Callback(resultSet);
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// Anonymous Helpers

namespace {

class VisitedQuery : public AsyncStatementCallback
{
public:
  static nsresult Start(nsIURI* aURI)
  {
    NS_PRECONDITION(aURI, "Null URI");

#ifdef MOZ_IPC
  // If we are a content process, always remote the request to the
  // parent process.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentChild* cpc =
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendStartVisitedQuery(aURI);
    return NS_OK;
  }
#endif

    mozIStorageAsyncStatement* stmt =
      History::GetService()->GetIsVisitedStatement();
    NS_ENSURE_STATE(stmt);

    // Bind by index for performance.
    nsresult rv = URIBinder::Bind(stmt, 0, aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<VisitedQuery> callback = new VisitedQuery(aURI);
    NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<mozIStoragePendingStatement> handle;
    return stmt->ExecuteAsync(callback, getter_AddRefs(handle));
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResults)
  {
    // If this method is called, we've gotten results, which means we have a
    // visit.
    mIsVisited = true;
    return NS_OK;
  }

  NS_IMETHOD HandleError(mozIStorageError* aError)
  {
    // mIsVisited is already set to false, and that's the assumption we will
    // make if an error occurred.
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    if (aReason != mozIStorageStatementCallback::REASON_FINISHED) {
      return NS_OK;
    }

    if (mIsVisited) {
      History::GetService()->NotifyVisited(mURI);
    }

    // Notify any observers about that we have resolved the visited state of
    // this URI.
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      nsAutoString status;
      if (mIsVisited) {
        status.AssignLiteral(URI_VISITED);
      }
      else {
        status.AssignLiteral(URI_NOT_VISITED);
      }
      (void)observerService->NotifyObservers(mURI,
                                             URI_VISITED_RESOLUTION_TOPIC,
                                             status.get());
    }

    return NS_OK;
  }
private:
  VisitedQuery(nsIURI* aURI)
  : mURI(aURI)
  , mIsVisited(false)
  {
  }

  nsCOMPtr<nsIURI> mURI;
  bool mIsVisited;
};

struct VisitData {
  VisitData()
  : placeId(0)
  , visitId(0)
  , sessionId(0)
  , hidden(false)
  , typed(false)
  , transitionType(-1)
  , visitTime(0)
  {
  }

  /**
   * Copy constructor that is designed to not call AddRef/Release on the URI
   * objects, so they do not change their refcount on the wrong thread.
   */
  VisitData(VisitData& aOther)
  : placeId(aOther.placeId)
  , visitId(aOther.visitId)
  , sessionId(aOther.sessionId)
  , spec(aOther.spec)
  , hidden(aOther.hidden)
  , typed(aOther.typed)
  , transitionType(aOther.transitionType)
  , visitTime(aOther.visitTime)
  {
    uri.swap(aOther.uri);
  }

  PRInt64 placeId;
  PRInt64 visitId;
  PRInt64 sessionId;
  nsCString spec;
  nsCOMPtr<nsIURI> uri;
  bool hidden;
  bool typed;
  PRInt32 transitionType;
  PRTime visitTime;
};


/**
 * Notifies observers about a visit.
 */
class NotifyVisitObservers : public nsRunnable
{
public:
  NotifyVisitObservers(VisitData& aPlace,
                       VisitData& aReferrer)
  : mPlace(aPlace)
  , mReferrer(aReferrer)
  {
  }

  NS_IMETHOD Run()
  {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    if (!history) {
      NS_WARNING("Trying to notify about a visit but cannot get the history service!");
      return NS_OK;
    }

    // Notify nsNavHistory observers of visit, but only for certain types of
    // visits to maintain consistency with nsNavHistory::GetQueryResults.
    if (!mPlace.hidden &&
        mPlace.transitionType != nsINavHistoryService::TRANSITION_EMBED &&
        mPlace.transitionType != nsINavHistoryService::TRANSITION_FRAMED_LINK) {
      history->NotifyOnVisit(mPlace.uri, mPlace.visitId, mPlace.visitTime,
                             mPlace.sessionId, mReferrer.visitId,
                             mPlace.transitionType);
    }

    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
    if (obsService) {
      nsresult rv = obsService->NotifyObservers(mPlace.uri, URI_VISIT_SAVED,
                                                nsnull);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Could not notify observers");
    }

    History::GetService()->NotifyVisited(mPlace.uri);
    return NS_OK;
  }
private:
  const VisitData mPlace;
  const VisitData mReferrer;
};

/**
 * Adds a visit to the database.
 */
class InsertVisitedURI : public nsRunnable
{
public:
  /**
   * Adds a visit to the database asynchronously.
   *
   * @param aConnection
   *        The database connection to use for these operations.
   * @param aPlace
   *        The location to record a visit.
   * @param [optional] aReferrer
   *        The page that "referred" us to aPlace.
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        VisitData& aPlace,
                        nsIURI* aReferrer = nsnull)
  {
    NS_PRECONDITION(NS_IsMainThread(),
                    "This should be called on the main thread");

    nsRefPtr<InsertVisitedURI> event =
      new InsertVisitedURI(aConnection, aPlace, aReferrer);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

    // Speculatively get a new session id for our visit.  While it is true that
    // we will use the session id from the referrer if the visit was "recent"
    // enough, we cannot call this method off of the main thread, so we have to
    // consume an id now.
    nsNavHistory* navhistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(navhistory, NS_ERROR_UNEXPECTED);
    event->mPlace.sessionId = navhistory->GetNewSessionID();

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    NS_PRECONDITION(!NS_IsMainThread(),
                    "This should not be called on the main thread");

    mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);
    bool known = FetchPageInfo(mPlace);

    // If the page was in moz_places, we need to update the entry.
    if (known) {
      NS_ASSERTION(mPlace.placeId > 0, "must have a valid place id!");

      nsCOMPtr<mozIStorageStatement> stmt =
        mHistory->syncStatements.GetCachedStatement(
          "UPDATE moz_places "
          "SET hidden = :hidden, typed = :typed "
          "WHERE id = :page_id "
        );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"),
                                          mPlace.typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), mPlace.hidden);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Otherwise, the page was not in moz_places, so now we have to add it.
    else {
      NS_ASSERTION(mPlace.placeId == 0, "should not have a valid place id!");

      nsCOMPtr<mozIStorageStatement> stmt =
        mHistory->syncStatements.GetCachedStatement(
          "INSERT INTO moz_places "
            "(url, rev_host, hidden, typed) "
          "VALUES (:page_url, :rev_host, :hidden, :typed) "
        );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      nsAutoString revHost;
      nsresult rv = GetReversedHostname(mPlace.uri, revHost);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mPlace.uri);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("rev_host"), revHost);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), mPlace.typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), mPlace.hidden);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      // Now, we need to get the id of what we just added.
      bool added = FetchPageInfo(mPlace);
      NS_ASSERTION(added, "not known after adding the place!");
    }

    // If we had a referrer, we want to know about its last visit to put this
    // new visit into the same session.
    if (mReferrer.uri) {
      bool recentVisit = FetchVisitInfo(mReferrer, mPlace.visitTime);
      // At this point, we know the referrer's session id, which this new visit
      // should also share.
      if (recentVisit) {
        mPlace.sessionId = mReferrer.sessionId;
      }
      // However, if it isn't recent enough, we don't care to log anything about
      // the referrer and we'll start a new session.
      else {
        // This is sufficient to ignore our referrer.  This behavior has test
        // coverage, so if this invariant changes, we'll know.
        mReferrer.visitId = 0;
      }
    }

    nsresult rv = AddVisit(mPlace, mReferrer);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = UpdateFrecency(mPlace);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, dispatch an event to the main thread to notify observers.
    nsCOMPtr<nsIRunnable> event = new NotifyVisitObservers(mPlace, mReferrer);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    rv = NS_DispatchToMainThread(event);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
private:
  InsertVisitedURI(mozIStorageConnection* aConnection,
                   VisitData& aPlace,
                   nsIURI* aReferrer)
  : mDBConn(aConnection)
  , mPlace(aPlace)
  , mHistory(History::GetService())
  {
    mReferrer.uri = aReferrer;
  }

  /**
   * Loads information about the page into _place from moz_places.
   *
   * @param _place
   *        The VisitData for the place we need to know information about.
   * @return true if the page was recorded in moz_places, false otherwise.
   */
  bool FetchPageInfo(VisitData& _place)
  {
    NS_PRECONDITION(_place.uri || _place.spec.Length(),
                    "must have a non-null uri or a non-empty spec!");

    nsCOMPtr<mozIStorageStatement> stmt =
      mHistory->syncStatements.GetCachedStatement(
        "SELECT id, typed, hidden "
        "FROM moz_places "
        "WHERE url = :page_url "
      );
    NS_ENSURE_TRUE(stmt, false);
    mozStorageStatementScoper scoper(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                                  _place.uri);
    NS_ENSURE_SUCCESS(rv, false);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, false);
    if (!hasResult) {
      return false;
    }

    rv = stmt->GetInt64(0, &_place.placeId);
    NS_ENSURE_SUCCESS(rv, false);

    if (!_place.typed) {
      // If this transition wasn't typed, others might have been. If database
      // has location as typed, reflect that in our data structure.
      PRInt32 typed;
      rv = stmt->GetInt32(1, &typed);
      _place.typed = !!typed;
      NS_ENSURE_SUCCESS(rv, true);
    }
    if (_place.hidden) {
      // If this transition was hidden, it is possible that others were not.
      // Any one visible transition makes this location visible. If database
      // has location as visible, reflect that in our data structure.
      PRInt32 hidden;
      rv = stmt->GetInt32(2, &hidden);
      _place.hidden = !!hidden;
      NS_ENSURE_SUCCESS(rv, true);
    }

    return true;
  }

  /**
   * Loads visit information about the page into _place.
   *
   * @param _place
   *        The VisitData for the place we need to know visit information about.
   * @param [optional] aThresholdStart
   *        The timestamp of a new visit (not represented by _place) used to
   *        determine if the page was recently visited or not.
   * @return true if the page was recently (determined with aThresholdStart)
   *         visited, false otherwise.
   */
  bool FetchVisitInfo(VisitData& _place,
                      PRTime aThresholdStart = 0)
  {
    NS_PRECONDITION(_place.uri || _place.spec.Length(),
                    "must have a non-null uri or a non-empty spec!");

    nsCOMPtr<mozIStorageStatement> stmt =
      mHistory->syncStatements.GetCachedStatement(
        "SELECT id, session, visit_date "
        "FROM moz_historyvisits "
        "WHERE place_id = (SELECT id FROM moz_places WHERE url = :page_url) "
        "ORDER BY visit_date DESC "
      );
    NS_ENSURE_TRUE(stmt, false);
    mozStorageStatementScoper scoper(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                                  _place.uri);
    NS_ENSURE_SUCCESS(rv, false);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, false);
    if (!hasResult) {
      return false;
    }

    rv = stmt->GetInt64(0, &_place.visitId);
    NS_ENSURE_SUCCESS(rv, false);
    rv = stmt->GetInt64(1, &_place.sessionId);
    NS_ENSURE_SUCCESS(rv, false);
    rv = stmt->GetInt64(2, &_place.visitTime);
    NS_ENSURE_SUCCESS(rv, false);

    // If we have been given a visit threshold start time, go ahead and
    // calculate if we have been recently visited.
    if (aThresholdStart &&
        aThresholdStart - _place.visitTime <= RECENT_EVENT_THRESHOLD) {
      return true;
    }

    return false;
  }

  /**
   * Adds a visit for _place and updates it with the right visit id.
   *
   * @param _place
   *        The VisitData for the place we need to know visit information about.
   * @param aReferrer
   *        A reference to the referrer's visit data.
   */
  nsresult AddVisit(VisitData& _place,
                    const VisitData& aReferrer)
  {
    nsCOMPtr<mozIStorageStatement> stmt =
      mHistory->syncStatements.GetCachedStatement(
        "INSERT INTO moz_historyvisits "
          "(from_visit, place_id, visit_date, visit_type, session) "
        "VALUES (:from_visit, :page_id, :visit_date, :visit_type, :session) "
      );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("from_visit"),
                                        aReferrer.visitId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                               _place.placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"),
                               _place.visitTime);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 transitionType = _place.transitionType;
    NS_ASSERTION(transitionType >= nsINavHistoryService::TRANSITION_LINK &&
                 transitionType <= nsINavHistoryService::TRANSITION_FRAMED_LINK,
                 "Invalid transition type!");
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"),
                               transitionType);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("session"),
                               _place.sessionId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // Now that it should be in the database, we need to obtain the id of the
    // visit we just added.
    bool visited = FetchVisitInfo(_place);
    NS_ASSERTION(!visited, "Not visited after adding a visit!");

    return NS_OK;
  }

  /**
   * Updates the frecency, and possibly the hidden-ness of aPlace.
   *
   * @param aPlace
   *        The VisitData for the place we want to update.
   */
  nsresult UpdateFrecency(const VisitData& aPlace)
  {
    { // First, set our frecency to the proper value.
      nsCOMPtr<mozIStorageStatement> stmt =
        mHistory->syncStatements.GetCachedStatement(
          "UPDATE moz_places "
          "SET frecency = CALCULATE_FRECENCY(:page_id) "
          "WHERE id = :page_id"
        );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                                          aPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    { // Now, we need to mark the page as not hidden if the frecency is now
      // nonzero.
      nsCOMPtr<mozIStorageStatement> stmt =
        mHistory->syncStatements.GetCachedStatement(
          "UPDATE moz_places "
          "SET hidden = 0 "
          "WHERE id = :page_id AND frecency <> 0"
        );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                                          aPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  mozIStorageConnection* mDBConn;

  VisitData mPlace;
  VisitData mReferrer;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  nsRefPtr<History> mHistory;
};

/**
 * Fail-safe mechanism for ensuring that your task completes, no matter what.
 * Pass this around as an nsAutoPtr in your steps to guarantee that when all
 * your steps are finished, your task is finished.
 *
 * Be sure to use AppendTask to add your first step to the queue.
 */
class FailSafeFinishTask
{
public:
  FailSafeFinishTask()
  : mAppended(false)
  {
  }

  ~FailSafeFinishTask()
  {
    if (mAppended) {
      History::GetService()->CurrentTaskFinished();
    }
  }

  /**
   * Appends task to History's queue.  When this object is destroyed, it will
   * consider the task finished.
   */
  void AppendTask(Step* step)
  {
    History::GetService()->AppendTask(step);
    mAppended = true;
  }

private:
  bool mAppended;
};
////////////////////////////////////////////////////////////////////////////////
//// Steps for SetURITitle

struct SetTitleData : public FailSafeFinishTask
{
  nsCOMPtr<nsIURI> uri;
  nsString title;
};

/**
 * Step 3: Notify that title has been updated.
 */
class TitleNotifyStep: public Step
{
public:
  TitleNotifyStep(nsAutoPtr<SetTitleData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    history->NotifyTitleChange(mData->uri, mData->title);

    return NS_OK;
  }

protected:
  nsAutoPtr<SetTitleData> mData;
};

/**
 * Step 2: Set title.
 */
class SetTitleStep : public Step
{
public:
  SetTitleStep(nsAutoPtr<SetTitleData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    if (!aResultSet) {
      // URI record was not found.
      return NS_OK;
    }

    nsCOMPtr<mozIStorageRow> row;
    nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString title;
    rv = row->GetString(2, title);
    NS_ENSURE_SUCCESS(rv, rv);

    // It is actually common to set the title to be the same thing it used to
    // be. For example, going to any web page will always cause a title to be set,
    // even though it will often be unchanged since the last visit. In these
    // cases, we can avoid DB writing and observer overhead.
    if (mData->title.Equals(title) || (mData->title.IsVoid() && title.IsVoid()))
      return NS_OK;

    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<mozIStorageStatement> stmt =
      history->GetStatementById(DB_SET_PLACE_TITLE);
    NS_ENSURE_STATE(stmt);

    if (mData->title.IsVoid()) {
      rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_title"));
    }
    else {
      rv = stmt->BindStringByName(
        NS_LITERAL_CSTRING("page_title"),
        StringHead(mData->title, TITLE_LENGTH_MAX)
      );
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new TitleNotifyStep(mData);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<SetTitleData> mData;
};

/**
 * Step 1: See if there is an existing URI.
 */
class StartSetURITitleStep : public Step
{
public:
  StartSetURITitleStep(nsAutoPtr<SetTitleData> aData)
  : mData(aData)
  {
    mData->AppendTask(this);
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

    // Find existing entry in moz_places table, if any.
    nsCOMPtr<mozIStorageStatement> stmt =
      history->GetStatementById(DB_GET_URL_PAGE_INFO);
    NS_ENSURE_STATE(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new SetTitleStep(mData);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<SetTitleData> mData;
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// History

History* History::gService = NULL;

History::History()
: mShuttingDown(false)
, syncStatements(mDBConn)
{
  NS_ASSERTION(!gService, "Ruh-roh!  This service has already been created!");
  gService = this;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_WARN_IF_FALSE(os, "Observer service was not found!");
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, PR_FALSE);
  }
}

History::~History()
{
  gService = NULL;

#ifdef DEBUG
  if (mObservers.IsInitialized()) {
    NS_ASSERTION(mObservers.Count() == 0,
                 "Not all Links were removed before we disappear!");
  }
#endif

  // Places shutdown event may not occur, but we *must* clean up before History
  // goes away.
  Shutdown();
}

void
History::AppendTask(Step* aTask)
{
  NS_PRECONDITION(aTask, "Got NULL task.");

  if (mShuttingDown) {
    return;
  }

  NS_ADDREF(aTask);
  mPendingVisits.Push(aTask);

  if (mPendingVisits.GetSize() == 1) {
    // There are no other pending tasks.
    StartNextTask();
  }
}

void
History::CurrentTaskFinished()
{
  if (mShuttingDown) {
    return;
  }

  NS_ASSERTION(mPendingVisits.PeekFront(), "Tried to finish task not on the queue");

  nsCOMPtr<Step> deadTaskWalking =
    dont_AddRef(static_cast<Step*>(mPendingVisits.PopFront()));
  StartNextTask();
}

void
History::NotifyVisited(nsIURI* aURI)
{
  NS_ASSERTION(aURI, "Ruh-roh!  A NULL URI was passed to us!");

#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    mozilla::dom::ContentParent* cpp = 
      mozilla::dom::ContentParent::GetSingleton(PR_FALSE);
    if (cpp)
      (void)cpp->SendNotifyVisited(aURI);
  }
#endif

  // If the hash table has not been initialized, then we have nothing to notify
  // about.
  if (!mObservers.IsInitialized()) {
    return;
  }

  // Additionally, if we have no observers for this URI, we have nothing to
  // notify about.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    return;
  }

  // Walk through the array, and update each Link node.
  const ObserverArray& observers = key->array;
  ObserverArray::index_type len = observers.Length();
  for (ObserverArray::index_type i = 0; i < len; i++) {
    Link* link = observers[i];
    link->SetLinkState(eLinkState_Visited);
    NS_ASSERTION(len == observers.Length(),
                 "Calling SetLinkState added or removed an observer!");
  }

  // All the registered nodes can now be removed for this URI.
  mObservers.RemoveEntry(aURI);
}

mozIStorageAsyncStatement*
History::GetIsVisitedStatement()
{
  if (mIsVisitedStatement) {
    return mIsVisitedStatement;
  }

  // If we don't yet have a database connection, go ahead and clone it now.
  if (!mReadOnlyDBConn) {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_TRUE(dbConn, nsnull);

    (void)dbConn->Clone(PR_TRUE, getter_AddRefs(mReadOnlyDBConn));
    NS_ENSURE_TRUE(mReadOnlyDBConn, nsnull);
  }

  // Now we can create our cached statement.
  nsresult rv = mReadOnlyDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT h.id "
    "FROM moz_places h "
    "WHERE url = ?1 "
      "AND EXISTS(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
  ),  getter_AddRefs(mIsVisitedStatement));
  NS_ENSURE_SUCCESS(rv, nsnull);
  return mIsVisitedStatement;
}

/* static */
History*
History::GetService()
{
  if (gService) {
    return gService;
  }

  nsCOMPtr<IHistory> service(do_GetService(NS_IHISTORY_CONTRACTID));
  NS_ABORT_IF_FALSE(service, "Cannot obtain IHistory service!");
  NS_ASSERTION(gService, "Our constructor was not run?!");

  return gService;
}

/* static */
History*
History::GetSingleton()
{
  if (!gService) {
    gService = new History();
    NS_ENSURE_TRUE(gService, nsnull);
  }

  NS_ADDREF(gService);
  return gService;
}

mozIStorageConnection*
History::GetDBConn()
{
  if (mDBConn) {
    return mDBConn;
  }

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, nsnull);

  nsresult rv = history->GetDBConnection(getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBConn;
}

void
History::StartNextTask()
{
  if (mShuttingDown) {
    return;
  }

  nsCOMPtr<Step> nextTask =
    static_cast<Step*>(mPendingVisits.PeekFront());
  if (!nextTask) {
    // No more pending visits left to process.
    return;
  }
  nsresult rv = nextTask->Callback(NULL);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Beginning a task failed.");
}

void
History::Shutdown()
{
  mShuttingDown = true;

  while (mPendingVisits.PeekFront()) {
    nsCOMPtr<Step> deadTaskWalking =
      dont_AddRef(static_cast<Step*>(mPendingVisits.PopFront()));
  }

  // Clean up our statements and connection.
  syncStatements.FinalizeStatements();

  if (mReadOnlyDBConn) {
    if (mIsVisitedStatement) {
      (void)mIsVisitedStatement->Finalize();
    }
    (void)mReadOnlyDBConn->AsyncClose(nsnull);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// IHistory

NS_IMETHODIMP
History::VisitURI(nsIURI* aURI,
                  nsIURI* aLastVisitedURI,
                  PRUint32 aFlags)
{
  NS_PRECONDITION(aURI, "URI should not be NULL.");
  if (mShuttingDown) {
    return NS_OK;
  }

#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentChild* cpc =
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendVisitURI(aURI, aLastVisitedURI, aFlags);
    return NS_OK;
  } 
#endif /* MOZ_IPC */

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  // Silently return if URI is something we shouldn't add to DB.
  PRBool canAdd;
  nsresult rv = history->CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  if (aLastVisitedURI) {
    PRBool same;
    rv = aURI->Equals(aLastVisitedURI, &same);
    NS_ENSURE_SUCCESS(rv, rv);
    if (same) {
      // Do not save refresh-page visits.
      return NS_OK;
    }
  }

  VisitData place;
  rv = aURI->GetSpec(place.spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Assigns a type to the edge in the visit linked list. Each type will be
  // considered differently when weighting the frecency of a location.
  PRUint32 recentFlags = history->GetRecentFlags(aURI);
  bool redirected = false;
  if (aFlags & IHistory::REDIRECT_TEMPORARY) {
    place.transitionType = nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY;
    redirected = true;
  }
  else if (aFlags & IHistory::REDIRECT_PERMANENT) {
    place.transitionType = nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT;
    redirected = true;
  }
  else if (recentFlags & nsNavHistory::RECENT_TYPED) {
    place.transitionType = nsINavHistoryService::TRANSITION_TYPED;
  }
  else if (recentFlags & nsNavHistory::RECENT_BOOKMARKED) {
    place.transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;
  }
  else if (aFlags & IHistory::TOP_LEVEL) {
    // User was redirected or link was clicked in the main window.
    place.transitionType = nsINavHistoryService::TRANSITION_LINK;
  }
  else if (recentFlags & nsNavHistory::RECENT_ACTIVATED) {
    // User activated a link in a frame.
    place.transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
  }
  else {
    // A frame redirected to a new site without user interaction.
    place.transitionType = nsINavHistoryService::TRANSITION_EMBED;
  }

  place.typed = place.transitionType == nsINavHistoryService::TRANSITION_TYPED;
  place.hidden =
    place.transitionType == nsINavHistoryService::TRANSITION_FRAMED_LINK ||
    place.transitionType == nsINavHistoryService::TRANSITION_EMBED ||
    redirected;
  place.visitTime = PR_Now();
  place.uri = aURI;

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  rv = InsertVisitedURI::Start(dbConn, place, aLastVisitedURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, notify that we've been visited.
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::RegisterVisitedCallback(nsIURI* aURI,
                                 Link* aLink)
{
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    NS_PRECONDITION(aLink, "Must pass a non-null Link!");
  }
#else
  NS_PRECONDITION(aLink, "Must pass a non-null Link!");
#endif

  // First, ensure that our hash table is setup.
  if (!mObservers.IsInitialized()) {
    NS_ENSURE_TRUE(mObservers.Init(), NS_ERROR_OUT_OF_MEMORY);
  }

  // Obtain our array of observers for this URI.
#ifdef DEBUG
  bool keyAlreadyExists = !!mObservers.GetEntry(aURI);
#endif
  KeyClass* key = mObservers.PutEntry(aURI);
  NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
  ObserverArray& observers = key->array;

  if (observers.IsEmpty()) {
    NS_ASSERTION(!keyAlreadyExists,
                 "An empty key was kept around in our hashtable!");

    // We are the first Link node to ask about this URI, or there are no pending
    // Links wanting to know about this URI.  Therefore, we should query the
    // database now.
    nsresult rv = VisitedQuery::Start(aURI);

    // In IPC builds, we are passed a NULL Link from
    // ContentParent::RecvStartVisitedQuery.  Since we won't be adding a NULL
    // entry to our list of observers, and the code after this point assumes
    // that aLink is non-NULL, we will need to return now.
    if (NS_FAILED(rv) || !aLink) {
      // Remove our array from the hashtable so we don't keep it around.
      mObservers.RemoveEntry(aURI);
      return rv;
    }
  }
#ifdef MOZ_IPC
  // In IPC builds, we are passed a NULL Link from
  // ContentParent::RecvStartVisitedQuery.  All of our code after this point
  // assumes aLink is non-NULL, so we have to return now.
  else if (!aLink) {
    NS_ASSERTION(XRE_GetProcessType() == GeckoProcessType_Default,
                 "We should only ever get a null Link in the default process!");
    return NS_OK;
  }
#endif

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  NS_ASSERTION(!observers.Contains(aLink),
               "Already tracking this Link object!");

  // Start tracking our Link.
  if (!observers.AppendElement(aLink)) {
    // Curses - unregister and return failure.
    (void)UnregisterVisitedCallback(aURI, aLink);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
History::UnregisterVisitedCallback(nsIURI* aURI,
                                   Link* aLink)
{
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  NS_ASSERTION(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    NS_ERROR("Trying to unregister for a URI that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }
  ObserverArray& observers = key->array;
  if (!observers.RemoveElement(aLink)) {
    NS_ERROR("Trying to unregister a node that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }

  // If the array is now empty, we should remove it from the hashtable.
  if (observers.IsEmpty()) {
    mObservers.RemoveEntry(aURI);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::SetURITitle(nsIURI* aURI, const nsAString& aTitle)
{
  NS_PRECONDITION(aURI, "Must pass a non-null URI!");
  if (mShuttingDown) {
    return NS_OK;
  }

#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentChild * cpc = 
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendSetURITitle(aURI, nsDependentString(aTitle));
    return NS_OK;
  } 
#endif /* MOZ_IPC */

  nsNavHistory* history = nsNavHistory::GetHistoryService();

  // At first, it seems like nav history should always be available here, no
  // matter what.
  //
  // nsNavHistory fails to register as a service if there is no profile in
  // place (for instance, if user is choosing a profile).
  //
  // Maybe the correct thing to do is to not register this service if no
  // profile has been selected?
  //
  NS_ENSURE_TRUE(history, NS_ERROR_FAILURE);

  PRBool canAdd;
  nsresult rv = history->CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  nsAutoPtr<SetTitleData> data(new SetTitleData());
  NS_ENSURE_STATE(data);

  data->uri = aURI;

  if (aTitle.IsEmpty()) {
    data->title.SetIsVoid(PR_TRUE);
  }
  else {
    data->title.Assign(aTitle);
  }

  nsCOMPtr<Step> task(new StartSetURITitleStep(data));

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
History::Observe(nsISupports* aSubject, const char* aTopic,
                 const PRUnichar* aData)
{
  if (strcmp(aTopic, TOPIC_PLACES_SHUTDOWN) == 0) {
    Shutdown();

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      (void)os->RemoveObserver(this, TOPIC_PLACES_SHUTDOWN);
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_THREADSAFE_ISUPPORTS2(
  History
, IHistory
, nsIObserver
)

} // namespace places
} // namespace mozilla
