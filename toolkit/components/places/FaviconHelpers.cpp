/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FaviconHelpers.h"

#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIClassOfService.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIHttpChannel.h"
#include "nsIPrincipal.h"

#include "nsComponentManagerUtils.h"
#include "nsNavHistory.h"
#include "nsFaviconService.h"

#include "mozilla/dom/PlacesFavicon.h"
#include "mozilla/dom/PlacesObservers.h"
#include "mozilla/storage.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Telemetry.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsISupportsPriority.h"
#include <algorithm>
#include <deque>
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "imgIEncoder.h"

using namespace mozilla;
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
nsresult FetchPageInfo(const RefPtr<Database>& aDB, PageData& _page) {
  MOZ_ASSERT(_page.spec.Length(), "Must have a non-empty spec!");
  MOZ_ASSERT(!NS_IsMainThread());

  // The subquery finds the bookmarked uri we want to set the icon for,
  // walking up redirects.
  nsCString query = nsPrintfCString(
      "SELECT h.id, pi.id, h.guid, ( "
      "WITH RECURSIVE "
      "destinations(visit_type, from_visit, place_id, rev_host, bm) AS ( "
      "SELECT v.visit_type, v.from_visit, p.id, p.rev_host, b.id "
      "FROM moz_places p  "
      "LEFT JOIN moz_historyvisits v ON v.place_id = p.id  "
      "LEFT JOIN moz_bookmarks b ON b.fk = p.id "
      "WHERE p.id = h.id "
      "UNION "
      "SELECT src.visit_type, src.from_visit, src.place_id, p.rev_host, b.id "
      "FROM moz_places p "
      "JOIN moz_historyvisits src ON src.place_id = p.id "
      "JOIN destinations dest ON dest.from_visit = src.id AND dest.visit_type "
      "IN (%d, %d) "
      "LEFT JOIN moz_bookmarks b ON b.fk = src.place_id "
      "WHERE instr(p.rev_host, dest.rev_host) = 1 "
      "OR instr(dest.rev_host, p.rev_host) = 1 "
      ") "
      "SELECT url "
      "FROM moz_places p "
      "JOIN destinations r ON r.place_id = p.id "
      "WHERE bm NOTNULL "
      "LIMIT 1 "
      "), fixup_url(get_unreversed_host(h.rev_host)) AS host "
      "FROM moz_places h "
      "LEFT JOIN moz_pages_w_icons pi ON page_url_hash = hash(:page_url) AND "
      "page_url = :page_url "
      "WHERE h.url_hash = hash(:page_url) AND h.url = :page_url",
      nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
      nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);

  nsCOMPtr<mozIStorageStatement> stmt = aDB->GetStatement(query);
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, "page_url"_ns, _page.spec);
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
    } else {
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
nsresult SetIconInfo(const RefPtr<Database>& aDB, IconData& aIcon,
                     bool aMustReplace = false) {
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
      "AND icon_url = :url ");
  NS_ENSURE_STATE(selectStmt);
  mozStorageStatementScoper scoper(selectStmt);
  nsresult rv = URIBinder::Bind(selectStmt, "url"_ns, aIcon.spec);
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
      "VALUES (:url, hash(fixup_url(:url)), :width, :root, :expire, :data) ");
  NS_ENSURE_STATE(insertStmt);
  // ReplaceFaviconData may replace data for an already existing icon, and in
  // that case it won't have the page uri at hand, thus it can't tell if the
  // icon is a root icon or not. For that reason, never overwrite a root = 1.
  nsCOMPtr<mozIStorageStatement> updateStmt = aDB->GetStatement(
      "UPDATE moz_icons SET width = :width, "
      "expire_ms = :expire, "
      "data = :data, "
      "root = (root  OR :root) "
      "WHERE id = :id ");
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
      rv = updateStmt->BindInt64ByName("id"_ns, id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt32ByName("width"_ns, payload.width);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt64ByName("expire"_ns, aIcon.expiration / 1000);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindInt32ByName("root"_ns, aIcon.rootIcon);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->BindBlobByName("data"_ns, TO_INTBUFFER(payload.data),
                                      payload.data.Length());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = updateStmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
      // Set the new payload id.
      payload.id = id;
    } else {
      // Insert a new entry.
      mozStorageStatementScoper scoper(insertStmt);
      rv = URIBinder::Bind(insertStmt, "url"_ns, aIcon.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindInt32ByName("width"_ns, payload.width);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = insertStmt->BindInt32ByName("root"_ns, aIcon.rootIcon);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindInt64ByName("expire"_ns, aIcon.expiration / 1000);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = insertStmt->BindBlobByName("data"_ns, TO_INTBUFFER(payload.data),
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
    sql.AppendLiteral(" 0)");  // Non-existing id to match the trailing comma.
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
nsresult FetchIconInfo(const RefPtr<Database>& aDB, uint16_t aPreferredWidth,
                       IconData& _icon) {
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
      "ORDER BY width DESC ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  DebugOnly<nsresult> rv = URIBinder::Bind(stmt, "url"_ns, _icon.spec);
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

nsresult FetchIconPerSpec(const RefPtr<Database>& aDB,
                          const nsACString& aPageSpec,
                          const nsACString& aPageHost, IconData& aIconData,
                          uint16_t aPreferredWidth) {
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
      "ORDER BY width DESC, root ASC ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, "url"_ns, aPageSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString rootIconFixedUrl(aPageHost);
  if (!rootIconFixedUrl.IsEmpty()) {
    rootIconFixedUrl.AppendLiteral("/favicon.ico");
  }
  rv = stmt->BindUTF8StringByName("root_icon_url"_ns, rootIconFixedUrl);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t hashIdx = PromiseFlatCString(aPageSpec).RFind("#");
  rv = stmt->BindInt32ByName("hash_idx"_ns, hashIdx + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return the biggest icon close to the preferred width. It may be bigger
  // or smaller if the preferred width isn't found.
  bool hasResult;
  int32_t lastWidth = 0;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    int32_t width;
    rv = stmt->GetInt32(0, &width);
    if (lastWidth == width) {
      // We already found an icon for this width. We always prefer the first
      // icon found, because it's a non-root icon, per the root ASC ordering.
      continue;
    }
    if (!aIconData.spec.IsEmpty() && width < aPreferredWidth) {
      // We found the best match, or we already found a match so we don't need
      // to fallback to the root domain icon.
      break;
    }
    lastWidth = width;
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
PRTime GetExpirationTimeFromChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  // Attempt to get an expiration time from the cache.  If this fails, we'll
  // make one up.
  PRTime now = PR_Now();
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
        expiration = now + std::min((PRTime)seconds * PR_USEC_PER_SEC,
                                    MAX_FAVICON_EXPIRATION);
      }
    }
  }
  // If we did not obtain a time from the cache, use the cap value.
  return expiration < now + MIN_FAVICON_EXPIRATION
             ? now + MAX_FAVICON_EXPIRATION
             : expiration;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//// AsyncFetchAndSetIconForPage

NS_IMPL_ISUPPORTS_INHERITED(AsyncFetchAndSetIconForPage, Runnable,
                            nsIStreamListener, nsIInterfaceRequestor,
                            nsIChannelEventSink, mozIPlacesPendingOperation)

AsyncFetchAndSetIconForPage::AsyncFetchAndSetIconForPage(
    IconData& aIcon, PageData& aPage, bool aFaviconLoadPrivate,
    nsIFaviconDataCallback* aCallback, nsIPrincipal* aLoadingPrincipal,
    uint64_t aRequestContextID)
    : Runnable("places::AsyncFetchAndSetIconForPage"),
      mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
          "AsyncFetchAndSetIconForPage::mCallback", aCallback)),
      mIcon(aIcon),
      mPage(aPage),
      mFaviconLoadPrivate(aFaviconLoadPrivate),
      mLoadingPrincipal(new nsMainThreadPtrHolder<nsIPrincipal>(
          "AsyncFetchAndSetIconForPage::mLoadingPrincipal", aLoadingPrincipal)),
      mCanceled(false),
      mRequestContextID(aRequestContextID) {
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  // Try to fetch the icon from the database.
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsresult rv = FetchIconInfo(DB, 0, mIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInvalidIcon = !mIcon.payloads.Length() || PR_Now() > mIcon.expiration;
  bool fetchIconFromNetwork =
      mIcon.fetchMode == FETCH_ALWAYS ||
      (mIcon.fetchMode == FETCH_IF_MISSING && isInvalidIcon);

  // Check if we can associate the icon to this page.
  rv = FetchPageInfo(DB, mPage);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // We have never seen this page.  If we can add the page to history,
      // we will try to do it later, otherwise just bail out.
      if (!mPage.canAddToHistory) {
        return NS_OK;
      }
    }
    return rv;
  }

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
      NewRunnableMethod("places::AsyncFetchAndSetIconForPage::FetchFromNetwork",
                        this, &AsyncFetchAndSetIconForPage::FetchFromNetwork);
  return NS_DispatchToMainThread(event);
}

nsresult AsyncFetchAndSetIconForPage::FetchFromNetwork() {
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
  rv = NS_NewChannel(getter_AddRefs(channel), iconURI, mLoadingPrincipal,
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT |
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

  if (StaticPrefs::network_http_tailing_enabled()) {
    nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(channel);
    if (cos) {
      cos->AddClassFlags(nsIClassOfService::Tail |
                         nsIClassOfService::Throttleable);
    }

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
      Unused << httpChannel->SetRequestContextID(mRequestContextID);
    }
  }

  rv = channel->AsyncOpen(this);
  if (NS_SUCCEEDED(rv)) {
    mRequest = channel;
  }
  return rv;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mCanceled) {
    return NS_ERROR_UNEXPECTED;
  }
  mCanceled = true;
  if (mRequest) {
    mRequest->CancelWithReason(NS_BINDING_ABORTED,
                               "AsyncFetchAndSetIconForPage::Cancel"_ns);
  }
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnStartRequest(nsIRequest* aRequest) {
  // mRequest should already be set from ::FetchFromNetwork, but in the case of
  // a redirect we might get a new request, and we should make sure we keep a
  // reference to the most current request.
  mRequest = aRequest;
  if (mCanceled) {
    mRequest->Cancel(NS_BINDING_ABORTED);
  }
  // Don't store icons responding with Cache-Control: no-store, but always
  // allow root domain icons.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (httpChannel) {
    bool isNoStore;
    nsAutoCString path;
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(httpChannel->GetURI(getter_AddRefs(uri))) &&
        NS_SUCCEEDED(uri->GetFilePath(path)) &&
        !path.EqualsLiteral("/favicon.ico") &&
        NS_SUCCEEDED(httpChannel->IsNoStoreResponse(&isNoStore)) && isNoStore) {
      // Abandon the network fetch.
      mRequest->CancelWithReason(
          NS_BINDING_ABORTED, "AsyncFetchAndSetIconForPage::OnStartRequest"_ns);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnDataAvailable(nsIRequest* aRequest,
                                             nsIInputStream* aInputStream,
                                             uint64_t aOffset,
                                             uint32_t aCount) {
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
AsyncFetchAndSetIconForPage::GetInterface(const nsIID& uuid, void** aResult) {
  return QueryInterface(uuid, aResult);
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::AsyncOnChannelRedirect(
    nsIChannel* oldChannel, nsIChannel* newChannel, uint32_t flags,
    nsIAsyncVerifyRedirectCallback* cb) {
  // If we've been canceled, stop the redirect with NS_BINDING_ABORTED, and
  // handle the cancel on the original channel.
  (void)cb->OnRedirectVerifyCallback(mCanceled ? NS_BINDING_ABORTED : NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
AsyncFetchAndSetIconForPage::OnStopRequest(nsIRequest* aRequest,
                                           nsresult aStatusCode) {
  MOZ_ASSERT(NS_IsMainThread());

  // Don't need to track this anymore.
  mRequest = nullptr;
  if (mCanceled) {
    return NS_OK;
  }

  nsFaviconService* favicons = nsFaviconService::GetFaviconService();
  NS_ENSURE_STATE(favicons);

  nsresult rv;

  // If fetching the icon failed, bail out.
  if (NS_FAILED(aStatusCode) || mIcon.payloads.Length() == 0) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  // aRequest should always QI to nsIChannel.
  MOZ_ASSERT(channel);

  MOZ_ASSERT(mIcon.payloads.Length() == 1);
  IconPayload& payload = mIcon.payloads[0];

  nsAutoCString contentType;
  channel->GetContentType(contentType);
  // Bug 366324 - We don't want to sniff for SVG, so rely on server-specified
  // type.
  if (contentType.EqualsLiteral(SVG_MIME_TYPE)) {
    payload.mimeType.AssignLiteral(SVG_MIME_TYPE);
    payload.width = UINT16_MAX;
  } else {
    NS_SniffContent(NS_DATA_SNIFFER_CATEGORY, aRequest,
                    TO_INTBUFFER(payload.data), payload.data.Length(),
                    payload.mimeType);
  }

  // If the icon does not have a valid MIME type, bail out.
  if (payload.mimeType.IsEmpty()) {
    return NS_OK;
  }

  mIcon.expiration = GetExpirationTimeFromChannel(channel);

  // Telemetry probes to measure the favicon file sizes for each different file
  // type. This allow us to measure common file sizes while also observing each
  // type popularity.
  if (payload.mimeType.EqualsLiteral(PNG_MIME_TYPE)) {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_PNG_SIZES,
                          payload.data.Length());
  } else if (payload.mimeType.EqualsLiteral("image/x-icon") ||
             payload.mimeType.EqualsLiteral("image/vnd.microsoft.icon")) {
    Telemetry::Accumulate(mozilla::Telemetry::PLACES_FAVICON_ICO_SIZES,
                          payload.data.Length());
  } else if (payload.mimeType.EqualsLiteral("image/jpeg") ||
             payload.mimeType.EqualsLiteral("image/pjpeg")) {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_JPEG_SIZES,
                          payload.data.Length());
  } else if (payload.mimeType.EqualsLiteral("image/gif")) {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_GIF_SIZES,
                          payload.data.Length());
  } else if (payload.mimeType.EqualsLiteral("image/bmp") ||
             payload.mimeType.EqualsLiteral("image/x-windows-bmp")) {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_BMP_SIZES,
                          payload.data.Length());
  } else if (payload.mimeType.EqualsLiteral(SVG_MIME_TYPE)) {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_SVG_SIZES,
                          payload.data.Length());
  } else {
    Telemetry::Accumulate(Telemetry::PLACES_FAVICON_OTHER_SIZES,
                          payload.data.Length());
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
    const IconData& aIcon, const PageData& aPage,
    const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback)
    : Runnable("places::AsyncAssociateIconToPage"),
      mCallback(aCallback),
      mIcon(aIcon),
      mPage(aPage) {
  // May be created in both threads.
}

NS_IMETHODIMP
AsyncAssociateIconToPage::Run() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mPage.guid.IsEmpty(),
             "Page info should have been fetched already");
  MOZ_ASSERT(mPage.canAddToHistory || !mPage.bookmarkedSpec.IsEmpty(),
             "The page should be addable to history or a bookmark");

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

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);

  mozStorageTransaction transaction(
      DB->MainConn(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  nsresult rv;
  if (shouldUpdateIcon) {
    rv = SetIconInfo(DB, mIcon);
    if (NS_FAILED(rv)) {
      (void)transaction.Commit();
      return rv;
    }

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

  // Expire old favicons to keep up with website changes. Associated icons must
  // be expired also when storing a root favicon, because a page may change to
  // only have a root favicon.
  // Note that here we could also be in the process of adding further payloads
  // to a page, and we don't want to expire just added payloads. For this
  // reason we only remove expired payloads.
  // Oprhan icons are not removed at this time because it'd be expensive. The
  // privacy implications are limited, since history removal methods also expire
  // orphan icons.
  if (mPage.id > 0) {
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = DB->GetStatement(
        "DELETE FROM moz_icons_to_pages "
        "WHERE page_id = :page_id "
        "AND expire_ms < strftime('%s','now','localtime','utc') * 1000 ");
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->BindInt64ByName("page_id"_ns, mPage.id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Don't associate pages to root domain icons, since those will be returned
  // regardless.  This saves a lot of work and database space since we don't
  // need to store urls and relations.
  // Though, this is possible only if both the page and the icon have the same
  // host, otherwise we couldn't relate them.
  if (!mIcon.rootIcon || !mIcon.host.Equals(mPage.host)) {
    // The page may have associated payloads already, and those could have to be
    // expired. For example at a certain point a page could decide to stop
    // serving its usual 16px and 32px pngs, and use an svg instead. On the
    // other side, we could also be in the process of adding more payloads to
    // this page, and we should not expire the payloads we just added. For this,
    // we use the expiration field as an indicator and remove relations based on
    // it being elapsed. We don't remove orphan icons at this time since it
    // would have a cost. The privacy hit is limited since history removal
    // methods already expire orphan icons.
    if (mPage.id == 0) {
      // We need to create the page entry.
      nsCOMPtr<mozIStorageStatement> stmt;
      stmt = DB->GetStatement(
          "INSERT OR IGNORE INTO moz_pages_w_icons (page_url, page_url_hash) "
          "VALUES (:page_url, hash(:page_url)) ");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      rv = URIBinder::Bind(stmt, "page_url"_ns, mPage.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Then we can create the relations.
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = DB->GetStatement(
        "INSERT INTO moz_icons_to_pages (page_id, icon_id, expire_ms) "
        "VALUES ((SELECT id from moz_pages_w_icons WHERE page_url_hash = "
        "hash(:page_url) AND page_url = :page_url), "
        ":icon_id, :expire) "
        "ON CONFLICT(page_id, icon_id) DO "
        "UPDATE SET expire_ms = :expire ");
    NS_ENSURE_STATE(stmt);

    // For some reason using BindingParamsArray here fails execution, so we must
    // execute the statements one by one.
    // In the future we may want to investigate the reasons, sounds like related
    // to contraints.
    for (const auto& payload : mIcon.payloads) {
      mozStorageStatementScoper scoper(stmt);
      nsCOMPtr<mozIStorageBindingParams> params;
      rv = URIBinder::Bind(stmt, "page_url"_ns, mPage.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByName("icon_id"_ns, payload.id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByName("expire"_ns, mIcon.expiration / 1000);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mIcon.status |= ICON_STATUS_ASSOCIATED;

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, dispatch an event to the main thread to notify observers.
  nsCOMPtr<nsIRunnable> event =
      new NotifyIconObservers(mIcon, mPage, mCallback);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is a bookmarked page that redirects to this one, try to update its
  // icon as well.
  if (!mPage.bookmarkedSpec.IsEmpty() &&
      !mPage.bookmarkedSpec.Equals(mPage.spec)) {
    // Create a new page struct to avoid polluting it with old data.
    PageData bookmarkedPage;
    bookmarkedPage.spec = mPage.bookmarkedSpec;
    RefPtr<Database> DB = Database::GetDatabase();
    if (DB && NS_SUCCEEDED(FetchPageInfo(DB, bookmarkedPage))) {
      // This will be silent, so be sure to not pass in the current callback.
      nsMainThreadPtrHandle<nsIFaviconDataCallback> nullCallback;
      RefPtr<AsyncAssociateIconToPage> event =
          new AsyncAssociateIconToPage(mIcon, bookmarkedPage, nullCallback);
      Unused << event->Run();
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncGetFaviconURLForPage

AsyncGetFaviconURLForPage::AsyncGetFaviconURLForPage(
    const nsACString& aPageSpec, const nsACString& aPageHost,
    uint16_t aPreferredWidth, nsIFaviconDataCallback* aCallback)
    : Runnable("places::AsyncGetFaviconURLForPage"),
      mPreferredWidth(aPreferredWidth == 0 ? UINT16_MAX : aPreferredWidth),
      mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
          "AsyncGetFaviconURLForPage::mCallback", aCallback)) {
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
  mPageHost.Assign(aPageHost);
}

NS_IMETHODIMP
AsyncGetFaviconURLForPage::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  IconData iconData;
  nsresult rv =
      FetchIconPerSpec(DB, mPageSpec, mPageHost, iconData, mPreferredWidth);
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
    const nsACString& aPageSpec, const nsACString& aPageHost,
    uint16_t aPreferredWidth, nsIFaviconDataCallback* aCallback)
    : Runnable("places::AsyncGetFaviconDataForPage"),
      mPreferredWidth(aPreferredWidth == 0 ? UINT16_MAX : aPreferredWidth),
      mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
          "AsyncGetFaviconDataForPage::mCallback", aCallback)) {
  MOZ_ASSERT(NS_IsMainThread());
  mPageSpec.Assign(aPageSpec);
  mPageHost.Assign(aPageHost);
}

NS_IMETHODIMP
AsyncGetFaviconDataForPage::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  IconData iconData;
  nsresult rv =
      FetchIconPerSpec(DB, mPageSpec, mPageHost, iconData, mPreferredWidth);
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

AsyncReplaceFaviconData::AsyncReplaceFaviconData(const IconData& aIcon)
    : Runnable("places::AsyncReplaceFaviconData"), mIcon(aIcon) {
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncReplaceFaviconData::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);

  mozStorageTransaction transaction(
      DB->MainConn(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  nsresult rv = SetIconInfo(DB, mIcon, true);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // There's no previous icon to replace, we don't need to do anything.
    (void)transaction.Commit();
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // We can invalidate the cache version since we now persist the icon.
  nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
      "places::AsyncReplaceFaviconData::RemoveIconDataCacheEntry", this,
      &AsyncReplaceFaviconData::RemoveIconDataCacheEntry);
  rv = NS_DispatchToMainThread(event);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult AsyncReplaceFaviconData::RemoveIconDataCacheEntry() {
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
    const IconData& aIcon, const PageData& aPage,
    const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback)
    : Runnable("places::NotifyIconObservers"),
      mCallback(aCallback),
      mIcon(aIcon),
      mPage(aPage) {}

// MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is marked
// MOZ_CAN_RUN_SCRIPT.  See bug 1535398.
MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
NotifyIconObservers::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> iconURI;
  if (!mIcon.spec.IsEmpty()) {
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(iconURI), mIcon.spec));
    if (iconURI) {
      // Notify observers only if something changed.
      if (mIcon.status & ICON_STATUS_SAVED ||
          mIcon.status & ICON_STATUS_ASSOCIATED) {
        nsCOMPtr<nsIURI> pageURI;
        MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(pageURI), mPage.spec));
        if (pageURI) {
          // Invalide page-icon image cache, since the icon is about to change.
          nsFaviconService* favicons = nsFaviconService::GetFaviconService();
          MOZ_ASSERT(favicons);
          if (favicons) {
            nsCString pageIconSpec("page-icon:");
            pageIconSpec.Append(mPage.spec);
            nsCOMPtr<nsIURI> pageIconURI;
            if (NS_SUCCEEDED(
                    NS_NewURI(getter_AddRefs(pageIconURI), pageIconSpec))) {
              favicons->ClearImageCache(pageIconURI);
            }
          }

          // Notify about the favicon change.
          dom::Sequence<OwningNonNull<dom::PlacesEvent>> events;
          RefPtr<dom::PlacesFavicon> faviconEvent = new dom::PlacesFavicon();
          AppendUTF8toUTF16(mPage.spec, faviconEvent->mUrl);
          AppendUTF8toUTF16(mIcon.spec, faviconEvent->mFaviconUrl);
          faviconEvent->mPageGuid.Assign(mPage.guid);
          bool success =
              !!events.AppendElement(faviconEvent.forget(), fallible);
          MOZ_RELEASE_ASSERT(success);
          dom::PlacesObservers::NotifyListeners(events);
        }
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
  return mCallback->OnComplete(iconURI, 0, TO_INTBUFFER(EmptyCString()), ""_ns,
                               0);
}

////////////////////////////////////////////////////////////////////////////////
//// AsyncCopyFavicons

AsyncCopyFavicons::AsyncCopyFavicons(PageData& aFromPage, PageData& aToPage,
                                     nsIFaviconDataCallback* aCallback)
    : Runnable("places::AsyncCopyFavicons"),
      mFromPage(aFromPage),
      mToPage(aToPage),
      mCallback(new nsMainThreadPtrHolder<nsIFaviconDataCallback>(
          "AsyncCopyFavicons::mCallback", aCallback)) {
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMETHODIMP
AsyncCopyFavicons::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  IconData icon;

  // Ensure we'll callback and dispatch notifications to the main-thread.
  auto cleanup = MakeScopeExit([&]() {
    // If we bailed out early, just return a null icon uri, since we didn't
    // copy anything.
    if (!(icon.status & ICON_STATUS_ASSOCIATED)) {
      icon.spec.Truncate();
    }
    nsCOMPtr<nsIRunnable> event =
        new NotifyIconObservers(icon, mToPage, mCallback);
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
  rv = FetchIconPerSpec(DB, mFromPage.spec, ""_ns, icon, UINT16_MAX);
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
        "VALUES (:page_url, hash(:page_url)) ");
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = URIBinder::Bind(stmt, "page_url"_ns, mToPage.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
    // Required to to fetch the id and the guid.
    rv = FetchPageInfo(DB, mToPage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create the relations.
  nsCOMPtr<mozIStorageStatement> stmt = DB->GetStatement(
      "INSERT OR IGNORE INTO moz_icons_to_pages (page_id, icon_id, expire_ms) "
      "SELECT :id, icon_id, expire_ms "
      "FROM moz_icons_to_pages "
      "WHERE page_id = (SELECT id FROM moz_pages_w_icons WHERE page_url_hash = "
      "hash(:url) AND page_url = :url) ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);
  rv = stmt->BindInt64ByName("id"_ns, mToPage.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, "url"_ns, mFromPage.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Setting this will make us send pageChanged notifications.
  // The scope exit will take care of the callback and notifications.
  icon.status |= ICON_STATUS_ASSOCIATED;

  return NS_OK;
}

}  // namespace places
}  // namespace mozilla
