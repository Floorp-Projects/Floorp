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
#include "mozStorageHelper.h"

// This is the maximum favicon size that we will bother storing. Most icons
// are about 4K. Some people add 32x32 versions at different bit depths,
// making them much bigger. It would be nice if could extract just the 16x16
// version that we need. Instead, we'll just store everything below this
// sanity threshold.
#define MAX_FAVICON_SIZE 32768

#define FAVICON_BUFFER_INCREMENT 8192

#define MAX_FAVICON_CACHE_SIZE 512
#define FAVICON_CACHE_REDUCE_COUNT 64

#define CONTENT_SNIFFING_SERVICES "content-sniffing-services"


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

  nsCOMPtr<nsFaviconService> mFaviconService;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mPageURI;
  nsCOMPtr<nsIURI> mFaviconURI;

  nsCString mData;
};


nsFaviconService* nsFaviconService::gFaviconService;

NS_IMPL_ISUPPORTS1(nsFaviconService, nsIFaviconService)

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
  nsresult rv;

  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);
  mDBConn = historyService->GetStorageConnection();
  NS_ENSURE_TRUE(mDBConn, NS_ERROR_FAILURE);

  // creation of history service will have called InitTables before now

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id, length(data), expiration FROM moz_favicons WHERE url = ?1"),
                                getter_AddRefs(mDBGetIconInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT f.id, f.url, length(f.data), f.expiration FROM moz_places h JOIN moz_favicons f ON h.favicon_id = f.id WHERE h.url = ?1"),
                                getter_AddRefs(mDBGetURL));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT f.data, f.mime_type FROM moz_favicons f WHERE url = ?1"),
                                getter_AddRefs(mDBGetData));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_favicons (url, data, mime_type, expiration) VALUES (?1, ?2, ?3, ?4)"),
                                getter_AddRefs(mDBInsertIcon));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_favicons SET data = ?2, mime_type = ?3, expiration = ?4 where id = ?1"),
                                getter_AddRefs(mDBUpdateIcon));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_places SET favicon_id = ?2 WHERE id = ?1"),
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
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_favicons (id INTEGER PRIMARY KEY, url LONGVARCHAR UNIQUE, data BLOB, mime_type VARCHAR(32), expiration LONG)"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE INDEX moz_favicons_url ON moz_favicons (url)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsFaviconService::SetFaviconUrlForPage

NS_IMETHODIMP
nsFaviconService::SetFaviconUrlForPage(nsIURI* aPage, nsIURI* aFavicon)
{
  // we don't care whether there was data or what the expiration was
  PRBool hasData;
  PRTime expiration;
  nsresult rv = SetFaviconUrlForPageInternal(aPage, aFavicon,
                                             &hasData, &expiration);
  NS_ENSURE_SUCCESS(rv, rv);

  // send favicon change notifications if the URL has any data
  if (hasData)
    SendFaviconNotifications(aPage, aFavicon);
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
//    history service to associate the given favicon ID with the given URL. We
//    don't want to update the history table directly since that may involve
//    creating a new row in the history table, which should only be done by
//    history.
//
//    This sets aHasData if there was already icon data for this favicon. Used
//    to know if we should try reloading.
//
//    Expiration will be 0 if the icon has not been set yet.
//
//    Does NOT send out notifications. Caller should send out notifications
//    if the favicon has data.

nsresult
nsFaviconService::SetFaviconUrlForPageInternal(nsIURI* aPage, nsIURI* aFavicon,
                                               PRBool* aHasData,
                                               PRTime* aExpiration)
{
  mozStorageStatementScoper scoper(mDBGetIconInfo);
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  nsresult rv = BindStatementURI(mDBGetIconInfo, 0, aFavicon);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = mDBGetIconInfo->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 iconId;
  if (hasResult) {
    // have an entry for this icon already, just get the stats
    rv = mDBGetIconInfo->GetInt64(0, &iconId);
    NS_ENSURE_SUCCESS(rv, rv);

    // see if there's data already
    PRInt32 dataSize;
    rv = mDBGetIconInfo->GetInt32(1, &dataSize);
    NS_ENSURE_SUCCESS(rv, rv);
    if (dataSize > 0)
      *aHasData = PR_TRUE;

    // expiration
    rv = mDBGetIconInfo->GetInt64(2, aExpiration);
    NS_ENSURE_SUCCESS(rv, rv);

    mDBGetIconInfo->Reset();
    scoper.Abandon();
  } else {
    // create a new icon entry
    mDBGetIconInfo->Reset();
    scoper.Abandon();

    *aHasData = PR_FALSE;
    *aExpiration = 0;

    // no entry for this icon yet, create it
    mozStorageStatementScoper scoper2(mDBInsertIcon);
    rv = BindStatementURI(mDBInsertIcon, 0, aFavicon);
    NS_ENSURE_SUCCESS(rv, rv);
    mDBInsertIcon->BindNullParameter(1);
    mDBInsertIcon->BindNullParameter(2);
    mDBInsertIcon->BindNullParameter(3);
    rv = mDBInsertIcon->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->GetLastInsertRowID(&iconId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // now link our entry with the history service
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_NO_INTERFACE);

  PRInt64 pageId;
  rv = historyService->GetUrlIdFor(aPage, &pageId, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scoper2(mDBSetPageFavicon);
  rv = mDBSetPageFavicon->BindInt64Parameter(0, pageId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBSetPageFavicon->BindInt64Parameter(1, iconId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBSetPageFavicon->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  transaction.Commit();
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
nsFaviconService::UpdateBookmarkRedirectFavicon(nsIURI* aPage, nsIURI* aFavicon)
{
  NS_ENSURE_TRUE(aPage, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aFavicon, NS_ERROR_INVALID_ARG);

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIURI> bookmarkURI;
  nsresult rv = bookmarks->GetBookmarkedURIFor(aPage, getter_AddRefs(bookmarkURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (! bookmarkURI)
    return NS_OK; // no bookmark redirect

  PRBool sameAsBookmark;
  if (NS_SUCCEEDED(bookmarkURI->Equals(aPage, &sameAsBookmark)) && sameAsBookmark)
    return NS_OK; // bookmarked directly, not through a redirect

  PRBool hasData = PR_FALSE;
  PRTime expiration = 0;
  rv = SetFaviconUrlForPageInternal(bookmarkURI, aFavicon, &hasData, &expiration);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasData) {
    // send notifications
    SendFaviconNotifications(bookmarkURI, aFavicon);
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
nsFaviconService::SendFaviconNotifications(nsIURI* aPage, nsIURI* aFaviconURI)
{
  nsCAutoString faviconSpec;
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  if (historyService && NS_SUCCEEDED(aFaviconURI->GetSpec(faviconSpec))) {
    historyService->SendPageChangedNotification(aPage,
                                       nsINavHistoryObserver::ATTRIBUTE_FAVICON,
                                       NS_ConvertUTF8toUTF16(faviconSpec));
  }
}


// nsFaviconService::SetAndLoadFaviconForPage

NS_IMETHODIMP
nsFaviconService::SetAndLoadFaviconForPage(nsIURI* aPage, nsIURI* aFavicon,
                                           PRBool aForceReload)
{
#ifdef LAZY_ADD
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);
  return historyService->AddLazyLoadFaviconMessage(aPage, aFavicon,
                                                   aForceReload);
#else
  return DoSetAndLoadFaviconForPage(aPage, aFavicon, aForceReload);
#endif
}


// nsFaviconService::DoSetAndLoadFaviconForPage

nsresult
nsFaviconService::DoSetAndLoadFaviconForPage(nsIURI* aPage, nsIURI* aFavicon,
                                             PRBool aForceReload)
{
  nsCOMPtr<nsIURI> page(aPage);

  // don't load favicons when history is disabled
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_FAILURE);
  if (history->IsHistoryDisabled()) {
    // history is disabled - check to see if this favicon could be for a
    // bookmark
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI> bookmarkURI;
    nsresult rv = bookmarks->GetBookmarkedURIFor(aPage,
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
  nsresult rv = IsFailedFavicon(aFavicon, &previouslyFailed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (previouslyFailed) {
    if (aForceReload)
      RemoveFailedFavicon(aFavicon); // force reload clears from failed cache
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
  rv = page->Equals(aFavicon, &pageEqualsFavicon);
  NS_ENSURE_SUCCESS(rv, rv);
  if (pageEqualsFavicon)
    return NS_OK;

  // ignore data URL favicons. The main case here is the error page, which uses
  // a data URL as the favicon. The result is that we get the data URL in the
  // favicon table associated with the decoded version of the data URL.
  PRBool isDataURL;
  rv = aFavicon->SchemeIs("data", &isDataURL);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isDataURL)
    return NS_OK;

  // See if we have data and get the expiration time for this favicon. It might
  // be nice to say SetFaviconUrlForPageInternal here but we DON'T want to set
  // the favicon for the page unless we know we have it. For example, if I go
  // to random site x.com, the browser will still tell us that the favicon is
  // x.com/favicon.ico even if there is no such file. We don't want to pollute
  // our tables with this useless data.
  PRBool hasData = PR_FALSE;
  PRTime expiration = 0;
  { // scope for statement
    mozStorageStatementScoper scoper(mDBGetIconInfo);
    rv = BindStatementURI(mDBGetIconInfo, 0, aFavicon);
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
        NS_SUCCEEDED(aFavicon->Equals(oldFavicon, &faviconsEqual)) &&
        faviconsEqual)
      return NS_OK; // already set

    // This will associate the favicon URL with the page.
    rv = SetFaviconUrlForPageInternal(page, aFavicon, &hasData, &expiration);
    NS_ENSURE_SUCCESS(rv, rv);

    SendFaviconNotifications(page, aFavicon);
    UpdateBookmarkRedirectFavicon(page, aFavicon);
    return NS_OK;
  }

  nsCOMPtr<nsIIOService> ioservice = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = ioservice->NewChannelFromURI(aFavicon, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamListener> listener =
    new FaviconLoadListener(this, page, aFavicon, channel);
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
nsFaviconService::SetFaviconData(nsIURI* aFavicon, const PRUint8* aData,
                                 PRUint32 aDataLen, const nsACString& aMimeType,
                                 PRTime aExpiration)
{
  nsresult rv;
  mozIStorageStatement* statement;
  {
    // this block forces the scoper to reset our statement: necessary for the
    // next statement
    mozStorageStatementScoper scoper(mDBGetIconInfo);
    rv = BindStatementURI(mDBGetIconInfo, 0, aFavicon);
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
      rv = BindStatementURI(statement, 0, aFavicon);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageStatementScoper scoper(statement);

  // the insert and update statements share all but the 0th parameter
  rv = statement->BindBlobParameter(1, aData, aDataLen);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringParameter(2, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(3, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);
  return statement->Execute();
}


// nsFaviconService::GetFaviconData

NS_IMETHODIMP
nsFaviconService::GetFaviconData(nsIURI* aFavicon, nsACString& aMimeType,
                                 PRUint32* aDataLen, PRUint8** aData)
{
  mozStorageStatementScoper scoper(mDBGetData);
  nsresult rv = BindStatementURI(mDBGetData, 0, aFavicon);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = mDBGetData->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! hasResult)
    return NS_ERROR_NOT_AVAILABLE;

  rv = mDBGetData->GetUTF8String(1, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  return mDBGetData->GetBlob(0, aDataLen, aData);
}


// nsFaviconService::GetFaviconForPage

NS_IMETHODIMP
nsFaviconService::GetFaviconForPage(nsIURI* aPage, nsIURI** _retval)
{
  mozStorageStatementScoper scoper(mDBGetURL);
  nsresult rv = BindStatementURI(mDBGetURL, 0, aPage);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = mDBGetURL->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! hasResult)
    return NS_ERROR_NOT_AVAILABLE;

  nsCAutoString url;
  rv = mDBGetURL->GetUTF8String(1, url);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewURI(_retval, url);
}


// nsFaviconService::GetFaviconImageForPage

NS_IMETHODIMP
nsFaviconService::GetFaviconImageForPage(nsIURI* aPage, nsIURI** _retval)
{
  mozStorageStatementScoper scoper(mDBGetURL);
  nsresult rv = BindStatementURI(mDBGetURL, 0, aPage);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = mDBGetURL->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> faviconURI;
  if (hasResult)
  {
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
nsFaviconService::GetFaviconLinkForIcon(nsIURI* aIcon, nsIURI** aOutput)
{
  nsCAutoString spec;
  if (aIcon) {
    nsresult rv = aIcon->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return GetFaviconLinkForIconString(spec, aOutput);
}


// nsFaviconService::AddFailedFavicon

PR_STATIC_CALLBACK(PLDHashOperator)
ExpireFailedFaviconsCallback(nsCStringHashKey::KeyType aKey,
                             PRUint32& aData,
                             void* userArg)
{
  PRUint32* threshold = NS_REINTERPRET_CAST(PRUint32*, userArg);
  if (aData < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsFaviconService::AddFailedFavicon(nsIURI* aIcon)
{
  nsCAutoString spec;
  nsresult rv = aIcon->GetSpec(spec);
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
nsFaviconService::RemoveFailedFavicon(nsIURI* aIcon)
{
  nsCAutoString spec;
  nsresult rv = aIcon->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // we silently do nothing and succeed if the icon is not in the cache
  mFailedFavicons.Remove(spec);
  return NS_OK;
}


// nsFaviconService::IsFailedFavicon

NS_IMETHODIMP
nsFaviconService::IsFailedFavicon(nsIURI* aIcon, PRBool* _retval)
{
  nsCAutoString spec;
  nsresult rv = aIcon->GetSpec(spec);
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
nsFaviconService::GetFaviconLinkForIconString(const nsCString& aSpec, nsIURI** aOutput)
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
nsFaviconService::GetFaviconSpecForIconString(const nsCString& aSpec, nsACString& aOutput)
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


NS_IMPL_ISUPPORTS3(FaviconLoadListener,
                   nsIStreamListener, // is a nsIRequestObserver
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
#ifndef MOZILLA_1_8_BRANCH
               aRequest,
#endif
               NS_REINTERPRET_CAST(PRUint8*, NS_CONST_CAST(char*, mData.get())),
               mData.Length(), mimeType);
    // ignore errors: mime type will be left empty and we'll try the next sniffer
  }

  if (mimeType.IsEmpty()) {
    // we can not handle favicons that do not have a recognisable MIME type
    mFaviconService->AddFailedFavicon(mFaviconURI);
    return NS_OK;
  }

  // Expire this favicon in one day. An old version of this actually extracted
  // the expiration time from the cache. But what if people (especially web
  // developers) get a bad favicon or change it? The problem is that we're not
  // aware when the icon has been reloaded in the cache or cleared. This way
  // we'll always pick up any changes in the cache after a day. In most cases
  // re-set favicons will come from the cache anyway and reloading them is not
  // very expensive.
  PRTime expiration = PR_Now() +
                      (PRInt64)(24 * 60 * 60) * (PRInt64)PR_USEC_PER_SEC;

  // save the favicon data
  rv = mFaviconService->SetFaviconData(mFaviconURI,
               NS_REINTERPRET_CAST(PRUint8*, NS_CONST_CAST(char*, mData.get())),
               mData.Length(), mimeType, expiration);
  NS_ENSURE_SUCCESS(rv, rv);

  // set the favicon for the page
  PRBool hasData;
  rv = mFaviconService->SetFaviconUrlForPageInternal(mPageURI, mFaviconURI,
                                                     &hasData, &expiration);
  NS_ENSURE_SUCCESS(rv, rv);

  mFaviconService->SendFaviconNotifications(mPageURI, mFaviconURI);
  mFaviconService->UpdateBookmarkRedirectFavicon(mPageURI, mFaviconURI);
  return NS_OK;
}


// FaviconLoadListener::OnDataAvailable (nsIStreamListener)

NS_IMETHODIMP
FaviconLoadListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                  nsIInputStream *aInputStream,
                                  PRUint32 aOffset, PRUint32 aCount)
{
  if (aOffset + aCount > MAX_FAVICON_SIZE)
    return NS_ERROR_FAILURE; // too big

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
                                       nsIChannel* newChannel, PRUint32 flags)
{
  mChannel = newChannel;
  return NS_OK;
}


