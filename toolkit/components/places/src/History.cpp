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

class VisitedQuery : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS

  static nsresult Start(nsIURI* aURI)
  {
    NS_PRECONDITION(aURI, "Null URI");

#ifdef MOZ_IPC
  // If we are a content process, always remote the request to the
  // parent process.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentChild * cpc = 
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendStartVisitedQuery(aURI);
    return NS_OK;
  }
#endif

    nsNavHistory* navHist = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(navHist, NS_ERROR_FAILURE);
    mozIStorageStatement* stmt = navHist->GetStatementById(DB_IS_PAGE_VISITED);
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
NS_IMPL_ISUPPORTS1(
  VisitedQuery,
  mozIStorageStatementCallback
)

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
//// Steps for VisitURI

struct VisitURIData : public FailSafeFinishTask
{
  PRInt64 placeId;
  PRInt32 hidden;
  PRInt32 typed;
  nsCOMPtr<nsIURI> uri;

  // Url of last added visit in chain.
  nsCString lastSpec;
  PRInt64 lastVisitId;
  PRInt32 transitionType;
  PRInt64 sessionId;
  PRTime dateTime;
};

/**
 * Step 6: Update frecency of URI and notify observers.
 */
class UpdateFrecencyAndNotifyStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  UpdateFrecencyAndNotifyStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    // Result set contains new visit created in earlier step
    NS_ENSURE_STATE(aResultSet);

    nsCOMPtr<mozIStorageRow> row;
    nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt64 visitId;
    rv = row->GetInt64(0, &visitId);
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO need to figure out story for not synchronous frecency updating
    // (bug 556631)

    // Swallow errors here, since if we've gotten this far, it's more
    // important to notify the observers below.
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_WARN_IF_FALSE(history, "Could not get history service");
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_WARN_IF_FALSE(bookmarks, "Could not get bookmarks service");
    if (history && bookmarks) {
      // Update frecency *after* the visit info is in the db
      nsresult rv = history->UpdateFrecency(
        mData->placeId,
        bookmarks->IsRealBookmark(mData->placeId)
      );
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Could not update frecency");

      // Notify nsNavHistory observers of visit, but only for certain types of
      // visits to maintain consistency with nsNavHistory::GetQueryResults.
      if (!mData->hidden &&
          mData->transitionType != nsINavHistoryService::TRANSITION_EMBED &&
          mData->transitionType != nsINavHistoryService::TRANSITION_FRAMED_LINK) {
        history->NotifyOnVisit(mData->uri, visitId, mData->dateTime,
                               mData->sessionId, mData->lastVisitId,
                               mData->transitionType);
      }
    }

    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
    if (obsService) {
      nsresult rv = obsService->NotifyObservers(mData->uri, URI_VISIT_SAVED, nsnull);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Could not notify observers");
    }

    History::GetService()->NotifyVisited(mData->uri);

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  UpdateFrecencyAndNotifyStep
, mozIStorageStatementCallback
)

/**
 * Step 5: Get newly created visit ID from moz_history_visits table.
 */
class GetVisitIDStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  GetVisitIDStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    // Find visit ID, needed for notifying observers in next step.
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<mozIStorageStatement> stmt =
      history->GetStatementById(DB_RECENT_VISIT_OF_URL);
    NS_ENSURE_STATE(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new UpdateFrecencyAndNotifyStep(mData);
    NS_ENSURE_STATE(step);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  GetVisitIDStep
, mozIStorageStatementCallback
)

/**
 * Step 4: Add visit to moz_history_visits table.
 */
class AddVisitStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  AddVisitStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsresult rv;

    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

    // TODO need to figure out story for new session IDs that isn't synchronous
    // (bug 561450)

    if (aResultSet) {
      // Result set contains last visit information for this session
      nsCOMPtr<mozIStorageRow> row;
      rv = aResultSet->GetNextRow(getter_AddRefs(row));
      NS_ENSURE_SUCCESS(rv, rv);

      PRInt64 possibleSessionId;
      PRTime lastVisitOfSession;

      rv = row->GetInt64(0, &mData->lastVisitId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = row->GetInt64(1, &possibleSessionId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = row->GetInt64(2, &lastVisitOfSession);
      NS_ENSURE_SUCCESS(rv, rv);

      if (mData->dateTime - lastVisitOfSession <= RECENT_EVENT_THRESHOLD) {
        mData->sessionId = possibleSessionId;
      }
      else {
        // Session is too old. Start a new one.
        mData->sessionId = history->GetNewSessionID();
        mData->lastVisitId = 0;
      }
    }
    else {
      // No previous saved visit entry could be found, so start a new session.
      mData->sessionId = history->GetNewSessionID();
      mData->lastVisitId = 0;
    }

    nsCOMPtr<mozIStorageStatement> stmt =
      history->GetStatementById(DB_INSERT_VISIT);
    NS_ENSURE_STATE(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("from_visit"),
                               mData->lastVisitId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                               mData->placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"),
                               mData->dateTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"),
                               mData->transitionType);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("session"),
                               mData->sessionId);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new GetVisitIDStep(mData);
    NS_ENSURE_STATE(step);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  AddVisitStep
, mozIStorageStatementCallback
)

/**
 * Step 3: Callback for inserting or updating a moz_places entry.
 *         This step checks database for the last visit in session.
 */
class CheckLastVisitStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  CheckLastVisitStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsresult rv;

    if (aResultSet) {
      // Last step inserted a new URL. This query contains the id.
      nsCOMPtr<mozIStorageRow> row;
      rv = aResultSet->GetNextRow(getter_AddRefs(row));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = row->GetInt64(0, &mData->placeId);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!mData->lastSpec.IsEmpty()) {
      // Find last visit ID and session ID using lastSpec so we can add them
      // to a browsing session if the visit was recent.
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      nsCOMPtr<mozIStorageStatement> stmt =
        history->GetStatementById(DB_RECENT_VISIT_OF_URL);
      NS_ENSURE_STATE(stmt);

      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->lastSpec);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<Step> step = new AddVisitStep(mData);
      NS_ENSURE_STATE(step);
      rv = step->ExecuteAsync(stmt);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Empty lastSpec.
      // Not part of a session.  Just run next step's callback with no results.
      nsCOMPtr<Step> step = new AddVisitStep(mData);
      NS_ENSURE_STATE(step);
      rv = step->Callback(NULL);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  CheckLastVisitStep
, mozIStorageStatementCallback
)

/**
 * Step 2a: Called only when a new entry is put into moz_places.
 *          Finds the ID of a recently inserted place.
 */
class FindNewIdStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  FindNewIdStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<mozIStorageStatement> stmt =
      history->GetStatementById(DB_GET_PAGE_VISIT_STATS);
    NS_ENSURE_STATE(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new CheckLastVisitStep(mData);
    NS_ENSURE_STATE(step);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  FindNewIdStep
, mozIStorageStatementCallback
)

/**
 * Step 2: Callback for checking for an existing URI in moz_places.
 *         This step inserts or updates the URI accordingly.
 */
class CheckExistingStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  CheckExistingStep(nsAutoPtr<VisitURIData> aData)
  : mData(aData)
  {
  }

  NS_IMETHOD Callback(mozIStorageResultSet* aResultSet)
  {
    nsresult rv;
    nsCOMPtr<mozIStorageStatement> stmt;

    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

    if (aResultSet) {
      nsCOMPtr<mozIStorageRow> row;
      rv = aResultSet->GetNextRow(getter_AddRefs(row));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = row->GetInt64(0, &mData->placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!mData->typed) {
        // If this transition wasn't typed, others might have been. If database
        // has location as typed, reflect that in our data structure.
        rv = row->GetInt32(2, &mData->typed);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (mData->hidden) {
        // If this transition was hidden, it is possible that others were not.
        // Any one visible transition makes this location visible. If database
        // has location as visible, reflect that in our data structure.
        rv = row->GetInt32(3, &mData->hidden);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Note: trigger will update visit_count.
      stmt = history->GetStatementById(DB_UPDATE_PAGE_VISIT_STATS);
      NS_ENSURE_STATE(stmt);

      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), mData->typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), mData->hidden);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mData->placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<Step> step = new CheckLastVisitStep(mData);
      NS_ENSURE_STATE(step);
      rv = step->ExecuteAsync(stmt);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // No entry exists, so create one.
      stmt = history->GetStatementById(DB_ADD_NEW_PAGE);
      NS_ENSURE_STATE(stmt);

      nsAutoString revHost;
      rv = GetReversedHostname(mData->uri, revHost);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("rev_host"), revHost);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), mData->typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), mData->hidden);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("frecency"), -1);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<Step> step = new FindNewIdStep(mData);
      NS_ENSURE_STATE(step);
      rv = step->ExecuteAsync(stmt);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  CheckExistingStep
, mozIStorageStatementCallback
)

/**
 * Step 1: See if there is an existing URI.
 */
class StartVisitURIStep : public Step
{
public:
  NS_DECL_ISUPPORTS

  StartVisitURIStep(nsAutoPtr<VisitURIData> aData)
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
      history->GetStatementById(DB_GET_PAGE_VISIT_STATS);
    NS_ENSURE_STATE(stmt);

    nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mData->uri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<Step> step = new CheckExistingStep(mData);
    NS_ENSURE_STATE(step);
    rv = step->ExecuteAsync(stmt);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

protected:
  nsAutoPtr<VisitURIData> mData;
};
NS_IMPL_ISUPPORTS1(
  StartVisitURIStep
, Step
)

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
  NS_DECL_ISUPPORTS

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
NS_IMPL_ISUPPORTS1(
  TitleNotifyStep
, mozIStorageStatementCallback
)

/**
 * Step 2: Set title.
 */
class SetTitleStep : public Step
{
public:
  NS_DECL_ISUPPORTS

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
NS_IMPL_ISUPPORTS1(
  SetTitleStep
, mozIStorageStatementCallback
)

/**
 * Step 1: See if there is an existing URI.
 */
class StartSetURITitleStep : public Step
{
public:
  NS_DECL_ISUPPORTS

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
NS_IMPL_ISUPPORTS1(
  StartSetURITitleStep
, Step
)

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// History

History* History::gService = NULL;

History::History()
: mShuttingDown(false)
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
    mozilla::dom::ContentChild * cpc = 
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

  // Populate data structure that will be used in our async SQL steps.
  nsAutoPtr<VisitURIData> data(new VisitURIData());
  NS_ENSURE_STATE(data);

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aLastVisitedURI) {
    rv = aLastVisitedURI->GetSpec(data->lastSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (spec.Equals(data->lastSpec)) {
    // Do not save refresh-page visits.
    return NS_OK;
  }

  // Assigns a type to the edge in the visit linked list. Each type will be
  // considered differently when weighting the frecency of a location.
  PRUint32 recentFlags = history->GetRecentFlags(aURI);
  bool redirected = false;
  if (aFlags & IHistory::REDIRECT_TEMPORARY) {
    data->transitionType = nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY;
    redirected = true;
  }
  else if (aFlags & IHistory::REDIRECT_PERMANENT) {
    data->transitionType = nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT;
    redirected = true;
  }
  else if (recentFlags & nsNavHistory::RECENT_TYPED) {
    data->transitionType = nsINavHistoryService::TRANSITION_TYPED;
  }
  else if (recentFlags & nsNavHistory::RECENT_BOOKMARKED) {
    data->transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;
  }
  else if (aFlags & IHistory::TOP_LEVEL) {
    // User was redirected or link was clicked in the main window.
    data->transitionType = nsINavHistoryService::TRANSITION_LINK;
  }
  else if (recentFlags & nsNavHistory::RECENT_ACTIVATED) {
    // User activated a link in a frame.
    data->transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
  }
  else {
    // A frame redirected to a new site without user interaction.
    data->transitionType = nsINavHistoryService::TRANSITION_EMBED;
  }

  data->typed = (data->transitionType == nsINavHistoryService::TRANSITION_TYPED) ? 1 : 0;
  data->hidden = 
    (data->transitionType == nsINavHistoryService::TRANSITION_FRAMED_LINK ||
     data->transitionType == nsINavHistoryService::TRANSITION_EMBED ||
     redirected) ? 1 : 0;
  data->dateTime = PR_Now();
  data->uri = aURI;

  nsCOMPtr<Step> task(new StartVisitURIStep(data));

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

NS_IMPL_ISUPPORTS2(
  History
, IHistory
, nsIObserver
)

} // namespace places
} // namespace mozilla
