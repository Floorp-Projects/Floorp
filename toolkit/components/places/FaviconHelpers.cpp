/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FaviconHelpers.h"

#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIPrincipal.h"

#include "nsNavHistory.h"
#include "nsFaviconService.h"
#include "mozilla/storage.h"
#include "mozilla/Telemetry.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsISupportsPriority.h"
#include "nsContentUtils.h"
#include <algorithm>
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "imgLoader.h"
#include "imgIEncoder.h"

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
FetchPageInfo(const RefPtr<Database>& aDB,
              PageData& _page)
{
  MOZ_ASSERT(_page.spec.Length(), "Must have a non-empty spec!");
  MOZ_ASSERT(!NS_IsMainThread());

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
    ") FROM moz_places h WHERE h.url_hash = hash(:page_url) AND h.url = :page_url",
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
  // favicon_id can be nullptr.
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
SetIconInfo(const RefPtr<Database>& aDB,
            const IconData& aIcon)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "INSERT OR REPLACE INTO moz_favicons "
      "(id, url, data, mime_type, expiration) "
    "VALUES ((SELECT id FROM moz_favicons WHERE url = :icon_url), "
            ":icon_url, :data, :mime_type, :expiration) "
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
FetchIconInfo(const RefPtr<Database>& aDB,
              IconData& _icon)
{
  MOZ_ASSERT(_icon.spec.Length(), "Must have a non-empty spec!");
  MOZ_ASSERT(!NS_IsMainThread());

  if (_icon.status & ICON_STATUS_CACHED) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "SELECT id, expiration, data, mime_type "
    "FROM moz_favicons WHERE url = :icon_url"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  DebugOnly<nsresult> rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"),
                                           _icon.spec);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!hasResult) {
    // The icon does not exist yet, bail out.
    return NS_OK;
  }

  rv = stmt->GetInt64(0, &_icon.id);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Expiration can be nullptr.
  bool isNull;
  rv = stmt->GetIsNull(1, &isNull);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!isNull) {
    rv = stmt->GetInt64(1, reinterpret_cast<int64_t*>(&_icon.expiration));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // Data can be nullptr.
  rv = stmt->GetIsNull(2, &isNull);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!isNull) {
    uint8_t* data;
    uint32_t dataLen = 0;
    rv = stmt->GetBlob(2, &dataLen, &data);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    _icon.data.Adopt(TO_CHARBUFFER(data), dataLen);
    // Read mime only if we have data.
    rv = stmt->GetUTF8String(3, _icon.mimeType);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  return NS_OK;
}

nsresult
FetchIconURL(const RefPtr<Database>& aDB,
             const nsACString& aPageSpec,
             nsACString& aIconSpec)
{
  MOZ_ASSERT(!aPageSpec.IsEmpty(), "Page spec must not be empty.");
  MOZ_ASSERT(!NS_IsMainThread());

  aIconSpec.Truncate();

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "SELECT f.url "
    "FROM moz_places h "
    "JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url_hash = hash(:page_url) AND h.url = :page_url"
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
  MOZ_ASSERT(NS_IsMainThread());

  // Attempt to get an expiration time from the cache.  If this fails, we'll
  // make one up.
  PRTime expiration = -1;
  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(aChannel);
  if (cachingChannel) {
    nsCOMPtr<nsISupports> cacheToken;
    nsresult rv = cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICacheEntry> cacheEntry = do_QueryInterface(cacheToken);
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
  MOZ_ASSERT(NS_IsMainThread());

  // Even if the page provides a large image for the favicon (eg, a highres
  // image or a multiresolution .ico file), don't try to store more data than
  // needed.
  nsAutoCString newData, newMimeType;
  if (aIcon.data.Length() > MAX_FAVICON_FILESIZE) {
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

} // namespace

////////////////////////////////////////////////////////////////////////////////
//// AsyncFetchAndSetIconForPage

NS_IMPL_ISUPPORTS_INHERITED(
  AsyncFetchAndSetIconForPage
, Runnable
, nsIStreamListener
, nsIInterfaceRequestor
, nsIChannelEventSink
, mozIPlacesPendingOperation
)

AsyncFetchAndSetIconForPage::AsyncFetchAndSetIconForPage(
  IconData& aIcon
, PageData& aPage
, bool aFaviconLoadPrivate
, nsIFaviconDataCallback* aCallback
, nsIPrincipal* aLoadingPrincipal
) : mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(aCallback))
  , mIcon(aIcon)
  , mPage(aPage)
  , mFaviconLoadPrivate(aFaviconLoadPrivate)
  , mLoadingPrincipal(new nsMainThreadPtrHolder<nsIPrincipal>(aLoadingPrincipal))
  , mCanceled(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Try to fetch the icon from the database.
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsresult rv = FetchIconInfo(DB, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInvalidIcon = mIcon.data.IsEmpty() ||
                       (mIcon.expiration && PR_Now() > mIcon.expiration);
  bool fetchIconFromNetwork = mIcon.fetchMode == FETCH_ALWAYS ||
                              (mIcon.fetchMode == FETCH_IF_MISSING && isInvalidIcon);

  if (!fetchIconFromNetwork) {
    // There is already a valid icon or we don't want to fetch a new one,
    // directly proceed with association.
    RefPtr<AsyncAssociateIconToPage> event =
        new AsyncAssociateIconToPage(mIcon, mPage, mCallback);
    DB->DispatchToAsyncThread(event);

    return NS_OK;
  }

  // Fetch the icon from the network, the request starts from the main-thread.
  // When done this will associate the icon to the page and notify.
  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod(this, &AsyncFetchAndSetIconForPage::FetchFromNetwork);
  return NS_DispatchToMainThread(event);
}

nsresult
AsyncFetchAndSetIconForPage::FetchFromNetwork() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mCanceled) {
    return NS_OK;
  }

  // Ensure data is cleared, since it's going to be overwritten.
  if (mIcon.data.Length() > 0) {
    mIcon.data.Truncate(0);
    mIcon.mimeType.Truncate(0);
  }

  nsCOMPtr<nsIURI> iconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     iconURI,
                     mLoadingPrincipal,
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
                     nsILoadInfo::SEC_ALLOW_CHROME |
                     nsILoadInfo::SEC_DISALLOW_SCRIPT,
                     nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON);

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

  nsCOMPtr<nsISupportsPriority> priorityChannel = do_QueryInterface(channel);
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }

  rv = channel->AsyncOpen2(this);
  if (NS_SUCCEEDED(rv)) {
    mRequest = channel;
  }
  return rv;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mCanceled) {
    return NS_ERROR_UNEXPECTED;
  }
  mCanceled = true;
  if (mRequest) {
    mRequest->Cancel(NS_BINDING_ABORTED);
  }
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnStartRequest(nsIRequest* aRequest,
                                            nsISupports* aContext)
{
  // mRequest should already be set from ::FetchFromNetwork, but in the case of
  // a redirect we might get a new request, and we should make sure we keep a
  // reference to the most current request.
  mRequest = aRequest;
  if (mCanceled) {
    mRequest->Cancel(NS_BINDING_ABORTED);
  }
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aInputStream,
                                             uint64_t aOffset,
                                             uint32_t aCount)
{
  const size_t kMaxFaviconDownloadSize = 1 * 1024 * 1024;
  if (mIcon.data.Length() + aCount > kMaxFaviconDownloadSize) {
    mIcon.data.Truncate();
    return NS_ERROR_FILE_TOO_BIG;
  }

  nsAutoCString buffer;
  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv)) {
    return rv;
  }

  if (!mIcon.data.Append(buffer, fallible)) {
    mIcon.data.Truncate();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


NS_IMETHODIMP
AsyncFetchAndSetIconForPage::GetInterface(const nsIID& uuid,
                                          void** aResult)
{
  return QueryInterface(uuid, aResult);
}


NS_IMETHODIMP
AsyncFetchAndSetIconForPage::AsyncOnChannelRedirect(
  nsIChannel* oldChannel
, nsIChannel* newChannel
, uint32_t flags
, nsIAsyncVerifyRedirectCallback *cb
)
{
  // If we've been canceled, stop the redirect with NS_BINDING_ABORTED, and
  // handle the cancel on the original channel.
  (void)cb->OnRedirectVerifyCallback(mCanceled ? NS_BINDING_ABORTED : NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnStopRequest(nsIRequest* aRequest,
                                           nsISupports* aContext,
                                           nsresult aStatusCode)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Don't need to track this anymore.
  mRequest = nullptr;
  if (mCanceled) {
    return NS_OK;
  }

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

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  // aRequest should always QI to nsIChannel.
  MOZ_ASSERT(channel);

  nsAutoCString contentType;
  channel->GetContentType(contentType);
  // Bug 366324 - can't sniff SVG yet, so rely on server-specified type
  if (contentType.EqualsLiteral("image/svg+xml")) {
    mIcon.mimeType.AssignLiteral("image/svg+xml");
  } else {
    NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, aRequest,
                    TO_INTBUFFER(mIcon.data), mIcon.data.Length(),
                    mIcon.mimeType);
  }

  // If the icon does not have a valid MIME type, add it to the failed cache.
  if (mIcon.mimeType.IsEmpty()) {
    nsCOMPtr<nsIURI> iconURI;
    rv = NS_NewURI(getter_AddRefs(iconURI), mIcon.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = favicons->AddFailedFavicon(iconURI);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  mIcon.expiration = GetExpirationTimeFromChannel(channel);

  // Telemetry probes to measure the favicon file sizes for each different file type.
  // This allow us to measure common file sizes while also observing each type popularity.
  if (mIcon.mimeType.EqualsLiteral("image/png")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_PNG_SIZES, mIcon.data.Length());
  }
  else if (mIcon.mimeType.EqualsLiteral("image/x-icon") ||
           mIcon.mimeType.EqualsLiteral("image/vnd.microsoft.icon")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_ICO_SIZES, mIcon.data.Length());
  }
  else if (mIcon.mimeType.EqualsLiteral("image/jpeg") ||
           mIcon.mimeType.EqualsLiteral("image/pjpeg")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_JPEG_SIZES, mIcon.data.Length());
  }
  else if (mIcon.mimeType.EqualsLiteral("image/gif")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_GIF_SIZES, mIcon.data.Length());
  }
  else if (mIcon.mimeType.EqualsLiteral("image/bmp") ||
           mIcon.mimeType.EqualsLiteral("image/x-windows-bmp")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_BMP_SIZES, mIcon.data.Length());
  }
  else if (mIcon.mimeType.EqualsLiteral("image/svg+xml")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_SVG_SIZES, mIcon.data.Length());
  }
  else {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_OTHER_SIZES, mIcon.data.Length());
  }

  rv = OptimizeIconSize(mIcon, favicons);
  NS_ENSURE_SUCCESS(rv, rv);

  // If over the maximum size allowed, don't save data to the database to
  // avoid bloating it.
  if (mIcon.data.Length() > nsIFaviconService::MAX_FAVICON_BUFFER_SIZE) {
    return NS_OK;
  }

  mIcon.status = ICON_STATUS_CHANGED;

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  RefPtr<AsyncAssociateIconToPage> event =
    new AsyncAssociateIconToPage(mIcon, mPage, mCallback);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncAssociateIconToPage

AsyncAssociateIconToPage::AsyncAssociateIconToPage(
  const IconData& aIcon
, const PageData& aPage
, const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback
) : mCallback(aCallback)
  , mIcon(aIcon)
  , mPage(aPage)
{
}

NS_IMETHODIMP
AsyncAssociateIconToPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsresult rv = FetchPageInfo(DB, mPage);
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

  mozStorageTransaction transaction(DB->MainConn(), false,
                                    mozIStorageConnection::TRANSACTION_IMMEDIATE);

  // If there is no entry for this icon, or the entry is obsolete, replace it.
  if (mIcon.id == 0 || (mIcon.status & ICON_STATUS_CHANGED)) {
    rv = SetIconInfo(DB, mIcon);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the new icon id.  Do this regardless mIcon.id, since other code
    // could have added a entry before us.  Indeed we interrupted the thread
    // after the previous call to FetchIconInfo.
    mIcon.status = (mIcon.status & ~(ICON_STATUS_CACHED)) | ICON_STATUS_SAVED;
    rv = FetchIconInfo(DB, mIcon);
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
      stmt = DB->GetStatement(
        "UPDATE moz_places SET favicon_id = :icon_id WHERE id = :page_id"
      );
      NS_ENSURE_STATE(stmt);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPage.id);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      stmt = DB->GetStatement(
        "UPDATE moz_places SET favicon_id = :icon_id "
        "WHERE url_hash = hash(:page_url) AND url = :page_url"
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

AsyncGetFaviconURLForPage::AsyncGetFaviconURLForPage(
  const nsACString& aPageSpec
, nsIFaviconDataCallback* aCallback
) : mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(aCallback))
{
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
}

NS_IMETHODIMP
AsyncGetFaviconURLForPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsAutoCString iconSpec;
  nsresult rv = FetchIconURL(DB, mPageSpec, iconSpec);
  NS_ENSURE_SUCCESS(rv, rv);

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

AsyncGetFaviconDataForPage::AsyncGetFaviconDataForPage(
  const nsACString& aPageSpec
, nsIFaviconDataCallback* aCallback
) : mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(aCallback))
 {
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
}

NS_IMETHODIMP
AsyncGetFaviconDataForPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsAutoCString iconSpec;
  nsresult rv = FetchIconURL(DB, mPageSpec, iconSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  IconData iconData;
  iconData.spec.Assign(iconSpec);

  PageData pageData;
  pageData.spec.Assign(mPageSpec);

  if (!iconSpec.IsEmpty()) {
    rv = FetchIconInfo(DB, iconData);
    if (NS_FAILED(rv)) {
      iconData.spec.Truncate();
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

AsyncReplaceFaviconData::AsyncReplaceFaviconData(const IconData &aIcon)
  : mIcon(aIcon)
{
}

NS_IMETHODIMP
AsyncReplaceFaviconData::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  IconData dbIcon;
  dbIcon.spec.Assign(mIcon.spec);
  nsresult rv = FetchIconInfo(DB, dbIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!dbIcon.id) {
    return NS_OK;
  }

  rv = SetIconInfo(DB, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  // We can invalidate the cache version since we now persist the icon.
  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod(this, &AsyncReplaceFaviconData::RemoveIconDataCacheEntry);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
AsyncReplaceFaviconData::RemoveIconDataCacheEntry()
{
  MOZ_ASSERT(NS_IsMainThread());

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
  const IconData& aIcon
, const PageData& aPage
, const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback
)
: mCallback(aCallback)
, mIcon(aIcon)
, mPage(aPage)
{
}

NS_IMETHODIMP
NotifyIconObservers::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> iconURI;
  if (!mIcon.spec.IsEmpty()) {
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(iconURI), mIcon.spec));
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
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(pageURI), mPage.spec));
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

    RefPtr<Database> DB = Database::GetDatabase();
    if (!DB)
      return;
    // This will be silent, so be sure to not pass in the current callback.
    nsMainThreadPtrHandle<nsIFaviconDataCallback> nullCallback;
    RefPtr<AsyncAssociateIconToPage> event =
        new AsyncAssociateIconToPage(mIcon, bookmarkedPage, nullCallback);
    DB->DispatchToAsyncThread(event);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// FetchAndConvertUnsupportedPayloads

FetchAndConvertUnsupportedPayloads::FetchAndConvertUnsupportedPayloads (
  mozIStorageConnection* aDBConn
) : mDB(aDBConn)
{

}

NS_IMETHODIMP
FetchAndConvertUnsupportedPayloads::Run()
{
  if (NS_IsMainThread()) {
    Preferences::ClearUser(PREF_CONVERT_PAYLOADS);
    return NS_OK;
  }

  MOZ_ASSERT(!NS_IsMainThread());
  NS_ENSURE_STATE(mDB);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDB->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, width, data FROM moz_icons WHERE typeof(width) = 'text' "
    "ORDER BY id ASC "
    "LIMIT 200 "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB, false,
                                    mozIStorageConnection::TRANSACTION_IMMEDIATE);

  // We should do the work in chunks, or the wal journal may grow too much.
  uint8_t count = 0;
  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    ++count;
    int64_t id = stmt->AsInt64(0);
    MOZ_ASSERT(id > 0);
    nsAutoCString mimeType;
    rv = stmt->GetUTF8String(1, mimeType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    uint8_t* data;
    uint32_t dataLen = 0;
    rv = stmt->GetBlob(2, &dataLen, &data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    nsCString buf;
    buf.Adopt(TO_CHARBUFFER(data), dataLen);

    int32_t width = 0;
    rv = ConvertPayload(id, mimeType, buf, &width);
    Unused << NS_WARN_IF(NS_FAILED(rv));
    if (NS_SUCCEEDED(rv)) {
      rv = StorePayload(id, width, buf);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (count == 200) {
    // There are more results to handle. Re-dispatch to the same thread for the
    // next chunk.
    return NS_DispatchToCurrentThread(this);
  }

  // We're done. Remove any leftovers and force a checkpoint for safety.
  rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_icons WHERE typeof(width) = 'text'"
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA wal_checkpoint"
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  // Run a one-time VACUUM of places.sqlite, since we removed a lot from it.
  // It may cause jank, but not doing it could cause dataloss due to expiration.
  rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "VACUUM"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Re-dispatch to the main-thread to flip the conversion pref.
  return NS_DispatchToMainThread(this);
}

nsresult
FetchAndConvertUnsupportedPayloads::ConvertPayload(int64_t aId,
                                                   const nsACString& aMimeType,
                                                   nsCString& aPayload,
                                                   int32_t* aWidth)
{
  // TODO (bug 1346139): this should probably be unified with the function that
  // will handle additions optimization off the main thread.
  MOZ_ASSERT(!NS_IsMainThread());
  *aWidth = 0;

  // Exclude invalid mime types.
  if (aPayload.Length() == 0 ||
      !imgLoader::SupportImageWithMimeType(PromiseFlatCString(aMimeType).get(),
                                           AcceptedMimeTypes::IMAGES)) {
    return NS_ERROR_FAILURE;
  }

  // If it's an SVG, there's nothing to optimize or convert.
  if (aMimeType.EqualsLiteral(SVG_MIME_TYPE)) {
    *aWidth = UINT16_MAX;
    return NS_OK;
  }

  // Convert the payload to an input stream.
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                aPayload.get(), aPayload.Length(),
                NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  // Decode the input stream to a surface.
  RefPtr<gfx::SourceSurface> surface =
      image::ImageOps::DecodeToSurface(stream,
                                       aMimeType,
                                       imgIContainer::DECODE_FLAGS_DEFAULT);
  NS_ENSURE_STATE(surface);
  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  NS_ENSURE_STATE(dataSurface);

  // Read the current size and set an appropriate final width.
  int32_t width = dataSurface->GetSize().width;
  int32_t height = dataSurface->GetSize().height;
  // For non-square images, pick the largest side.
  int32_t originalSize = std::max(width, height);
  int32_t size = originalSize;
  for (uint16_t supportedSize : sFaviconSizes) {
    if (supportedSize <= originalSize) {
      size = supportedSize;
      break;
    }
  }
  *aWidth = size;

  // If the original payload is png and the size is the same, no reason to
  // rescale the image.
  if (aMimeType.EqualsLiteral(PNG_MIME_TYPE) && size == originalSize) {
    return NS_OK;
  }

  // Rescale when needed.
  RefPtr<gfx::DataSourceSurface> targetDataSurface =
    gfx::Factory::CreateDataSourceSurface(gfx::IntSize(size, size),
                                          gfx::SurfaceFormat::B8G8R8A8,
                                          true);
  NS_ENSURE_STATE(targetDataSurface);

  { // Block scope for map.
    gfx::DataSourceSurface::MappedSurface map;
    if (!targetDataSurface->Map(gfx::DataSourceSurface::MapType::WRITE, &map)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<gfx::DrawTarget> dt =
      gfx::Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                            map.mData,
                                            targetDataSurface->GetSize(),
                                            map.mStride,
                                            gfx::SurfaceFormat::B8G8R8A8);
    NS_ENSURE_STATE(dt);

    gfx::IntSize frameSize = dataSurface->GetSize();
    dt->DrawSurface(dataSurface,
                    gfx::Rect(0, 0, size, size),
                    gfx::Rect(0, 0, frameSize.width, frameSize.height),
                    gfx::DrawSurfaceOptions(),
                    gfx::DrawOptions(1.0f, gfx::CompositionOp::OP_SOURCE));
    targetDataSurface->Unmap();
  }

  // Finally Encode.
  nsCOMPtr<imgIEncoder> encoder =
    do_CreateInstance("@mozilla.org/image/encoder;2?type=image/png");
  NS_ENSURE_STATE(encoder);

  gfx::DataSourceSurface::MappedSurface map;
  if (!targetDataSurface->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }
  rv = encoder->InitFromData(map.mData, map.mStride * size, size, size,
                             map.mStride, imgIEncoder::INPUT_FORMAT_HOSTARGB,
                             EmptyString());
  targetDataSurface->Unmap();
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the stream into a new buffer.
  nsCOMPtr<nsIInputStream> iconStream = do_QueryInterface(encoder);
  NS_ENSURE_STATE(iconStream);
  rv = NS_ConsumeStream(iconStream, UINT32_MAX, aPayload);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
FetchAndConvertUnsupportedPayloads::StorePayload(int64_t aId,
                                                 int32_t aWidth,
                                                 const nsCString& aPayload)
{
  MOZ_ASSERT(!NS_IsMainThread());

  NS_ENSURE_STATE(mDB);
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDB->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_icons SET data = :data, width = :width WHERE id = :id"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), aId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("width"), aWidth);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindBlobByName(NS_LITERAL_CSTRING("data"),
                            TO_INTBUFFER(aPayload), aPayload.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace places
} // namespace mozilla
