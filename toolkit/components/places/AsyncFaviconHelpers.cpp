/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncFaviconHelpers.h"

#include "nsICacheService.h"
#include "nsICacheVisitor.h"
#include "nsICachingChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"

#include "nsNavHistory.h"
#include "nsFaviconService.h"
#include "mozilla/storage.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "nsIPrivateBrowsingChannel.h"
#include <algorithm>

using namespace mozilla::places;
using namespace mozilla::storage;

namespace mozilla {
namespace places {

namespace {

/**
 * Fetches information on a page from the Places database.
 *
 * @param aDBConn
 *        Database connection to history tables.
 * @param _page
 *        Page that should be fetched.
 */
nsresult
FetchPageInfo(nsRefPtr<Database>& aDB,
              PageData& _page)
{
  NS_PRECONDITION(_page.spec.Length(), "Must have a non-empty spec!");
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  // This query finds the bookmarked uri we want to set the icon for,
  // walking up to two redirect levels.
  nsCString query = nsPrintfCString(
    "SELECT h.id, h.favicon_id, h.guid, ( "
      "SELECT h.url FROM moz_bookmarks b WHERE b.fk = h.id "
      "UNION ALL " // Union not directly bookmarked pages.
      "SELECT url FROM moz_places WHERE id = ( "
        "SELECT COALESCE(grandparent.place_id, parent.place_id) as r_place_id "
        "FROM moz_historyvisits dest "
        "LEFT JOIN moz_historyvisits parent ON parent.id = dest.from_visit "
                                          "AND dest.visit_type IN (%d, %d) "
        "LEFT JOIN moz_historyvisits grandparent ON parent.from_visit = grandparent.id "
          "AND parent.visit_type IN (%d, %d) "
        "WHERE dest.place_id = h.id "
        "AND EXISTS(SELECT 1 FROM moz_bookmarks b WHERE b.fk = r_place_id) "
        "LIMIT 1 "
      ") "
    ") FROM moz_places h WHERE h.url = :page_url",
    nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY,
    nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY
  );

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(query);
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                                _page.spec);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult) {
    // The page does not exist.
    return NS_ERROR_NOT_AVAILABLE;
  }

  rv = stmt->GetInt64(0, &_page.id);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNull;
  rv = stmt->GetIsNull(1, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  // favicon_id can be NULL.
  if (!isNull) {
    rv = stmt->GetInt64(1, &_page.iconId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->GetUTF8String(2, _page.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetIsNull(3, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  // The page could not be bookmarked.
  if (!isNull) {
    rv = stmt->GetUTF8String(3, _page.bookmarkedSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!_page.canAddToHistory) {
    // Either history is disabled or the scheme is not supported.  In such a
    // case we want to update the icon only if the page is bookmarked.

    if (_page.bookmarkedSpec.IsEmpty()) {
      // The page is not bookmarked.  Since updating the icon with a disabled
      // history would be a privacy leak, bail out as if the page did not exist.
      return NS_ERROR_NOT_AVAILABLE;
    }
    else {
      // The page, or a redirect to it, is bookmarked.  If the bookmarked spec
      // is different from the requested one, use it.
      if (!_page.bookmarkedSpec.Equals(_page.spec)) {
        _page.spec = _page.bookmarkedSpec;
        rv = FetchPageInfo(aDB, _page);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

/**
 * Stores information on a icon in the database.
 *
 * @param aDBConn
 *        Database connection to history tables.
 * @param aIcon
 *        Icon that should be stored.
 */
nsresult
SetIconInfo(nsRefPtr<Database>& aDB,
            IconData& aIcon)
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  // The 'multi-coalesce' here ensures that replacing a favicon without
  // specifying a :guid parameter doesn't cause it to be allocated a new
  // GUID.
  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "INSERT OR REPLACE INTO moz_favicons "
      "(id, url, data, mime_type, expiration, guid) "
    "VALUES ((SELECT id FROM moz_favicons WHERE url = :icon_url), "
            ":icon_url, :data, :mime_type, :expiration, "
            "COALESCE(:guid, "
                     "(SELECT guid FROM moz_favicons "
                      "WHERE url = :icon_url), "
                     "GENERATE_GUID()))"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aIcon.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindBlobByName(NS_LITERAL_CSTRING("data"),
                            TO_INTBUFFER(aIcon.data), aIcon.data.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("mime_type"), aIcon.mimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("expiration"), aIcon.expiration);
  NS_ENSURE_SUCCESS(rv, rv);

  // Binding a GUID allows us to override the current (or generated) GUID.
  if (aIcon.guid.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("guid"));
  }
  else {
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), aIcon.guid);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * Fetches information on a icon from the Places database.
 *
 * @param aDBConn
 *        Database connection to history tables.
 * @param _icon
 *        Icon that should be fetched.
 */
nsresult
FetchIconInfo(nsRefPtr<Database>& aDB,
              IconData& _icon)
{
  NS_PRECONDITION(_icon.spec.Length(), "Must have a non-empty spec!");
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  if (_icon.status & ICON_STATUS_CACHED) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "SELECT id, expiration, data, mime_type "
    "FROM moz_favicons WHERE url = :icon_url"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"),
                                _icon.spec);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult) {
    // The icon does not exist yet, bail out.
    return NS_OK;
  }

  rv = stmt->GetInt64(0, &_icon.id);
  NS_ENSURE_SUCCESS(rv, rv);

  // Expiration can be NULL.
  bool isNull;
  rv = stmt->GetIsNull(1, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetInt64(1, reinterpret_cast<int64_t*>(&_icon.expiration));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Data can be NULL.
  rv = stmt->GetIsNull(2, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    uint8_t* data;
    uint32_t dataLen = 0;
    rv = stmt->GetBlob(2, &dataLen, &data);
    NS_ENSURE_SUCCESS(rv, rv);
    _icon.data.Adopt(TO_CHARBUFFER(data), dataLen);
    // Read mime only if we have data.
    rv = stmt->GetUTF8String(3, _icon.mimeType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
FetchIconURL(nsRefPtr<Database>& aDB,
             const nsACString& aPageSpec,
             nsACString& aIconSpec)
{
  NS_PRECONDITION(!aPageSpec.IsEmpty(), "Page spec must not be empty.");
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread.");

  aIconSpec.Truncate();

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "SELECT f.url "
    "FROM moz_places h "
    "JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url = :page_url"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                                aPageSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    rv = stmt->GetUTF8String(0, aIconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/**
 * Tries to compute the expiration time for a icon from the channel.
 *
 * @param aChannel
 *        The network channel used to fetch the icon.
 * @return a valid expiration value for the fetched icon.
 */
PRTime
GetExpirationTimeFromChannel(nsIChannel* aChannel)
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  // Attempt to get an expiration time from the cache.  If this fails, we'll
  // make one up.
  PRTime expiration = -1;
  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(aChannel);
  if (cachingChannel) {
    nsCOMPtr<nsISupports> cacheToken;
    nsresult rv = cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICacheEntryInfo> cacheEntry = do_QueryInterface(cacheToken);
      uint32_t seconds;
      rv = cacheEntry->GetExpirationTime(&seconds);
      if (NS_SUCCEEDED(rv)) {
        // Set the expiration, but make sure we honor our cap.
        expiration = PR_Now() + std::min((PRTime)seconds * PR_USEC_PER_SEC,
                                       MAX_FAVICON_EXPIRATION);
      }
    }
  }
  // If we did not obtain a time from the cache, use the cap value.
  return expiration < 0 ? PR_Now() + MAX_FAVICON_EXPIRATION
                        : expiration;
}

/**
 * Checks the icon and evaluates if it needs to be optimized.  In such a case it
 * will try to reduce its size through OptimizeFaviconImage method of the
 * favicons service.
 *
 * @param aIcon
 *        The icon to be evaluated.
 * @param aFaviconSvc
 *        Pointer to the favicons service.
 */
nsresult
OptimizeIconSize(IconData& aIcon,
                 nsFaviconService* aFaviconSvc)
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  // Even if the page provides a large image for the favicon (eg, a highres
  // image or a multiresolution .ico file), don't try to store more data than
  // needed.
  nsAutoCString newData, newMimeType;
  if (aIcon.data.Length() > MAX_ICON_FILESIZE(aFaviconSvc->GetOptimizedIconDimension())) {
    nsresult rv = aFaviconSvc->OptimizeFaviconImage(TO_INTBUFFER(aIcon.data),
                                                    aIcon.data.Length(),
                                                    aIcon.mimeType,
                                                    newData,
                                                    newMimeType);
    if (NS_SUCCEEDED(rv) && newData.Length() < aIcon.data.Length()) {
      aIcon.data = newData;
      aIcon.mimeType = newMimeType;
    }
  }
  return NS_OK;
}

} // Anonymous namespace.


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconHelperBase

AsyncFaviconHelperBase::AsyncFaviconHelperBase(
  nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : mDB(Database::GetDatabase())
{
  // Don't AddRef or Release in runnables for thread-safety.
  mCallback.swap(aCallback);
}

AsyncFaviconHelperBase::~AsyncFaviconHelperBase()
{
  nsCOMPtr<nsIThread> thread;
  (void)NS_GetMainThread(getter_AddRefs(thread));
  if (mCallback) {
    (void)NS_ProxyRelease(thread, mCallback, true);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncFetchAndSetIconForPage

// static
nsresult
AsyncFetchAndSetIconForPage::start(nsIURI* aFaviconURI,
                                   nsIURI* aPageURI,
                                   enum AsyncFaviconFetchMode aFetchMode,
                                   uint32_t aFaviconLoadType,
                                   nsIFaviconDataCallback* aCallback)
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  PageData page;
  nsresult rv = aPageURI->GetSpec(page.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  // URIs can arguably miss a host.
  (void)GetReversedHostname(aPageURI, page.revHost);
  bool canAddToHistory;
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);
  rv = navHistory->CanAddURI(aPageURI, &canAddToHistory);
  NS_ENSURE_SUCCESS(rv, rv);
  page.canAddToHistory = !!canAddToHistory && aFaviconLoadType != nsIFaviconService::FAVICON_LOAD_PRIVATE;

  IconData icon;

  nsFaviconService* favicons = nsFaviconService::GetFaviconService();
  NS_ENSURE_STATE(favicons);

  UnassociatedIconHashKey* iconKey =
    favicons->mUnassociatedIcons.GetEntry(aFaviconURI);

  if (iconKey) {
    icon = iconKey->iconData;
    favicons->mUnassociatedIcons.RemoveEntry(aFaviconURI);
  } else {
    icon.fetchMode = aFetchMode;
    rv = aFaviconURI->GetSpec(icon.spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If the page url points to an image, the icon's url will be the same.
  // In future evaluate to store a resample of the image.  For now avoid that
  // for database size concerns.
  // Don't store favicons for error pages too.
  if (icon.spec.Equals(page.spec) ||
      icon.spec.Equals(FAVICON_ERRORPAGE_URL)) {
    return NS_OK;
  }

  // The event will swap owning pointers, thus we need a new pointer.
  nsCOMPtr<nsIFaviconDataCallback> callback(aCallback);
  nsRefPtr<AsyncFetchAndSetIconForPage> event =
    new AsyncFetchAndSetIconForPage(icon, page, aFaviconLoadType, callback);

  // Get the target thread and start the work.
  nsRefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

AsyncFetchAndSetIconForPage::AsyncFetchAndSetIconForPage(
  IconData& aIcon
, PageData& aPage
, uint32_t aFaviconLoadType
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : AsyncFaviconHelperBase(aCallback)
  , mIcon(aIcon)
  , mPage(aPage)
  , mFaviconLoadPrivate(aFaviconLoadType == nsIFaviconService::FAVICON_LOAD_PRIVATE)
{
}

AsyncFetchAndSetIconForPage::~AsyncFetchAndSetIconForPage()
{
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  // Try to fetch the icon from the database.
  nsresult rv = FetchIconInfo(mDB, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInvalidIcon = mIcon.data.IsEmpty() ||
                       (mIcon.expiration && PR_Now() > mIcon.expiration);
  bool fetchIconFromNetwork = mIcon.fetchMode == FETCH_ALWAYS ||
                              (mIcon.fetchMode == FETCH_IF_MISSING && isInvalidIcon);

  if (!fetchIconFromNetwork) {
    // There is already a valid icon or we don't want to fetch a new one,
    // directly proceed with association.
    nsRefPtr<AsyncAssociateIconToPage> event =
        new AsyncAssociateIconToPage(mIcon, mPage, mCallback);
    mDB->DispatchToAsyncThread(event);

    return NS_OK;
  }
  else {
    // Fetch the icon from network.  When done this will associate the
    // icon to the page and notify.
    nsRefPtr<AsyncFetchAndSetIconFromNetwork> event =
      new AsyncFetchAndSetIconFromNetwork(mIcon, mPage, mFaviconLoadPrivate, mCallback);

    // Start the work on the main thread.
    rv = NS_DispatchToMainThread(event);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncFetchAndSetIconFromNetwork

NS_IMPL_ISUPPORTS_INHERITED3(
  AsyncFetchAndSetIconFromNetwork
, nsRunnable
, nsIStreamListener
, nsIInterfaceRequestor
, nsIChannelEventSink
)

AsyncFetchAndSetIconFromNetwork::AsyncFetchAndSetIconFromNetwork(
  IconData& aIcon
, PageData& aPage
, bool aFaviconLoadPrivate
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
)
: AsyncFaviconHelperBase(aCallback)
, mIcon(aIcon)
, mPage(aPage)
, mFaviconLoadPrivate(aFaviconLoadPrivate)
{
}

AsyncFetchAndSetIconFromNetwork::~AsyncFetchAndSetIconFromNetwork()
{
}

NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::Run()
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  // Ensure data is cleared, since it's going to be overwritten.
  if (mIcon.data.Length() > 0) {
    mIcon.data.Truncate(0);
    mIcon.mimeType.Truncate(0);
  }

  nsCOMPtr<nsIURI> iconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), iconURI);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIInterfaceRequestor> listenerRequestor =
    do_QueryInterface(reinterpret_cast<nsISupports*>(this));
  NS_ENSURE_STATE(listenerRequestor);
  rv = channel->SetNotificationCallbacks(listenerRequestor);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
  if (pbChannel) {
    rv = pbChannel->SetPrivate(mFaviconLoadPrivate);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return channel->AsyncOpen(this, nullptr);
}

NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::OnStartRequest(nsIRequest* aRequest,
                                                nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::OnDataAvailable(nsIRequest* aRequest,
                                                 nsISupports* aContext,
                                                 nsIInputStream* aInputStream,
                                                 uint64_t aOffset,
                                                 uint32_t aCount)
{
  nsAutoCString buffer;
  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv)) {
    return rv;
  }

  mIcon.data.Append(buffer);
  return NS_OK;
}


NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::GetInterface(const nsIID& uuid,
                                              void** aResult)
{
  return QueryInterface(uuid, aResult);
}


NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::AsyncOnChannelRedirect(
  nsIChannel* oldChannel
, nsIChannel* newChannel
, uint32_t flags
, nsIAsyncVerifyRedirectCallback *cb
)
{
  (void)cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconFromNetwork::OnStopRequest(nsIRequest* aRequest,
                                               nsISupports* aContext,
                                               nsresult aStatusCode)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsFaviconService* favicons = nsFaviconService::GetFaviconService();
  NS_ENSURE_STATE(favicons);

  nsresult rv;

  // If fetching the icon failed, add it to the failed cache.
  if (NS_FAILED(aStatusCode) || mIcon.data.Length() == 0) {
    nsCOMPtr<nsIURI> iconURI;
    rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = favicons->AddFailedFavicon(iconURI);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, aRequest,
                  TO_INTBUFFER(mIcon.data), mIcon.data.Length(),
                  mIcon.mimeType);

  // If the icon does not have a valid MIME type, add it to the failed cache.
  if (mIcon.mimeType.IsEmpty()) {
    nsCOMPtr<nsIURI> iconURI;
    rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = favicons->AddFailedFavicon(iconURI);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  // aRequest should always QI to nsIChannel.
  // See AsyncFetchAndSetIconFromNetwork::Run()
  MOZ_ASSERT(channel);
  mIcon.expiration = GetExpirationTimeFromChannel(channel);

  rv = OptimizeIconSize(mIcon, favicons);
  NS_ENSURE_SUCCESS(rv, rv);

  // If over the maximum size allowed, don't save data to the database to
  // avoid bloating it.
  if (mIcon.data.Length() > MAX_FAVICON_SIZE) {
    return NS_OK;
  }

  mIcon.status = ICON_STATUS_CHANGED;

  nsRefPtr<AsyncAssociateIconToPage> event =
    new AsyncAssociateIconToPage(mIcon, mPage, mCallback);
  mDB->DispatchToAsyncThread(event);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncAssociateIconToPage

AsyncAssociateIconToPage::AsyncAssociateIconToPage(
  IconData& aIcon
, PageData& aPage
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : AsyncFaviconHelperBase(aCallback)
  , mIcon(aIcon)
  , mPage(aPage)
{
}

AsyncAssociateIconToPage::~AsyncAssociateIconToPage()
{
}

NS_IMETHODIMP
AsyncAssociateIconToPage::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  nsresult rv = FetchPageInfo(mDB, mPage);
  if (rv == NS_ERROR_NOT_AVAILABLE){
    // We have never seen this page.  If we can add the page to history,
    // we will try to do it later, otherwise just bail out.
    if (!mPage.canAddToHistory) {
      return NS_OK;
    }
  }
  else {
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageTransaction transaction(mDB->MainConn(), false,
                                    mozIStorageConnection::TRANSACTION_IMMEDIATE);

  // If there is no entry for this icon, or the entry is obsolete, replace it.
  if (mIcon.id == 0 || (mIcon.status & ICON_STATUS_CHANGED)) {
    rv = SetIconInfo(mDB, mIcon);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the new icon id.  Do this regardless mIcon.id, since other code
    // could have added a entry before us.  Indeed we interrupted the thread
    // after the previous call to FetchIconInfo.
    mIcon.status = (mIcon.status & ~(ICON_STATUS_CACHED)) | ICON_STATUS_SAVED;
    rv = FetchIconInfo(mDB, mIcon);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If the page does not have an id, don't try to insert a new one, cause we
  // don't know where the page comes from.  Not doing so we may end adding
  // a page that otherwise we'd explicitly ignore, like a POST or an error page.
  if (mPage.id == 0) {
    return NS_OK;
  }

  // Otherwise just associate the icon to the page, if needed.
  if (mPage.iconId != mIcon.id) {
    nsCOMPtr<mozIStorageStatement> stmt;
    if (mPage.id) {
      stmt = mDB->GetStatement(
        "UPDATE moz_places SET favicon_id = :icon_id WHERE id = :page_id"
      );
      NS_ENSURE_STATE(stmt);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPage.id);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      stmt = mDB->GetStatement(
        "UPDATE moz_places SET favicon_id = :icon_id WHERE url = :page_url"
      );
      NS_ENSURE_STATE(stmt);
      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mPage.spec);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("icon_id"), mIcon.id);
    NS_ENSURE_SUCCESS(rv, rv);

    mozStorageStatementScoper scoper(stmt);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    mIcon.status |= ICON_STATUS_ASSOCIATED;
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, dispatch an event to the main thread to notify observers.
  nsCOMPtr<nsIRunnable> event = new NotifyIconObservers(mIcon, mPage, mCallback);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncGetFaviconURLForPage

// static
nsresult
AsyncGetFaviconURLForPage::start(nsIURI* aPageURI,
                                 nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aCallback);
  NS_ENSURE_ARG(aPageURI);
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread.");

  nsAutoCString pageSpec;
  nsresult rv = aPageURI->GetSpec(pageSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFaviconDataCallback> callback = aCallback;
  nsRefPtr<AsyncGetFaviconURLForPage> event =
    new AsyncGetFaviconURLForPage(pageSpec, callback);

  nsRefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

AsyncGetFaviconURLForPage::AsyncGetFaviconURLForPage(
  const nsACString& aPageSpec
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : AsyncFaviconHelperBase(aCallback)
{
  mPageSpec.Assign(aPageSpec);
}

AsyncGetFaviconURLForPage::~AsyncGetFaviconURLForPage()
{
}

NS_IMETHODIMP
AsyncGetFaviconURLForPage::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread.");

  nsAutoCString iconSpec;
  nsresult rv = FetchIconURL(mDB, mPageSpec, iconSpec);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Now notify our callback of the icon spec we retrieved, even if empty.
  IconData iconData;
  iconData.spec.Assign(iconSpec);

  PageData pageData;
  pageData.spec.Assign(mPageSpec);

  nsCOMPtr<nsIRunnable> event =
    new NotifyIconObservers(iconData, pageData, mCallback);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncGetFaviconDataForPage

// static
nsresult
AsyncGetFaviconDataForPage::start(nsIURI* aPageURI,
                                  nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aCallback);
  NS_ENSURE_ARG(aPageURI);
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread.");

  nsAutoCString pageSpec;
  nsresult rv = aPageURI->GetSpec(pageSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFaviconDataCallback> callback = aCallback;
  nsRefPtr<AsyncGetFaviconDataForPage> event =
    new AsyncGetFaviconDataForPage(pageSpec, callback);

  nsRefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

AsyncGetFaviconDataForPage::AsyncGetFaviconDataForPage(
  const nsACString& aPageSpec
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : AsyncFaviconHelperBase(aCallback)
{
  mPageSpec.Assign(aPageSpec);
}

AsyncGetFaviconDataForPage::~AsyncGetFaviconDataForPage()
{
}

NS_IMETHODIMP
AsyncGetFaviconDataForPage::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread.");

  nsAutoCString iconSpec;
  nsresult rv = FetchIconURL(mDB, mPageSpec, iconSpec);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  IconData iconData;
  iconData.spec.Assign(iconSpec);

  PageData pageData;
  pageData.spec.Assign(mPageSpec);

  if (!iconSpec.IsEmpty()) {
    rv = FetchIconInfo(mDB, iconData);
    if (NS_FAILED(rv)) {
      iconData.spec.Truncate();
      MOZ_ASSERT(false, "Fetching favicon information failed unexpectedly.");
    }
  }

  nsCOMPtr<nsIRunnable> event =
    new NotifyIconObservers(iconData, pageData, mCallback);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncReplaceFaviconData

// static
nsresult
AsyncReplaceFaviconData::start(IconData *aIcon)
{
  NS_ENSURE_ARG(aIcon);
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread.");

  nsCOMPtr<nsIFaviconDataCallback> callback;
  nsRefPtr<AsyncReplaceFaviconData> event =
    new AsyncReplaceFaviconData(*aIcon, callback);

  nsRefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

AsyncReplaceFaviconData::AsyncReplaceFaviconData(
  IconData &aIcon
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
) : AsyncFaviconHelperBase(aCallback)
  , mIcon(aIcon)
{
}

AsyncReplaceFaviconData::~AsyncReplaceFaviconData()
{
}

NS_IMETHODIMP
AsyncReplaceFaviconData::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(),
                  "This should not be called on the main thread");

  IconData dbIcon;
  dbIcon.spec.Assign(mIcon.spec);
  nsresult rv = FetchIconInfo(mDB, dbIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!dbIcon.id) {
    return NS_OK;
  }

  rv = SetIconInfo(mDB, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  // We can invalidate the cache version since we now persist the icon.
  nsCOMPtr<nsIRunnable> event = new RemoveIconDataCacheEntry(mIcon, mCallback);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// RemoveIconDataCacheEntry

RemoveIconDataCacheEntry::RemoveIconDataCacheEntry(
  IconData& aIcon
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
)
: AsyncFaviconHelperBase(aCallback)
, mIcon(aIcon)
{
}

RemoveIconDataCacheEntry::~RemoveIconDataCacheEntry()
{
}

NS_IMETHODIMP
RemoveIconDataCacheEntry::Run()
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  nsCOMPtr<nsIURI> iconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsFaviconService* favicons = nsFaviconService::GetFaviconService();
  NS_ENSURE_STATE(favicons);
  favicons->mUnassociatedIcons.RemoveEntry(iconURI);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// NotifyIconObservers

NotifyIconObservers::NotifyIconObservers(
  IconData& aIcon
, PageData& aPage
, nsCOMPtr<nsIFaviconDataCallback>& aCallback
)
: AsyncFaviconHelperBase(aCallback)
, mIcon(aIcon)
, mPage(aPage)
{
}

NotifyIconObservers::~NotifyIconObservers()
{
}

NS_IMETHODIMP
NotifyIconObservers::Run()
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "This should be called on the main thread");

  nsCOMPtr<nsIURI> iconURI;
  if (!mIcon.spec.IsEmpty()) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_NewURI(getter_AddRefs(iconURI), mIcon.spec)));
    if (iconURI)
    {
      // Notify observers only if something changed.
      if (mIcon.status & ICON_STATUS_SAVED ||
          mIcon.status & ICON_STATUS_ASSOCIATED) {
        SendGlobalNotifications(iconURI);
      }
    }
  }

  if (mCallback) {
    (void)mCallback->OnComplete(iconURI, mIcon.data.Length(),
                                TO_INTBUFFER(mIcon.data), mIcon.mimeType);
  }

  return NS_OK;
}

void
NotifyIconObservers::SendGlobalNotifications(nsIURI* aIconURI)
{
  nsCOMPtr<nsIURI> pageURI;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_NewURI(getter_AddRefs(pageURI), mPage.spec)));
  if (pageURI) {
    nsFaviconService* favicons = nsFaviconService::GetFaviconService();
    MOZ_ASSERT(favicons);
    if (favicons) {
      (void)favicons->SendFaviconNotifications(pageURI, aIconURI, mPage.guid);
    }
  }

  // If the page is bookmarked and the bookmarked url is different from the
  // updated one, start a new task to update its icon as well.
  if (!mPage.bookmarkedSpec.IsEmpty() &&
      !mPage.bookmarkedSpec.Equals(mPage.spec)) {
    // Create a new page struct to avoid polluting it with old data.
    PageData bookmarkedPage;
    bookmarkedPage.spec = mPage.bookmarkedSpec;

    // This will be silent, so be sure to not pass in the current callback.
    nsCOMPtr<nsIFaviconDataCallback> nullCallback;
    nsRefPtr<AsyncAssociateIconToPage> event =
        new AsyncAssociateIconToPage(mIcon, bookmarkedPage, nullCallback);
    mDB->DispatchToAsyncThread(event);
  }
}

} // namespace places
} // namespace mozilla
