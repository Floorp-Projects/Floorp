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
#include "mozStorageHelper.h"
#include "plbase64.h"
#include "nsPlacesTables.h"
#include "mozIStoragePendingStatement.h"

// For favicon optimization
#include "imgITools.h"
#include "imgIContainer.h"

#define FAVICON_BUFFER_INCREMENT 8192

#define MAX_FAVICON_CACHE_SIZE 512
#define FAVICON_CACHE_REDUCE_COUNT 64

#define CONTENT_SNIFFING_SERVICES "content-sniffing-services"

// If favicon is bigger than this size we will try to optimize it into a
// 16x16 png. An uncompressed 16x16 RGBA image is 1024 bytes, and almost all
// sensible 16x16 icons are under 1024 bytes.
#define OPTIMIZED_FAVICON_SIZE 1024

// Favicons bigger than this size should not be saved to the db to avoid
// bloating it with large image blobs.
// This still allows us to accept a favicon even if we cannot optimize it.
#define MAX_FAVICON_SIZE 10240

/**
 * The maximum time we will keep a favicon around.  We always ask the cache, if
 * we can, but default to this value if we do not get a time back, or the time
 * is more in the future than this.
 * Currently set to one week from now.
 */
#define MAX_FAVICON_EXPIRATION PRTime(7 * 24 * 60 * 60 * PR_USEC_PER_SEC)

class FaviconLoadListener : public nsIStreamListener,
                            public nsIInterfaceRequestor,
                            public nsIChannelEventSink
{
public:
  FaviconLoadListener(nsFaviconService* aFaviconService,
                      nsIURI* aPageURI, nsIURI* aFaviconURI,
                      nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

private:
  ~FaviconLoadListener();

  nsRefPtr<nsFaviconService> mFaviconService;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mPageURI;
  nsCOMPtr<nsIURI> mFaviconURI;

  nsCString mData;
};


nsFaviconService* nsFaviconService::gFaviconService;

NS_IMPL_ISUPPORTS2(
  nsFaviconService
, nsIFaviconService
, nsIObserver
)

// nsFaviconService::nsFaviconService

nsFaviconService::nsFaviconService() : mFailedFaviconSerial(0)
{
  NS_ASSERTION(! gFaviconService, "ATTEMPTING TO CREATE TWO FAVICON SERVICES!");
  gFaviconService = this;
}


// nsFaviconService::~nsFaviconService

nsFaviconService::~nsFaviconService()
{
  NS_ASSERTION(gFaviconService == this, "Deleting a non-singleton favicon service");
  if (gFaviconService == this)
    gFaviconService = nsnull;
}


// nsFaviconService::Init
//
//    Called when the service is created.

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
        "SELECT * FROM moz_places_temp "
        "WHERE url = ?1 "
        "UNION ALL "
        "SELECT * FROM moz_places "
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
  if (! mFailedFavicons.Init(256))
    return NS_ERROR_OUT_OF_MEMORY;

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

nsresult
nsFaviconService::ExpireAllFavicons()
{
  // Remove all references to favicons.  We would love to not have to do this
  // because it could result in copying almost all of the moz_places into
  // memory for some sync in the future.  We mitigate this some by checking for
  // non-null favicon ids, but views do not use indexes when querying.
  nsCOMPtr<mozIStorageStatement> removeReferences;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view "
      "SET favicon_id = NULL "
      "WHERE favicon_id <> NULL"
    ), getter_AddRefs(removeReferences));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove all favicons
  nsCOMPtr<mozIStorageStatement> removeFavicons;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_favicons"
    ), getter_AddRefs(removeFavicons));
  NS_ENSURE_SUCCESS(rv, rv);

  mozIStorageStatement *stmts[] = {
    removeReferences,
    removeFavicons
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts), nsnull,
                             getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsFaviconService::Observe(nsISupports *aSubject, const char *aTopic,
                          const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_CACHESERVICE_EMPTYCACHE_TOPIC_ID) == 0)
    (void)ExpireAllFavicons();

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIFaviconService

// nsFaviconService::SetFaviconUrlForPage

NS_IMETHODIMP
nsFaviconService::SetFaviconUrlForPage(nsIURI* aPageURI, nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG_POINTER(aPageURI);
  NS_ENSURE_ARG_POINTER(aFaviconURI);

  // we don't care if there was data
  PRBool hasData;
  nsresult rv = SetFaviconUrlForPageInternal(aPageURI, aFaviconURI, &hasData);
  NS_ENSURE_SUCCESS(rv, rv);

  // send favicon change notifications if the URL has any data
  if (hasData)
    SendFaviconNotifications(aPageURI, aFaviconURI);
  return NS_OK;
}

// nsFaviconService::GetDefaultFavicon

NS_IMETHODIMP
nsFaviconService::GetDefaultFavicon(nsIURI** _retval)
{
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

  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);

  if (historyService->InPrivateBrowsingMode())
    return NS_OK;

  if (iconId == -1) {
    // We did not find any entry, so create a new one
    *aHasData = PR_FALSE;

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
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_UNEXPECTED);

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


// nsFaviconService::SetAndLoadFaviconForPage

NS_IMETHODIMP
nsFaviconService::SetAndLoadFaviconForPage(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI,
                                           PRBool aForceReload)
{
  NS_ENSURE_ARG_POINTER(aPageURI);
  NS_ENSURE_ARG_POINTER(aFaviconURI);

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


// nsFaviconService::DoSetAndLoadFaviconForPage

nsresult
nsFaviconService::DoSetAndLoadFaviconForPage(nsIURI* aPageURI,
                                             nsIURI* aFaviconURI,
                                             PRBool aForceReload)
{
  nsCOMPtr<nsIURI> page(aPageURI);

  // don't load favicons when history is disabled
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_FAILURE);
  if (history->IsHistoryDisabled()) {
    // history is disabled - check to see if this favicon could be for a
    // bookmark
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI> bookmarkURI;
    nsresult rv = bookmarks->GetBookmarkedURIFor(aPageURI,
                                                 getter_AddRefs(bookmarkURI));
    NS_ENSURE_SUCCESS(rv, rv);
    if (! bookmarkURI) {
      // page is not bookmarked, don't save favicon
      return NS_OK;
    }

    // page is bookmarked, set the URI to be the bookmark, the bookmark URI
    // might be different than our input URI if there was a redirect, so
    // always use the bookmark URI here to avoid setting the favicon for
    // non-bookmarked pages.
    page = bookmarkURI;
  }

  // check the failed favicon cache
  PRBool previouslyFailed;
  nsresult rv = IsFailedFavicon(aFaviconURI, &previouslyFailed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (previouslyFailed) {
    if (aForceReload)
      RemoveFailedFavicon(aFaviconURI); // force reload clears from failed cache
    else
      return NS_OK; // ignore previously failed favicons
  }

  // filter out bad URLs
  PRBool canAdd;
  rv = history->CanAddURI(page, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! canAdd)
    return NS_OK; // ignore favicons for this url

  // If have an image loaded in the main frame, that image will get set as its
  // own favicon. It would be nice to store a resampled version of the image,
  // but that's prohibitive for now. This workaround just refuses to save the
  // favicon in this case.
  PRBool pageEqualsFavicon;
  rv = page->Equals(aFaviconURI, &pageEqualsFavicon);
  NS_ENSURE_SUCCESS(rv, rv);
  if (pageEqualsFavicon)
    return NS_OK;

  // ignore error page favicons.
  nsCOMPtr<nsIURI> errorPageFavicon;
  rv = NS_NewURI(getter_AddRefs(errorPageFavicon),
                 NS_LITERAL_CSTRING(FAVICON_ERRORPAGE_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool isErrorPage;
  rv = aFaviconURI->Equals(errorPageFavicon, &isErrorPage);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isErrorPage)
    return NS_OK;

  // See if we have data and get the expiration time for this favicon.  We DO
  // NOT want to set the favicon for the page unless we know we have it.  For
  // example, if I go to random site x.com, the browser will still tell us that
  // the favicon is x.com/favicon.ico even if there is no such file.  We don't
  // want to pollute our tables with this useless data.
  PRBool hasData = PR_FALSE;
  PRTime expiration = 0;
  { // scope for statement
    mozStorageStatementScoper scoper(mDBGetIconInfo);
    rv = BindStatementURI(mDBGetIconInfo, 0, aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMatch;
    rv = mDBGetIconInfo->ExecuteStep(&hasMatch);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasMatch) {
      PRInt32 dataSize;
      mDBGetIconInfo->GetInt32(1, &dataSize);
      hasData = dataSize > 0;
      mDBGetIconInfo->GetInt64(2, &expiration);
    }
  }

  // See if this favicon has expired yet. We don't want to waste time reloading
  // from the web or cache if we have a recent version.
  PRTime now = PR_Now();
  if (hasData && now < expiration && ! aForceReload) {
    // data still valid, no need to reload

    // For page revisits (pretty common) we DON'T want to send out any
    // notifications if we've already set the favicon. These notifications will
    // cause parts of the UI to update and will be slow. This also saves us a
    // database write in these cases.
    nsCOMPtr<nsIURI> oldFavicon;
    PRBool faviconsEqual;
    if (NS_SUCCEEDED(GetFaviconForPage(page, getter_AddRefs(oldFavicon))) &&
        NS_SUCCEEDED(aFaviconURI->Equals(oldFavicon, &faviconsEqual)) &&
        faviconsEqual)
      return NS_OK; // already set

    // This will associate the favicon URL with the page.
    rv = SetFaviconUrlForPageInternal(page, aFaviconURI, &hasData);
    NS_ENSURE_SUCCESS(rv, rv);

    SendFaviconNotifications(page, aFaviconURI);
    UpdateBookmarkRedirectFavicon(page, aFaviconURI);
    return NS_OK;
  }

  nsCOMPtr<nsIIOService> ioservice = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = ioservice->NewChannelFromURI(aFaviconURI, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamListener> listener =
    new FaviconLoadListener(this, page, aFaviconURI, channel);
  NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr<nsIInterfaceRequestor> listenerRequestor =
    do_QueryInterface(listener, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetNotificationCallbacks(listenerRequestor);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = channel->AsyncOpen(listener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // DB will be update and observers will be notified when the data has
  // finished loading
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
  nsresult rv;
  PRUint32 dataLen = aDataLen;
  const PRUint8* data = aData;
  const nsACString* mimeType = &aMimeType;
  nsCString newData, newMimeType;

  // If the page provided a large image for the favicon (eg, a highres image
  // or a multiresolution .ico file), we don't want to store more data than
  // needed.
  if (aDataLen > OPTIMIZED_FAVICON_SIZE) {
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


// nsFaviconService::SetFaviconDataFromDataURL

NS_IMETHODIMP
nsFaviconService::SetFaviconDataFromDataURL(nsIURI* aFaviconURI,
                                            const nsAString& aDataURL,
                                            PRTime aExpiration)
{
  nsresult rv;

  nsCOMPtr<nsIURI> dataURI;
  rv = NS_NewURI(getter_AddRefs(dataURI), aDataURL);
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


// nsFaviconService::GetFaviconData

NS_IMETHODIMP
nsFaviconService::GetFaviconData(nsIURI* aFaviconURI, nsACString& aMimeType,
                                 PRUint32* aDataLen, PRUint8** aData)
{
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


// nsFaviconService::GetFaviconDataAsDataURL

NS_IMETHODIMP
nsFaviconService::GetFaviconDataAsDataURL(nsIURI* aFaviconURI,
                                          nsAString& aDataURL)
{
  nsresult rv;

  PRUint8* data;
  PRUint32 dataLen;
  nsCAutoString mimeType;

  rv = GetFaviconData(aFaviconURI, mimeType, &dataLen, &data);
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


// nsFaviconService::GetFaviconForPage

NS_IMETHODIMP
nsFaviconService::GetFaviconForPage(nsIURI* aPageURI, nsIURI** _retval)
{
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


// nsFaviconService::GetFaviconImageForPage

NS_IMETHODIMP
nsFaviconService::GetFaviconImageForPage(nsIURI* aPageURI, nsIURI** _retval)
{
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


// nsFaviconService::GetFaviconLinkForIcon

nsresult
nsFaviconService::GetFaviconLinkForIcon(nsIURI* aFaviconURI,
                                        nsIURI** aOutputURI)
{
  nsCAutoString spec;
  if (aFaviconURI) {
    nsresult rv = aFaviconURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return GetFaviconLinkForIconString(spec, aOutputURI);
}


// nsFaviconService::AddFailedFavicon

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


// nsFaviconService::RemoveFailedFavicon

NS_IMETHODIMP
nsFaviconService::RemoveFailedFavicon(nsIURI* aFaviconURI)
{
  nsCAutoString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // we silently do nothing and succeed if the icon is not in the cache
  mFailedFavicons.Remove(spec);
  return NS_OK;
}


// nsFaviconService::IsFailedFavicon

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
  rv = imgtool->EncodeScaledImage(container, aNewMimeType, 16, 16,
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

NS_IMPL_ISUPPORTS4(FaviconLoadListener,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIChannelEventSink)

// FaviconLoadListener::FaviconLoadListener

FaviconLoadListener::FaviconLoadListener(nsFaviconService* aFaviconService,
                                         nsIURI* aPageURI, nsIURI* aFaviconURI,
                                         nsIChannel* aChannel) :
  mFaviconService(aFaviconService),
  mChannel(aChannel),
  mPageURI(aPageURI),
  mFaviconURI(aFaviconURI)
{
}


// FaviconLoadListener::~FaviconLoadListener

FaviconLoadListener::~FaviconLoadListener()
{
}


// FaviconLoadListener::OnStartRequest (nsIRequestObserver)

NS_IMETHODIMP
FaviconLoadListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}


// FaviconLoadListener::OnStopRequest (nsIRequestObserver)

NS_IMETHODIMP
FaviconLoadListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                                 nsresult aStatusCode)
{
  if (NS_FAILED(aStatusCode) || mData.Length() == 0) {
    // load failed, add to failed cache
    mFaviconService->AddFailedFavicon(mFaviconURI);
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
    mFaviconService->AddFailedFavicon(mFaviconURI);
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
        expiration = PR_Now() + PR_MIN(seconds * PR_USEC_PER_SEC,
                                       MAX_FAVICON_EXPIRATION);
      }
    }
  }
  // If we did not obtain a time from the cache, or it was negative, use our cap
  if (expiration < 0)
    expiration = PR_Now() + MAX_FAVICON_EXPIRATION;

  mozStorageTransaction transaction(mFaviconService->mDBConn, PR_FALSE);
  // save the favicon data
  // This could fail if the favicon is bigger than defined limit, in such a
  // case data will not be saved to the db but we will still continue.
  (void) mFaviconService->SetFaviconData(mFaviconURI,
               reinterpret_cast<PRUint8*>(const_cast<char*>(mData.get())),
               mData.Length(), mimeType, expiration);

  // set the favicon for the page
  PRBool hasData;
  rv = mFaviconService->SetFaviconUrlForPageInternal(mPageURI, mFaviconURI,
                                                     &hasData);
  NS_ENSURE_SUCCESS(rv, rv);

  mFaviconService->UpdateBookmarkRedirectFavicon(mPageURI, mFaviconURI);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  mFaviconService->SendFaviconNotifications(mPageURI, mFaviconURI);
  return NS_OK;
}


// FaviconLoadListener::OnDataAvailable (nsIStreamListener)

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


// FaviconLoadListener::GetInterface (nsIInterfaceRequestor)

NS_IMETHODIMP
FaviconLoadListener::GetInterface(const nsIID& uuid, void** aResult)
{
  return QueryInterface(uuid, aResult);
}


// FaviconLoadListener::OnChannelRedirect (nsIChannelEventSink)

NS_IMETHODIMP
FaviconLoadListener::OnChannelRedirect(nsIChannel* oldChannel,
                                       nsIChannel* newChannel,
                                       PRUint32 flags)
{
  mChannel = newChannel;
  return NS_OK;
}


