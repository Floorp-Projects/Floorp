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
#include <deque>
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "imgIEncoder.h"

using namespace mozilla::places;
using namespace mozilla::storage;

namespace mozilla {
namespace places {

namespace {

/**
 * Fetches information about a page from the database.
 *
 * @param aDB
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
    "SELECT h.id, pi.id, h.guid, ( "
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
    "), fixup_url(get_unreversed_host(h.rev_host)) AS host "
    "FROM moz_places h "
    "LEFT JOIN moz_pages_w_icons pi ON page_url_hash = hash(:page_url) AND page_url = :page_url "
    "WHERE h.url_hash = hash(:page_url) AND h.url = :page_url",
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

  rv = stmt->GetInt64(0, &_page.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  // May be null, and in such a case this will be 0.
  _page.id = stmt->AsInt64(1);
  rv = stmt->GetUTF8String(2, _page.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  // Bookmarked url can be nullptr.
  bool isNull;
  rv = stmt->GetIsNull(3, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  // The page could not be bookmarked.
  if (!isNull) {
    rv = stmt->GetUTF8String(3, _page.bookmarkedSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (_page.host.IsEmpty()) {
    rv = stmt->GetUTF8String(4, _page.host);
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
 * Stores information about an icon in the database.
 *
 * @param aDB
 *        Database connection to history tables.
 * @param aIcon
 *        Icon that should be stored.
 * @param aMustReplace
 *        If set to true, the function will bail out with NS_ERROR_NOT_AVAILABLE
 *        if it can't find a previous stored icon to replace.
 * @note Should be wrapped in a transaction.
 */
nsresult
SetIconInfo(const RefPtr<Database>& aDB,
            IconData& aIcon,
            bool aMustReplace = false)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aIcon.payloads.Length() > 0);
  MOZ_ASSERT(!aIcon.spec.IsEmpty());
  MOZ_ASSERT(aIcon.expiration > 0);

  // There are multiple cases possible at this point:
  //   1. We must insert some payloads and no payloads exist in the table. This
  //      would be a straight INSERT.
  //   2. The table contains the same number of payloads we are inserting. This
  //      would be a straight UPDATE.
  //   3. The table contains more payloads than we are inserting. This would be
  //      an UPDATE and a DELETE.
  //   4. The table contains less payloads than we are inserting. This would be
  //      an UPDATE and an INSERT.
  // We can't just remove all the old entries and insert the new ones, cause
  // we'd lose the referential integrity with pages.  For the same reason we
  // cannot use INSERT OR REPLACE, since it's implemented as DELETE AND INSERT.
  // Thus, we follow this strategy:
  //   * SELECT all existing icon ids
  //   * For each payload, either UPDATE OR INSERT reusing icon ids.
  //   * If any previous icon ids is leftover, DELETE it.

  nsCOMPtr<mozIStorageStatement> selectStmt = aDB->GetStatement(
    "SELECT id FROM moz_icons "
    "WHERE fixed_icon_url_hash = hash(fixup_url(:url)) "
      "AND icon_url = :url "
  );
  NS_ENSURE_STATE(selectStmt);
  mozStorageStatementScoper scoper(selectStmt);
  nsresult rv = URIBinder::Bind(selectStmt, NS_LITERAL_CSTRING("url"), aIcon.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  std::deque<int64_t> ids;
  bool hasResult = false;
  while (NS_SUCCEEDED(selectStmt->ExecuteStep(&hasResult)) && hasResult) {
    int64_t id = selectStmt->AsInt64(0);
    MOZ_ASSERT(id > 0);
    ids.push_back(id);
  }
  if (aMustReplace && ids.empty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<mozIStorageStatement> insertStmt = aDB->GetStatement(
    "INSERT INTO moz_icons "
      "(icon_url, fixed_icon_url_hash, width, root, expire_ms, data) "
    "VALUES (:url, hash(fixup_url(:url)), :width, :root, :expire, :data) "
  );
  NS_ENSURE_STATE(insertStmt);
  nsCOMPtr<mozIStorageStatement> updateStmt = aDB->GetStatement(
    "UPDATE moz_icons SET width = :width, "
                         "expire_ms = :expire, "
                         "data = :data, "
                         "root = :root "
    "WHERE id = :id "
  );
  NS_ENSURE_STATE(updateStmt);

  for (auto& payload : aIcon.payloads) {
    // Sanity checks.
    MOZ_ASSERT(payload.mimeType.EqualsLiteral(PNG_MIME_TYPE) ||
              payload.mimeType.EqualsLiteral(SVG_MIME_TYPE),
              "Only png and svg payloads are supported");
    MOZ_ASSERT(!payload.mimeType.EqualsLiteral(SVG_MIME_TYPE) ||
               payload.width == UINT16_MAX,
              "SVG payloads should have max width");
    MOZ_ASSERT(payload.width > 0, "Payload should have a width");
#ifdef DEBUG
    // Done to ensure we fetch the id. See the MOZ_ASSERT below.
    payload.id = 0;
#endif
    if (!ids.empty()) {
      // Pop the first existing id for reuse.
      int64_t id = ids.front();
      ids.pop_front();
      mozStorageStatementScoper scoper(updateStmt);
      rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt32ByName(NS_LITERAL_CSTRING("width"),
                                       payload.width);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("expire"),
                                       aIcon.expiration / 1000);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt32ByName(NS_LITERAL_CSTRING("root"),
                                       aIcon.rootIcon);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindBlobByName(NS_LITERAL_CSTRING("data"),
                                TO_INTBUFFER(payload.data),
                                payload.data.Length());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
      // Set the new payload id.
      payload.id = id;
    } else {
      // Insert a new entry.
      mozStorageStatementScoper scoper(insertStmt);
      rv = URIBinder::Bind(insertStmt, NS_LITERAL_CSTRING("url"), aIcon.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindInt32ByName(NS_LITERAL_CSTRING("width"),
                                       payload.width);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = insertStmt->BindInt32ByName(NS_LITERAL_CSTRING("root"),
                                       aIcon.rootIcon);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindInt64ByName(NS_LITERAL_CSTRING("expire"),
                                       aIcon.expiration / 1000);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindBlobByName(NS_LITERAL_CSTRING("data"),
                                TO_INTBUFFER(payload.data),
                                payload.data.Length());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
      // Set the new payload id.
      payload.id = nsFaviconService::sLastInsertedIconId;
    }
    MOZ_ASSERT(payload.id > 0, "Payload should have an id");
  }

  if (!ids.empty()) {
    // Remove any old leftover payload.
    nsAutoCString sql("DELETE FROM moz_icons WHERE id IN (");
    for (int64_t id : ids) {
      sql.AppendInt(id);
      sql.AppendLiteral(",");
    }
    sql.AppendLiteral(" 0)"); // Non-existing id to match the trailing comma.
    nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(sql);
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/**
 * Fetches information on a icon url from the database.
 *
 * @param aDBConn
 *        Database connection to history tables.
 * @param aPreferredWidth
 *        The preferred size to fetch.
 * @param _icon
 *        Icon that should be fetched.
 */
nsresult
FetchIconInfo(const RefPtr<Database>& aDB,
              uint16_t aPreferredWidth,
              IconData& _icon
)
{
  MOZ_ASSERT(_icon.spec.Length(), "Must have a non-empty spec!");
  MOZ_ASSERT(!NS_IsMainThread());

  if (_icon.status & ICON_STATUS_CACHED) {
    // The icon data has already been set by ReplaceFaviconData.
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "/* do not warn (bug no: not worth having a compound index) */ "
    "SELECT id, expire_ms, data, width, root "
    "FROM moz_icons "
    "WHERE fixed_icon_url_hash = hash(fixup_url(:url)) "
      "AND icon_url = :url "
    "ORDER BY width DESC "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  DebugOnly<nsresult> rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("url"),
                                           _icon.spec);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  bool hasResult = false;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    IconPayload payload;
    rv = stmt->GetInt64(0, &payload.id);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Expiration can be nullptr.
    bool isNull;
    rv = stmt->GetIsNull(1, &isNull);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (!isNull) {
      int64_t expire_ms;
      rv = stmt->GetInt64(1, &expire_ms);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      _icon.expiration = expire_ms * 1000;
    }

    uint8_t* data;
    uint32_t dataLen = 0;
    rv = stmt->GetBlob(2, &dataLen, &data);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    payload.data.Adopt(TO_CHARBUFFER(data), dataLen);

    int32_t width;
    rv = stmt->GetInt32(3, &width);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    payload.width = width;
    if (payload.width == UINT16_MAX) {
      payload.mimeType.AssignLiteral(SVG_MIME_TYPE);
    } else {
      payload.mimeType.AssignLiteral(PNG_MIME_TYPE);
    }

    int32_t rootIcon;
    rv = stmt->GetInt32(4, &rootIcon);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    _icon.rootIcon = rootIcon;

    if (aPreferredWidth == 0 || _icon.payloads.Length() == 0) {
      _icon.payloads.AppendElement(payload);
    } else if (payload.width >= aPreferredWidth) {
      // Only retain the best matching payload.
      _icon.payloads.ReplaceElementAt(0, payload);
    } else {
      break;
    }
  }

  return NS_OK;
}

nsresult
FetchIconPerSpec(const RefPtr<Database>& aDB,
                 const nsACString& aPageSpec,
                 const nsACString& aPageHost,
                 IconData& aIconData,
                 uint16_t aPreferredWidth)
{
  MOZ_ASSERT(!aPageSpec.IsEmpty(), "Page spec must not be empty.");
  MOZ_ASSERT(!NS_IsMainThread());

  // This selects both associated and root domain icons, ordered by width,
  // where an associated icon has priority over a root domain icon.
  // Regardless, note that while this way we are far more efficient, we lost
  // associations with root domain icons, so it's possible we'll return one
  // for a specific size when an associated icon for that size doesn't exist.
  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(
    "/* do not warn (bug no: not worth having a compound index) */ "
    "SELECT width, icon_url, root "
    "FROM moz_icons i "
    "JOIN moz_icons_to_pages ON i.id = icon_id "
    "JOIN moz_pages_w_icons p ON p.id = page_id "
    "WHERE page_url_hash = hash(:url) AND page_url = :url "
       "OR (:hash_idx AND page_url_hash = hash(substr(:url, 0, :hash_idx)) "
                     "AND page_url = substr(:url, 0, :hash_idx)) "
    "UNION ALL "
    "SELECT width, icon_url, root "
    "FROM moz_icons i "
    "WHERE fixed_icon_url_hash = hash(fixup_url(:root_icon_url)) "
    "ORDER BY width DESC, root ASC "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("url"), aPageSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString rootIconFixedUrl(aPageHost);
  if (!rootIconFixedUrl.IsEmpty()) {
    rootIconFixedUrl.AppendLiteral("/favicon.ico");
  }
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("root_icon_url"),
                                  rootIconFixedUrl);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t hashIdx = PromiseFlatCString(aPageSpec).RFind("#");
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hash_idx"), hashIdx + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return the biggest icon close to the preferred width. It may be bigger
  // or smaller if the preferred width isn't found.
  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    int32_t width;
    rv = stmt->GetInt32(0, &width);
    if (!aIconData.spec.IsEmpty() && width < aPreferredWidth) {
      // We found the best match, or we already found a match so we don't need
      // to fallback to the root domain icon.
      break;
    }
    rv = stmt->GetUTF8String(1, aIconData.spec);
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
) : mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
      "AsyncFetchAndSetIconForPage::mCallback", aCallback))
  , mIcon(aIcon)
  , mPage(aPage)
  , mFaviconLoadPrivate(aFaviconLoadPrivate)
  , mLoadingPrincipal(new nsMainThreadPtrHolder<nsIPrincipal>(
      "AsyncFetchAndSetIconForPage::mLoadingPrincipal", aLoadingPrincipal))
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
  nsresult rv = FetchIconInfo(DB, 0, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInvalidIcon = !mIcon.payloads.Length() || PR_Now() > mIcon.expiration;
  bool fetchIconFromNetwork = mIcon.fetchMode == FETCH_ALWAYS ||
                              (mIcon.fetchMode == FETCH_IF_MISSING && isInvalidIcon);

  if (!fetchIconFromNetwork) {
    // There is already a valid icon or we don't want to fetch a new one,
    // directly proceed with association.
    RefPtr<AsyncAssociateIconToPage> event =
        new AsyncAssociateIconToPage(mIcon, mPage, mCallback);
    // We're already on the async thread.
    return event->Run();
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
  mIcon.payloads.Clear();

  IconPayload payload;
  mIcon.payloads.AppendElement(payload);

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
  MOZ_ASSERT(mIcon.payloads.Length() == 1);
  // Limit downloads to 500KB.
  const size_t kMaxDownloadSize = 500 * 1024;
  if (mIcon.payloads[0].data.Length() + aCount > kMaxDownloadSize) {
    mIcon.payloads.Clear();
    return NS_ERROR_FILE_TOO_BIG;
  }

  nsAutoCString buffer;
  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv)) {
    return rv;
  }

  if (!mIcon.payloads[0].data.Append(buffer, fallible)) {
    mIcon.payloads.Clear();
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
  if (NS_FAILED(aStatusCode) || mIcon.payloads.Length() == 0) {
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

  MOZ_ASSERT(mIcon.payloads.Length() == 1);
  IconPayload& payload = mIcon.payloads[0];

  nsAutoCString contentType;
  channel->GetContentType(contentType);
  // Bug 366324 - We don't want to sniff for SVG, so rely on server-specified type.
  if (contentType.EqualsLiteral(SVG_MIME_TYPE)) {
    payload.mimeType.AssignLiteral(SVG_MIME_TYPE);
    payload.width = UINT16_MAX;
  } else {
    NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, aRequest,
                    TO_INTBUFFER(payload.data), payload.data.Length(),
                    payload.mimeType);
  }

  // If the icon does not have a valid MIME type, add it to the failed cache.
  if (payload.mimeType.IsEmpty()) {
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
  if (payload.mimeType.EqualsLiteral(PNG_MIME_TYPE)) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_PNG_SIZES, payload.data.Length());
  }
  else if (payload.mimeType.EqualsLiteral("image/x-icon") ||
           payload.mimeType.EqualsLiteral("image/vnd.microsoft.icon")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_ICO_SIZES, payload.data.Length());
  }
  else if (payload.mimeType.EqualsLiteral("image/jpeg") ||
           payload.mimeType.EqualsLiteral("image/pjpeg")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_JPEG_SIZES, payload.data.Length());
  }
  else if (payload.mimeType.EqualsLiteral("image/gif")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_GIF_SIZES, payload.data.Length());
  }
  else if (payload.mimeType.EqualsLiteral("image/bmp") ||
           payload.mimeType.EqualsLiteral("image/x-windows-bmp")) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_BMP_SIZES, payload.data.Length());
  }
  else if (payload.mimeType.EqualsLiteral(SVG_MIME_TYPE)) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_SVG_SIZES, payload.data.Length());
  }
  else {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_OTHER_SIZES, payload.data.Length());
  }

  rv = favicons->OptimizeIconSizes(mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there's not valid payload, don't store the icon into to the database.
  if (mIcon.payloads.Length() == 0) {
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
  // May be created in both threads.
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

  bool shouldUpdateIcon = mIcon.status & ICON_STATUS_CHANGED;
  if (!shouldUpdateIcon) {
    for (const auto& payload : mIcon.payloads) {
      // If the entry is missing from the database, we should add it.
      if (payload.id == 0) {
        shouldUpdateIcon = true;
        break;
      }
    }
  }

  mozStorageTransaction transaction(DB->MainConn(), false,
                                    mozIStorageConnection::TRANSACTION_IMMEDIATE);

  if (shouldUpdateIcon) {
    rv = SetIconInfo(DB, mIcon);
    NS_ENSURE_SUCCESS(rv, rv);

    mIcon.status = (mIcon.status & ~(ICON_STATUS_CACHED)) | ICON_STATUS_SAVED;
  }

  // If the page does not have an id, don't try to insert a new one, cause we
  // don't know where the page comes from.  Not doing so we may end adding
  // a page that otherwise we'd explicitly ignore, like a POST or an error page.
  if (mPage.placeId == 0) {
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Don't associate pages to root domain icons, since those will be returned
  // regardless.  This saves a lot of work and database space since we don't
  // need to store urls and relations.
  // Though, this is possible only if both the page and the icon have the same
  // host, otherwise we couldn't relate them.
  if (!mIcon.rootIcon || !mIcon.host.Equals(mPage.host)) {
    // The page may have associated payloads already, and those could have to be
    // expired. For example at a certain point a page could decide to stop serving
    // its usual 16px and 32px pngs, and use an svg instead.
    // On the other side, we could also be in the process of adding more payloads
    // to this page, and we should not expire the payloads we just added.
    // For this, we use the expiration field as an indicator and remove relations
    // based on it being elapsed. We don't remove orphan icons at this time since
    // it would have a cost. The privacy hit is limited since history removal
    // methods already expire orphan icons.
    if (mPage.id != 0)  {
      nsCOMPtr<mozIStorageStatement> stmt;
      stmt = DB->GetStatement(
        "DELETE FROM moz_icons_to_pages "
        "WHERE icon_id IN ( "
          "SELECT icon_id FROM moz_icons_to_pages "
          "JOIN moz_icons i ON icon_id = i.id "
          "WHERE page_id = :page_id "
            "AND expire_ms < strftime('%s','now','localtime','start of day','-7 days','utc') * 1000 "
        ") AND page_id = :page_id "
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPage.id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // We need to create the page entry.
      nsCOMPtr<mozIStorageStatement> stmt;
      stmt = DB->GetStatement(
        "INSERT OR IGNORE INTO moz_pages_w_icons (page_url, page_url_hash) "
        "VALUES (:page_url, hash(:page_url)) "
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mPage.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Then we can create the relations.
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = DB->GetStatement(
      "INSERT OR IGNORE INTO moz_icons_to_pages (page_id, icon_id) "
      "VALUES ((SELECT id from moz_pages_w_icons WHERE page_url_hash = hash(:page_url) AND page_url = :page_url), "
              ":icon_id) "
    );
    NS_ENSURE_STATE(stmt);

    // For some reason using BindingParamsArray here fails execution, so we must
    // execute the statements one by one.
    // In the future we may want to investigate the reasons, sounds like related
    // to contraints.
    for (const auto& payload : mIcon.payloads) {
      mozStorageStatementScoper scoper(stmt);
      nsCOMPtr<mozIStorageBindingParams> params;
      rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mPage.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("icon_id"), payload.id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mIcon.status |= ICON_STATUS_ASSOCIATED;

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
, const nsACString& aPageHost
, uint16_t aPreferredWidth
, nsIFaviconDataCallback* aCallback
) : mPreferredWidth(aPreferredWidth == 0 ? UINT16_MAX : aPreferredWidth)
  , mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
      "AsyncGetFaviconURLForPage::mCallback", aCallback))
{
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
  mPageHost.Assign(aPageHost);
}

NS_IMETHODIMP
AsyncGetFaviconURLForPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  IconData iconData;
  nsresult rv = FetchIconPerSpec(DB, mPageSpec, mPageHost, iconData, mPreferredWidth);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now notify our callback of the icon spec we retrieved, even if empty.
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
, const nsACString& aPageHost
,  uint16_t aPreferredWidth
, nsIFaviconDataCallback* aCallback
) : mPreferredWidth(aPreferredWidth == 0 ? UINT16_MAX : aPreferredWidth)
  , mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
      "AsyncGetFaviconDataForPage::mCallback", aCallback))
 {
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
  mPageHost.Assign(aPageHost);
}

NS_IMETHODIMP
AsyncGetFaviconDataForPage::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  IconData iconData;
  nsresult rv = FetchIconPerSpec(DB, mPageSpec, mPageHost, iconData, mPreferredWidth);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!iconData.spec.IsEmpty()) {
    rv = FetchIconInfo(DB, mPreferredWidth, iconData);
    if (NS_FAILED(rv)) {
      iconData.spec.Truncate();
    }
  }

  PageData pageData;
  pageData.spec.Assign(mPageSpec);

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
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncReplaceFaviconData::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);

  mozStorageTransaction transaction(DB->MainConn(), false,
                                    mozIStorageConnection::TRANSACTION_IMMEDIATE);
  nsresult rv = SetIconInfo(DB, mIcon, true);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // There's no previous icon to replace, we don't need to do anything.
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  rv = transaction.Commit();
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

  if (!mCallback) {
    return NS_OK;
  }

  if (mIcon.payloads.Length() > 0) {
    IconPayload& payload = mIcon.payloads[0];
    return mCallback->OnComplete(iconURI, payload.data.Length(),
                                 TO_INTBUFFER(payload.data), payload.mimeType,
                                 payload.width);
  }
  return mCallback->OnComplete(iconURI, 0, TO_INTBUFFER(EmptyCString()),
                               EmptyCString(), 0);
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

  // We're done. Remove any leftovers.
  rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_icons WHERE typeof(width) = 'text'"
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
                                           AcceptedMimeTypes::IMAGES_AND_DOCUMENTS)) {
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

////////////////////////////////////////////////////////////////////////////////
//// AsyncCopyFavicons

AsyncCopyFavicons::AsyncCopyFavicons(
  PageData& aFromPage
, PageData& aToPage
, nsIFaviconDataCallback* aCallback
) : mFromPage(aFromPage)
  , mToPage(aToPage)
  , mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
      "AsyncCopyFavicons::mCallback", aCallback))
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncCopyFavicons::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  IconData icon;

  // Ensure we'll callback and dispatch notifications to the main-thread.
  auto cleanup = MakeScopeExit([&] () {
    // If we bailed out early, just return a null icon uri, since we didn't
    // copy anything.
    if (!(icon.status & ICON_STATUS_ASSOCIATED)) {
      icon.spec.Truncate();
    }
    nsCOMPtr<nsIRunnable> event = new NotifyIconObservers(icon, mToPage, mCallback);
    NS_DispatchToMainThread(event);
  });

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);

  nsresult rv = FetchPageInfo(DB, mToPage);
  if (rv == NS_ERROR_NOT_AVAILABLE || !mToPage.placeId) {
    // We have never seen this page, or we can't add this page to history and
    // and it's not a bookmark. We won't add the page.
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Get just one icon, to check whether the page has any, and to notify later.
  rv = FetchIconPerSpec(DB, mFromPage.spec, EmptyCString(), icon, UINT16_MAX);
  NS_ENSURE_SUCCESS(rv, rv);

  if (icon.spec.IsEmpty()) {
    // There's nothing to copy.
    return NS_OK;
  }

  // Insert an entry in moz_pages_w_icons if needed.
  if (!mToPage.id) {
    // We need to create the page entry.
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = DB->GetStatement(
      "INSERT OR IGNORE INTO moz_pages_w_icons (page_url, page_url_hash) "
      "VALUES (:page_url, hash(:page_url)) "
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mToPage.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
    // Required to to fetch the id and the guid.
    rv = FetchPageInfo(DB, mToPage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create the relations.
  nsCOMPtr<mozIStorageStatement> stmt = DB->GetStatement(
    "INSERT OR IGNORE INTO moz_icons_to_pages (page_id, icon_id) "
      "SELECT :id, icon_id "
      "FROM moz_icons_to_pages "
      "WHERE page_id = (SELECT id FROM moz_pages_w_icons WHERE page_url_hash = hash(:url) AND page_url = :url) "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mToPage.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("url"), mFromPage.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Setting this will make us send pageChanged notifications.
  // The scope exit will take care of the callback and notifications.
  icon.status |= ICON_STATUS_ASSOCIATED;

  return NS_OK;
}

} // namespace places
} // namespace mozilla
