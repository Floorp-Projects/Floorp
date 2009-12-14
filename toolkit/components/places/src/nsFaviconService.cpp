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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Marco Bonardo <mak77@bonardo.net>
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

/**
 * This is the favicon service, which stores favicons for web pages with your
 * history as you browse. It is also used to save the favicons for bookmarks.
 *
 * DANGER: The history query system makes assumptions about the favicon storage
 * so that icons can be quickly generated for history/bookmark result sets. If
 * you change the database layout at all, you will have to update both services.
 */

#include "nsFaviconService.h"
#include "nsICacheService.h"
#include "nsICacheVisitor.h"
#include "nsICachingChannel.h"
#include "nsICategoryManager.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsISupportsPrimitives.h"
#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "plbase64.h"
#include "nsPlacesTables.h"
#include "nsPlacesMacros.h"
#include "nsIPrefService.h"
#include "Helpers.h"

#include "mozilla/storage.h"

// For large favicons optimization.
#include "imgITools.h"
#include "imgIContainer.h"

// Default value for mOptimizedIconDimension
#define OPTIMIZED_FAVICON_DIMENSION 16

// Most icons will be smaller than this rough estimate of the size of an
// uncompressed 16x16 RGBA image of the same dimensions.
#define MAX_ICON_FILESIZE(s) ((PRUint32) s*s*4)

#define MAX_FAVICON_CACHE_SIZE 256
#define FAVICON_CACHE_REDUCE_COUNT 64

#define CONTENT_SNIFFING_SERVICES "content-sniffing-services"

/**
 * The maximum time we will keep a favicon around.  We always ask the cache, if
 * we can, but default to this value if we do not get a time back, or the time
 * is more in the future than this.
 * Currently set to one week from now.
 */
#define MAX_FAVICON_EXPIRATION ((PRTime)7 * 24 * 60 * 60 * PR_USEC_PER_SEC)

using namespace mozilla::places;

////////////////////////////////////////////////////////////////////////////////
//// Global Helpers

////////////////////////////////////////////////////////////////////////////////
//// FaviconLoadListener definition

class FaviconLoadListener : public nsIStreamListener,
                            public nsIInterfaceRequestor,
                            public nsIChannelEventSink
{
public:
  FaviconLoadListener(nsIURI* aPageURI,
                      nsIURI* aFaviconURI,
                      nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

private:
  ~FaviconLoadListener();

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mPageURI;
  nsCOMPtr<nsIURI> mFaviconURI;

  nsCString mData;
};

////////////////////////////////////////////////////////////////////////////////
//// ExpireFaviconsStatementCallbackNotifier definition

// Used to notify a topic to system observers on async execute completion.
// Will throw on error.
class ExpireFaviconsStatementCallbackNotifier : public AsyncStatementCallback
{
public:
  ExpireFaviconsStatementCallbackNotifier(bool *aFaviconsExpirationRunning);
  NS_DECL_ISUPPORTS
  NS_DECL_ASYNCSTATEMENTCALLBACK

private:
  bool *mFaviconsExpirationRunning;
};

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsFaviconService, gFaviconService)

NS_IMPL_ISUPPORTS1(
  nsFaviconService
, nsIFaviconService
)

namespace {

/**
 * Determines the real page URI that a favicon should be stored for.
 *
 * @param aPageURI
 *        The page that is trying to load the favicon.
 * @param aFaviconURI
 *        The URI of the favicon in question.
 * @return the URI to load this favicon for, or null if this icon should not be
 *         loaded.
 */
already_AddRefed<nsIURI>
GetEffectivePageForFavicon(nsIURI *aPageURI,
                              nsIURI *aFaviconURI)
{
  NS_ASSERTION(aPageURI, "Must provide a pageURI!");
  NS_ASSERTION(aFaviconURI, "Must provide a favicon URI!");

  nsCOMPtr<nsIURI> pageURI(aPageURI);

  nsNavHistory *history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, nsnull);

  PRBool canAddToHistory;
  nsresult rv = history->CanAddURI(pageURI, &canAddToHistory);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv), nsnull);

  // If history is disabled or the page isn't addable to history, only load
  // favicons if the page is bookmarked.
  if (!canAddToHistory || history->IsHistoryDisabled()) {
    nsNavBookmarks *bmSvc = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bmSvc, nsnull);

    // Check if the page is bookmarked.
    nsCOMPtr<nsIURI> bookmarkedURI;
    rv = bmSvc->GetBookmarkedURIFor(aPageURI, getter_AddRefs(bookmarkedURI));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && bookmarkedURI, nsnull);

    // We always want to use the bookmark URI regardless of aPageURI.
    pageURI = bookmarkedURI.forget();
  }

  // If we are given a URI to an image, the favicon URI will be the same as the
  // page URI.
  // TODO: In future we'll probably want to store a resample of the image, but
  // for now we just avoid that, for database size concerns.
  PRBool pageEqualsFavicon;
  rv = pageURI->Equals(aFaviconURI, &pageEqualsFavicon);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !pageEqualsFavicon, nsnull);

  // We don't store favicons to error pages.
  nsCOMPtr<nsIURI> errorPageFaviconURI;
  rv = NS_NewURI(getter_AddRefs(errorPageFaviconURI),
                 NS_LITERAL_CSTRING(FAVICON_ERRORPAGE_URL));
  NS_ENSURE_SUCCESS(rv, nsnull);
  PRBool isErrorPage;
  rv = aFaviconURI->Equals(errorPageFaviconURI, &isErrorPage);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !isErrorPage, nsnull);

  // This favicon should load, so return the page's URI.
  return pageURI.forget();
}

/**
 * This class gets the expiration data for a favicon, and starts the lookup of
 * the favicon's data if it should be loaded.
 */
class FaviconExpirationGetter : public AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS

  FaviconExpirationGetter(nsIURI *aPageURI,
                          nsIURI *aFaviconURI,
                          bool aForceReload) :
    mPageURI(aPageURI)
  , mFaviconURI(aFaviconURI)
  , mForceReload(aForceReload)
  , mHasData(false)
  , mExpiration(0)
  {
  }

  /**
   * Performs a lookup of the needed information asynchronously, and loads the
   * icon if necessary.
   */
  NS_IMETHOD checkAndLoad(mozIStorageStatement *aStatement)
  {
    mozStorageStatementScoper scoper(aStatement);
    nsresult rv = BindStatementURI(aStatement, 0, mFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStoragePendingStatement> ps;
    rv = aStatement->ExecuteAsync(this, getter_AddRefs(ps));
    NS_ENSURE_SUCCESS(rv, rv);
    // ExecuteAsync will reset the statement for us.
    scoper.Abandon();
    return NS_OK;
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet *aResultSet)
  {
    nsCOMPtr<mozIStorageRow> row;
    nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 dataSize = 0;
    (void)row->GetInt32(1, &dataSize);
    mHasData = dataSize > 0;
    (void)row->GetInt64(2, &mExpiration);

    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
      return NS_OK;

    // See if we have data and get the expiration time for this favicon.  We DO
    // NOT want to set the favicon for the page unless we know we have it.  For
    // example, if I go to random site x.com, the browser will still tell us
    // that the favicon is x.com/favicon.ico even if there is no such file.  We
    // don't want to pollute our tables with this useless data.  We also do not
    // want to reload the icon if it hasn't expired yet.
    if (mHasData && PR_Now() < mExpiration && !mForceReload) {
      // Our data is likely still valid, but we should check to make sure the
      // URI has changed, otherwise there is no need to notify.
      nsFaviconService *fs = nsFaviconService::GetFaviconService();
      NS_ENSURE_TRUE(fs, NS_ERROR_OUT_OF_MEMORY);
      fs->checkAndNotify(mPageURI, mFaviconURI);
      return NS_OK;
    }

    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel), mFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener =
      new FaviconLoadListener(mPageURI, mFaviconURI, channel);
    NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsIInterfaceRequestor> listenerRequestor =
      do_QueryInterface(listener, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = channel->SetNotificationCallbacks(listenerRequestor);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = channel->AsyncOpen(listener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }
private:
  nsCOMPtr<nsIURI> mPageURI;
  nsCOMPtr<nsIURI> mFaviconURI;
  const bool mForceReload;
  bool mHasData;
  PRTime mExpiration;
};

NS_IMPL_ISUPPORTS1(
  FaviconExpirationGetter,
  mozIStorageStatementCallback
)

} // anonymous namespace


nsFaviconService::nsFaviconService() : mFaviconsExpirationRunning(false)
                                     , mOptimizedIconDimension(OPTIMIZED_FAVICON_DIMENSION)
                                     , mFailedFaviconSerial(0)
{
  NS_ASSERTION(!gFaviconService,
               "Attempting to create two instances of the service!");
  gFaviconService = this;
}

nsFaviconService::~nsFaviconService()
{
  NS_ASSERTION(gFaviconService == this,
               "Deleting a non-singleton instance of the service");
  if (gFaviconService == this)
    gFaviconService = nsnull;
}

nsresult
nsFaviconService::Init()
{
  // creation of history service will call InitTables
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);
  mDBConn = historyService->GetStorageConnection();
  NS_ENSURE_TRUE(mDBConn, NS_ERROR_FAILURE);

  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, length(data), expiration FROM moz_favicons WHERE url = ?1"),
    getter_AddRefs(mDBGetIconInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // We can avoid checking for duplicates in the unified table since an uri
  // can only have one favicon associated. LIMIT 1 will ensure that we get
  // only one result.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT f.id, f.url, length(f.data), f.expiration "
      "FROM ( "
        "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places_temp "
        "WHERE url = ?1 "
        "UNION ALL "
        "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places "
        "WHERE url = ?1 "
      ") AS h JOIN moz_favicons f ON h.favicon_id = f.id "
      "LIMIT 1"),
    getter_AddRefs(mDBGetURL));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT f.data, f.mime_type FROM moz_favicons f WHERE url = ?1"),
    getter_AddRefs(mDBGetData));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_favicons (url, data, mime_type, expiration) "
      "VALUES (?1, ?2, ?3, ?4)"),
    getter_AddRefs(mDBInsertIcon));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_favicons SET data = ?2, mime_type = ?3, expiration = ?4 "
      "WHERE id = ?1"),
    getter_AddRefs(mDBUpdateIcon));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view SET favicon_id = ?2 WHERE id = ?1"),
    getter_AddRefs(mDBSetPageFavicon));
  NS_ENSURE_SUCCESS(rv, rv);

  // failed favicon cache
  if (! mFailedFavicons.Init(MAX_FAVICON_CACHE_SIZE))
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIPrefBranch> pb = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pb)
    pb->GetIntPref("places.favicons.optimizeToDimension", &mOptimizedIconDimension);

  return NS_OK;
}

// nsFaviconService::InitTables
//
//    Called by the history service to create the favicon table. The history
//    service uses this table in its queries, so it must exist even if
//    nobody has called the favicon service.

nsresult // static
nsFaviconService::InitTables(mozIStorageConnection* aDBConn)
{
  nsresult rv;
  PRBool exists = PR_FALSE;
  aDBConn->TableExists(NS_LITERAL_CSTRING("moz_favicons"), &exists);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_FAVICONS);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ExpireAllFavicons()
{
  mFaviconsExpirationRunning = true;

  // Remove all references to favicons.
  // We do this in 2 steps, first we null-out all favicons in the disk table,
  // then we do the same in the temp table.  This is because the view UPDATE
  // trigger does not allow setting a NULL value to prevent dataloss.
  nsCOMPtr<mozIStorageStatement> removeOnDiskReferences;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET favicon_id = NULL "
      "WHERE favicon_id NOT NULL"
    ), getter_AddRefs(removeOnDiskReferences));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> removeTempReferences;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_temp "
      "SET favicon_id = NULL "
      "WHERE favicon_id NOT NULL"
    ), getter_AddRefs(removeTempReferences));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove all favicons.
  // We run async, so be sure to not remove any favicon that could have been
  // created in the meantime.
  nsCOMPtr<mozIStorageStatement> removeFavicons;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_favicons WHERE id NOT IN ("
        "SELECT favicon_id FROM moz_places_temp WHERE favicon_id NOT NULL "
        "UNION ALL "
        "SELECT favicon_id FROM moz_places WHERE favicon_id NOT NULL "
      ")"
    ), getter_AddRefs(removeFavicons));
  NS_ENSURE_SUCCESS(rv, rv);

  mozIStorageStatement *stmts[] = {
    removeOnDiskReferences,
    removeTempReferences,
    removeFavicons
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  nsCOMPtr<ExpireFaviconsStatementCallbackNotifier> callback =
    new ExpireFaviconsStatementCallbackNotifier(&mFaviconsExpirationRunning);
  rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts), callback,
                             getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIFaviconService

NS_IMETHODIMP
nsFaviconService::SetFaviconUrlForPage(nsIURI* aPageURI, nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  if (mFaviconsExpirationRunning)
    return NS_OK;

  PRBool hasData;
  nsresult rv = SetFaviconUrlForPageInternal(aPageURI, aFaviconURI, &hasData);
  NS_ENSURE_SUCCESS(rv, rv);

  // send favicon change notifications if the URL has any data
  if (hasData)
    SendFaviconNotifications(aPageURI, aFaviconURI);
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetDefaultFavicon(nsIURI** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  // not found, use default
  if (!mDefaultIcon) {
    nsresult rv = NS_NewURI(getter_AddRefs(mDefaultIcon),
                            NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return mDefaultIcon->Clone(_retval);
}

// nsFaviconService::SetFaviconUrlForPageInternal
//
//    This creates a new entry in the favicon table if necessary and tells the
//    history service to associate the given favicon ID with the given URI. We
//    don't want to update the history table directly since that may involve
//    creating a new row in the history table, which should only be done by
//    history.
//
//    This sets aHasData if there was already icon data for this favicon. Used
//    to know if we should try reloading.
//
//    Does NOT send out notifications. Caller should send out notifications
//    if the favicon has data.

nsresult
nsFaviconService::SetFaviconUrlForPageInternal(nsIURI* aPageURI,
                                               nsIURI* aFaviconURI,
                                               PRBool* aHasData)
{
  nsresult rv;
  PRInt64 iconId = -1;
  *aHasData = PR_FALSE;

  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);

  if (historyService->InPrivateBrowsingMode())
    return NS_OK;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  {
    mozStorageStatementScoper scoper(mDBGetIconInfo);
    rv = BindStatementURI(mDBGetIconInfo, 0, aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult = PR_FALSE;
    if (NS_SUCCEEDED(mDBGetIconInfo->ExecuteStep(&hasResult)) && hasResult) {
      // We already have an entry for this icon, just get the stats
      rv = mDBGetIconInfo->GetInt64(0, &iconId);
      NS_ENSURE_SUCCESS(rv, rv);

      // see if this icon has data already
      PRInt32 dataSize;
      rv = mDBGetIconInfo->GetInt32(1, &dataSize);
      NS_ENSURE_SUCCESS(rv, rv);
      if (dataSize > 0)
        *aHasData = PR_TRUE;
    }
  }

  if (iconId == -1) {
    // We did not find any entry, so create a new one

    // not-binded params are automatically nullified by mozStorage
    mozStorageStatementScoper scoper(mDBInsertIcon);
    rv = BindStatementURI(mDBInsertIcon, 0, aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBInsertIcon->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    {
      mozStorageStatementScoper scoper(mDBGetIconInfo);

      rv = BindStatementURI(mDBGetIconInfo, 0, aFaviconURI);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool hasResult;
      rv = mDBGetIconInfo->ExecuteStep(&hasResult);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");
      iconId = mDBGetIconInfo->AsInt64(0);
    }
  }

  // now link our icon entry with the page
  PRInt64 pageId;
  rv = historyService->GetUrlIdFor(aPageURI, &pageId, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scoper(mDBSetPageFavicon);
  rv = mDBSetPageFavicon->BindInt64Parameter(0, pageId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBSetPageFavicon->BindInt64Parameter(1, iconId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBSetPageFavicon->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// nsFaviconService::UpdateBookmarkRedirectFavicon
//
//    It is not uncommon to have a bookmark (usually manually entered or
//    modified) that redirects to some other page. For example, "mozilla.org"
//    redirects to "www.mozilla.org". We want that bookmark's favicon to get
//    updated. So, we see if this URI has a bookmark redirect and set the
//    favicon there as well.
//
//    This should be called only when we know there is data for the favicon
//    already loaded. We will always send out notifications for the bookmarked
//    page.

nsresult
nsFaviconService::UpdateBookmarkRedirectFavicon(nsIURI* aPageURI,
                                                nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG_POINTER(aPageURI);
  NS_ENSURE_ARG_POINTER(aFaviconURI);

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIURI> bookmarkURI;
  nsresult rv = bookmarks->GetBookmarkedURIFor(aPageURI,
                                               getter_AddRefs(bookmarkURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (! bookmarkURI)
    return NS_OK; // no bookmark redirect

  PRBool sameAsBookmark;
  if (NS_SUCCEEDED(bookmarkURI->Equals(aPageURI, &sameAsBookmark)) &&
      sameAsBookmark)
    return NS_OK; // bookmarked directly, not through a redirect

  PRBool hasData = PR_FALSE;
  rv = SetFaviconUrlForPageInternal(bookmarkURI, aFaviconURI, &hasData);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasData) {
    // send notifications
    SendFaviconNotifications(bookmarkURI, aFaviconURI);
  } else {
    NS_WARNING("Calling UpdateBookmarkRedirectFavicon when you don't have data for the favicon yet.");
  }
  return NS_OK;
}

// nsFaviconService::SendFaviconNotifications
//
//    Call to send out favicon changed notifications. Should only be called
//    when you know there is data loaded for the favicon.

void
nsFaviconService::SendFaviconNotifications(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI)
{
  nsCAutoString faviconSpec;
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  if (historyService && NS_SUCCEEDED(aFaviconURI->GetSpec(faviconSpec))) {
    historyService->SendPageChangedNotification(aPageURI,
                                       nsINavHistoryObserver::ATTRIBUTE_FAVICON,
                                       NS_ConvertUTF8toUTF16(faviconSpec));
  }
}

NS_IMETHODIMP
nsFaviconService::SetAndLoadFaviconForPage(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI,
                                           PRBool aForceReload)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  if (mFaviconsExpirationRunning)
    return NS_OK;

#ifdef LAZY_ADD
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);
  return historyService->AddLazyLoadFaviconMessage(aPageURI,
                                                   aFaviconURI,
                                                   aForceReload);
#else
  return DoSetAndLoadFaviconForPage(aPageURI, aFaviconURI, aForceReload);
#endif
}

nsresult
nsFaviconService::DoSetAndLoadFaviconForPage(nsIURI* aPageURI,
                                             nsIURI* aFaviconURI,
                                             PRBool aForceReload)
{
  if (mFaviconsExpirationRunning)
    return NS_OK;

  // If a favicon is in the failed cache, we'll only load it if we are forcing
  // a reload.
  PRBool previouslyFailed;
  nsresult rv = IsFailedFavicon(aFaviconURI, &previouslyFailed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (previouslyFailed) {
    if (aForceReload)
      RemoveFailedFavicon(aFaviconURI); // force reload clears from failed cache
    else
      return NS_OK; // ignore previously failed favicons
  }

  // Get the URI we should load this favicon for.  If we don't get any URI, then
  // we don't want to save this favicon, so we early return.
  nsCOMPtr<nsIURI> page = GetEffectivePageForFavicon(aPageURI, aFaviconURI);
  NS_ENSURE_TRUE(page, NS_OK);

  nsRefPtr<FaviconExpirationGetter> dataGetter =
    new FaviconExpirationGetter(page, aFaviconURI, !!aForceReload);
  NS_ENSURE_TRUE(dataGetter, NS_ERROR_OUT_OF_MEMORY);

  rv = dataGetter->checkAndLoad(mDBGetIconInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // DB will be updated and observers notified when data has finished loading.
  return NS_OK;
}

// nsFaviconService::SetFaviconData
//
//    See the IDL for this function for lots of info. Note from there: we don't
//    send out notifications.

NS_IMETHODIMP
nsFaviconService::SetFaviconData(nsIURI* aFaviconURI, const PRUint8* aData,
                                 PRUint32 aDataLen, const nsACString& aMimeType,
                                 PRTime aExpiration)
{
  NS_ENSURE_ARG(aFaviconURI);

  if (mFaviconsExpirationRunning)
    return NS_OK;

  nsresult rv;
  PRUint32 dataLen = aDataLen;
  const PRUint8* data = aData;
  const nsACString* mimeType = &aMimeType;
  nsCString newData, newMimeType;

  // If the page provided a large image for the favicon (eg, a highres image
  // or a multiresolution .ico file), we don't want to store more data than
  // needed.
  if (aDataLen > MAX_ICON_FILESIZE(mOptimizedIconDimension)) {
    rv = OptimizeFaviconImage(aData, aDataLen, aMimeType, newData, newMimeType);
    if (NS_SUCCEEDED(rv) && newData.Length() < aDataLen) {
      data = reinterpret_cast<PRUint8*>(const_cast<char*>(newData.get())),
      dataLen = newData.Length();
      mimeType = &newMimeType;
    }
    else if (aDataLen > MAX_FAVICON_SIZE) {
      // We cannot optimize this favicon size and we are over the maximum size
      // allowed, so we will not save data to the db to avoid bloating it.
      return NS_ERROR_FAILURE;
    }
  }

  mozIStorageStatement* statement;
  {
    // this block forces the scoper to reset our statement: necessary for the
    // next statement
    mozStorageStatementScoper scoper(mDBGetIconInfo);
    rv = BindStatementURI(mDBGetIconInfo, 0, aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = mDBGetIconInfo->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasResult) {
      // update old one (statement parameter 0 = ID)
      PRInt64 id;
      rv = mDBGetIconInfo->GetInt64(0, &id);
      NS_ENSURE_SUCCESS(rv, rv);
      statement = mDBUpdateIcon;
      rv = statement->BindInt64Parameter(0, id);
    } else {
      // insert new one (statement parameter 0 = favicon URL)
      statement = mDBInsertIcon;
      rv = BindStatementURI(statement, 0, aFaviconURI);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageStatementScoper scoper(statement);

  // the insert and update statements share all but the 0th parameter
  rv = statement->BindBlobParameter(1, data, dataLen);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringParameter(2, *mimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(3, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);
  return statement->Execute();
}

NS_IMETHODIMP
nsFaviconService::SetFaviconDataFromDataURL(nsIURI* aFaviconURI,
                                            const nsAString& aDataURL,
                                            PRTime aExpiration)
{
  NS_ENSURE_ARG(aFaviconURI);
  if (mFaviconsExpirationRunning)
    return NS_OK;

  nsCOMPtr<nsIURI> dataURI;
  nsresult rv = NS_NewURI(getter_AddRefs(dataURI), aDataURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // use the data: protocol handler to convert the data
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIProtocolHandler> protocolHandler;
  rv = ioService->GetProtocolHandler("data", getter_AddRefs(protocolHandler));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = protocolHandler->NewChannel(dataURI, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // blocking stream is OK for data URIs
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 available;
  rv = stream->Available(&available);
  NS_ENSURE_SUCCESS(rv, rv);
  if (available == 0)
    return NS_ERROR_FAILURE;

  // read all the decoded data
  PRUint8* buffer = static_cast<PRUint8*>
                               (nsMemory::Alloc(sizeof(PRUint8) * available));
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 numRead;
  rv = stream->Read(reinterpret_cast<char*>(buffer), available, &numRead);
  if (NS_FAILED(rv) || numRead != available) {
    nsMemory::Free(buffer);
    return rv;
  }

  nsCAutoString mimeType;
  rv = channel->GetContentType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  // SetFaviconData can now do the dirty work 
  rv = SetFaviconData(aFaviconURI, buffer, available, mimeType, aExpiration);
  nsMemory::Free(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconData(nsIURI* aFaviconURI, nsACString& aMimeType,
                                 PRUint32* aDataLen, PRUint8** aData)
{
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG_POINTER(aDataLen);
  NS_ENSURE_ARG_POINTER(aData);

  mozStorageStatementScoper scoper(mDBGetData);
  nsresult rv = BindStatementURI(mDBGetData, 0, aFaviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult = PR_FALSE;
  if (NS_SUCCEEDED(mDBGetData->ExecuteStep(&hasResult)) && hasResult) {
    rv = mDBGetData->GetUTF8String(1, aMimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    return mDBGetData->GetBlob(0, aDataLen, aData);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconDataAsDataURL(nsIURI* aFaviconURI,
                                          nsAString& aDataURL)
{
  NS_ENSURE_ARG(aFaviconURI);

  PRUint8* data;
  PRUint32 dataLen;
  nsCAutoString mimeType;

  nsresult rv = GetFaviconData(aFaviconURI, mimeType, &dataLen, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!data) {
    aDataURL.SetIsVoid(PR_TRUE);
    return NS_OK;
  }

  char* encoded = PL_Base64Encode(reinterpret_cast<const char*>(data),
                                  dataLen, nsnull);
  nsMemory::Free(data);

  if (!encoded)
    return NS_ERROR_OUT_OF_MEMORY;

  aDataURL.AssignLiteral("data:");
  AppendUTF8toUTF16(mimeType, aDataURL);
  aDataURL.AppendLiteral(";base64,");
  AppendUTF8toUTF16(encoded, aDataURL);

  nsMemory::Free(encoded);
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconForPage(nsIURI* aPageURI, nsIURI** _retval)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG_POINTER(_retval);

  mozStorageStatementScoper scoper(mDBGetURL);
  nsresult rv = BindStatementURI(mDBGetURL, 0, aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  if (NS_SUCCEEDED(mDBGetURL->ExecuteStep(&hasResult)) && hasResult) {
    nsCAutoString url;
    rv = mDBGetURL->GetUTF8String(1, url);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_NewURI(_retval, url);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconImageForPage(nsIURI* aPageURI, nsIURI** _retval)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG_POINTER(_retval);

  mozStorageStatementScoper scoper(mDBGetURL);
  nsresult rv = BindStatementURI(mDBGetURL, 0, aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  nsCOMPtr<nsIURI> faviconURI;
  if (NS_SUCCEEDED(mDBGetURL->ExecuteStep(&hasResult)) && hasResult) {
    PRInt32 dataLen;
    rv = mDBGetURL->GetInt32(2, &dataLen);
    NS_ENSURE_SUCCESS(rv, rv);
    if (dataLen > 0) {
      // this page has a favicon entry with data
      nsCAutoString favIconUri;
      rv = mDBGetURL->GetUTF8String(1, favIconUri);
      NS_ENSURE_SUCCESS(rv, rv);

      return GetFaviconLinkForIconString(favIconUri, _retval);
    }
  }

  // not found, use default
  return GetDefaultFavicon(_retval);
}

nsresult
nsFaviconService::GetFaviconLinkForIcon(nsIURI* aFaviconURI,
                                        nsIURI** aOutputURI)
{
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG_POINTER(aOutputURI);

  nsCAutoString spec;
  if (aFaviconURI) {
    nsresult rv = aFaviconURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return GetFaviconLinkForIconString(spec, aOutputURI);
}

static PLDHashOperator
ExpireFailedFaviconsCallback(nsCStringHashKey::KeyType aKey,
                             PRUint32& aData,
                             void* userArg)
{
  PRUint32* threshold = reinterpret_cast<PRUint32*>(userArg);
  if (aData < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsFaviconService::AddFailedFavicon(nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aFaviconURI);

  nsCAutoString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (! mFailedFavicons.Put(spec, mFailedFaviconSerial))
    return NS_ERROR_OUT_OF_MEMORY;
  mFailedFaviconSerial ++;

  if (mFailedFavicons.Count() > MAX_FAVICON_CACHE_SIZE) {
    // need to expire some entries, delete the FAVICON_CACHE_REDUCE_COUNT number
    // of items that are the oldest
    PRUint32 threshold = mFailedFaviconSerial -
                         MAX_FAVICON_CACHE_SIZE + FAVICON_CACHE_REDUCE_COUNT;
    mFailedFavicons.Enumerate(ExpireFailedFaviconsCallback, &threshold);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::RemoveFailedFavicon(nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aFaviconURI);

  nsCAutoString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // we silently do nothing and succeed if the icon is not in the cache
  mFailedFavicons.Remove(spec);
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::IsFailedFavicon(nsIURI* aFaviconURI, PRBool* _retval)
{
  NS_ENSURE_ARG(aFaviconURI);
  nsCAutoString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 serial;
  *_retval = mFailedFavicons.Get(spec, &serial);
  return NS_OK;
}

// nsFaviconService::GetFaviconLinkForIconString
//
//    This computes a favicon URL with string input and using the cached
//    default one to minimize parsing.

nsresult
nsFaviconService::GetFaviconLinkForIconString(const nsCString& aSpec,
                                              nsIURI** aOutput)
{
  if (aSpec.IsEmpty()) {
    // default icon for empty strings
    if (! mDefaultIcon) {
      nsresult rv = NS_NewURI(getter_AddRefs(mDefaultIcon),
                              NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return mDefaultIcon->Clone(aOutput);
  }

  if (StringBeginsWith(aSpec, NS_LITERAL_CSTRING("chrome:"))) {
    // pass through for chrome URLs, since they can be referenced without
    // this service
    return NS_NewURI(aOutput, aSpec);
  }

  nsCAutoString annoUri;
  annoUri.AssignLiteral("moz-anno:" FAVICON_ANNOTATION_NAME ":");
  annoUri += aSpec;
  return NS_NewURI(aOutput, annoUri);
}

// nsFaviconService::GetFaviconSpecForIconString
//
//    This computes a favicon spec for when you don't want a URI object (as in
//    the tree view implementation), sparing all parsing and normalization.
void
nsFaviconService::GetFaviconSpecForIconString(const nsCString& aSpec,
                                              nsACString& aOutput)
{
  if (aSpec.IsEmpty()) {
    aOutput.AssignLiteral(FAVICON_DEFAULT_URL);
  } else if (StringBeginsWith(aSpec, NS_LITERAL_CSTRING("chrome:"))) {
    aOutput = aSpec;
  } else {
    aOutput.AssignLiteral("moz-anno:" FAVICON_ANNOTATION_NAME ":");
    aOutput += aSpec;
  }
}

// nsFaviconService::OptimizeFaviconImage
//
// Given a blob of data (a image file already read into a buffer), optimize
// its size by recompressing it as a 16x16 PNG.
nsresult
nsFaviconService::OptimizeFaviconImage(const PRUint8* aData, PRUint32 aDataLen,
                                       const nsACString& aMimeType,
                                       nsACString& aNewData,
                                       nsACString& aNewMimeType)
{
  nsresult rv;
  

  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                reinterpret_cast<const char*>(aData), aDataLen,
                NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  // decode image
  nsCOMPtr<imgIContainer> container;
  rv = imgtool->DecodeImageData(stream, aMimeType, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  aNewMimeType.AssignLiteral("image/png");

  // scale and recompress
  nsCOMPtr<nsIInputStream> iconStream;
  rv = imgtool->EncodeScaledImage(container, aNewMimeType,
                                  mOptimizedIconDimension,
                                  mOptimizedIconDimension,
                                  getter_AddRefs(iconStream));
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the stream into a new buffer.
  rv = NS_ConsumeStream(iconStream, PR_UINT32_MAX, aNewData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFaviconService::FinalizeStatements() {
  mozIStorageStatement* stmts[] = {
    mDBGetURL,
    mDBGetData,
    mDBGetIconInfo,
    mDBInsertIcon,
    mDBUpdateIcon,
    mDBSetPageFavicon
  };

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsFaviconService::GetFaviconDataAsync(nsIURI* aFaviconURI,
                                      mozIStorageStatementCallback *aCallback)
{
  NS_ASSERTION(aCallback, "Doesn't make sense to call this without a callback");
  nsresult rv = BindStatementURI(mDBGetData, 0, aFaviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStatement;
  return mDBGetData->ExecuteAsync(aCallback, getter_AddRefs(pendingStatement));
}

void
nsFaviconService::checkAndNotify(nsIURI *aPageURI,
                                 nsIURI *aFaviconURI)
{
  // For page revisits (pretty common) we DON'T want to send out any
  // notifications if we've already set the favicon. These notifications will
  // cause parts of the UI to update and will be slow. This also saves us a
  // database write in these cases.
  nsCOMPtr<nsIURI> oldFavicon;
  PRBool faviconsEqual;
  if (NS_SUCCEEDED(GetFaviconForPage(aPageURI, getter_AddRefs(oldFavicon))) &&
      NS_SUCCEEDED(aFaviconURI->Equals(oldFavicon, &faviconsEqual)) &&
      faviconsEqual)
    return; // already set

  // This will associate the favicon URL with the page.
  PRBool hasData;
  nsresult rv = SetFaviconUrlForPageInternal(aPageURI, aFaviconURI, &hasData);
  if (NS_FAILED(rv))
    return;

  if (hasData) {
    SendFaviconNotifications(aPageURI, aFaviconURI);
    UpdateBookmarkRedirectFavicon(aPageURI, aFaviconURI);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// FaviconLoadListener

NS_IMPL_ISUPPORTS4(FaviconLoadListener,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIChannelEventSink)

FaviconLoadListener::FaviconLoadListener(nsIURI* aPageURI,
                                         nsIURI* aFaviconURI,
                                         nsIChannel* aChannel) :
  mChannel(aChannel),
  mPageURI(aPageURI),
  mFaviconURI(aFaviconURI)
{
}

FaviconLoadListener::~FaviconLoadListener()
{
}

NS_IMETHODIMP
FaviconLoadListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
FaviconLoadListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                                 nsresult aStatusCode)
{
  nsFaviconService *fs = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(fs, NS_ERROR_OUT_OF_MEMORY);

  if (NS_FAILED(aStatusCode) || mData.Length() == 0) {
    // load failed, add to failed cache
    fs->AddFailedFavicon(mFaviconURI);
    return NS_OK;
  }

  // sniff the MIME type
  nsresult rv;
  nsCOMPtr<nsICategoryManager> categoryManager =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISimpleEnumerator> sniffers;
  rv = categoryManager->EnumerateCategory(CONTENT_SNIFFING_SERVICES,
                                          getter_AddRefs(sniffers));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString mimeType;
  PRBool hasMore = PR_FALSE;
  while (mimeType.IsEmpty() && NS_SUCCEEDED(sniffers->HasMoreElements(&hasMore))
         && hasMore) {
    nsCOMPtr<nsISupports> snifferCIDSupports;
    rv = sniffers->GetNext(getter_AddRefs(snifferCIDSupports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsCString> snifferCIDSupportsCString =
      do_QueryInterface(snifferCIDSupports, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString snifferCID;
    rv = snifferCIDSupportsCString->GetData(snifferCID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContentSniffer> sniffer = do_GetService(snifferCID.get(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    sniffer->GetMIMETypeFromContent(
               aRequest,
               reinterpret_cast<PRUint8*>(const_cast<char*>(mData.get())),
               mData.Length(), mimeType);
    // ignore errors: mime type will be left empty and we'll try the next sniffer
  }

  if (mimeType.IsEmpty()) {
    // we can not handle favicons that do not have a recognisable MIME type
    fs->AddFailedFavicon(mFaviconURI);
    return NS_OK;
  }

  // Attempt to get an expiration time from the cache.  If this fails, we'll
  // make one up.
  PRTime expiration = -1;
  nsCOMPtr<nsICachingChannel> cachingChannel(do_QueryInterface(mChannel));
  if (cachingChannel) {
    nsCOMPtr<nsISupports> cacheToken;
    rv = cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICacheEntryInfo> cacheEntry(do_QueryInterface(cacheToken));
      PRUint32 seconds;
      rv = cacheEntry->GetExpirationTime(&seconds);
      if (NS_SUCCEEDED(rv)) {
        // Set the expiration, but make sure we honor our cap.
        expiration = PR_Now() + NS_MIN((PRTime)seconds * PR_USEC_PER_SEC,
                                       MAX_FAVICON_EXPIRATION);
      }
    }
  }
  // If we did not obtain a time from the cache, or it was negative, use our cap
  if (expiration < 0)
    expiration = PR_Now() + MAX_FAVICON_EXPIRATION;

  mozStorageTransaction transaction(fs->mDBConn, PR_FALSE);
  // save the favicon data
  // This could fail if the favicon is bigger than defined limit, in such a
  // case data will not be saved to the db but we will still continue.
  (void)fs->SetFaviconData(mFaviconURI,
                           reinterpret_cast<PRUint8*>(const_cast<char*>(mData.get())),
                           mData.Length(), mimeType, expiration);

  // set the favicon for the page
  PRBool hasData;
  rv = fs->SetFaviconUrlForPageInternal(mPageURI, mFaviconURI,
                                                     &hasData);
  NS_ENSURE_SUCCESS(rv, rv);

  fs->UpdateBookmarkRedirectFavicon(mPageURI, mFaviconURI);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  fs->SendFaviconNotifications(mPageURI, mFaviconURI);
  return NS_OK;
}

NS_IMETHODIMP
FaviconLoadListener::OnDataAvailable(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsIInputStream *aInputStream,
                                     PRUint32 aOffset,
                                     PRUint32 aCount)
{
  nsCString buffer;
  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv))
    return rv;

  mData.Append(buffer);
  return NS_OK;
}

NS_IMETHODIMP
FaviconLoadListener::GetInterface(const nsIID& uuid, void** aResult)
{
  return QueryInterface(uuid, aResult);
}

NS_IMETHODIMP
FaviconLoadListener::OnChannelRedirect(nsIChannel* oldChannel,
                                       nsIChannel* newChannel,
                                       PRUint32 flags)
{
  mChannel = newChannel;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// ExpireFaviconsStatementCallbackNotifier

NS_IMPL_ISUPPORTS1(ExpireFaviconsStatementCallbackNotifier,
                   mozIStorageStatementCallback)

ExpireFaviconsStatementCallbackNotifier::ExpireFaviconsStatementCallbackNotifier(bool *aFaviconsExpirationRunning)
  : mFaviconsExpirationRunning(aFaviconsExpirationRunning)
{
  NS_ASSERTION(mFaviconsExpirationRunning, "Pointer to bool mFaviconsExpirationRunning can't be null");
}

NS_IMETHODIMP
ExpireFaviconsStatementCallbackNotifier::HandleCompletion(PRUint16 aReason)
{
  *mFaviconsExpirationRunning = false;

  // We should dispatch only if expiration has been successful.
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
    return NS_OK;

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    (void)observerService->NotifyObservers(nsnull,
                                           NS_PLACES_FAVICONS_EXPIRED_TOPIC_ID,
                                           nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
ExpireFaviconsStatementCallbackNotifier::HandleResult(mozIStorageResultSet *aResultSet)
{
  NS_ASSERTION(PR_FALSE, "You cannot use this statement callback to get async statements resultset");
  return NS_OK;
}
