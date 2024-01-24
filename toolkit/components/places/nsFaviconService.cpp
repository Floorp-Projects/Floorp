/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the favicon service, which stores favicons for web pages with your
 * history as you browse. It is also used to save the favicons for bookmarks.
 *
 * DANGER: The history query system makes assumptions about the favicon storage
 * so that icons can be quickly generated for history/bookmark result sets. If
 * you change the database layout at all, you will have to update both services.
 */

#include "nsFaviconService.h"

#include "nsNavHistory.h"
#include "nsPlacesMacros.h"
#include "Helpers.h"

#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "plbase64.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "nsILoadInfo.h"
#include "nsIContentPolicy.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "imgICache.h"

#define UNASSOCIATED_FAVICONS_LENGTH 32

// When replaceFaviconData is called, we store the icons in an in-memory cache
// instead of in storage. Icons in the cache are expired according to this
// interval.
#define UNASSOCIATED_ICON_EXPIRY_INTERVAL 60000

using namespace mozilla;
using namespace mozilla::places;

const uint16_t gFaviconSizes[7] = {192, 144, 96, 64, 48, 32, 16};

/**
 * Used to notify a topic to system observers on async execute completion.
 * Will throw on error.
 */
class ExpireFaviconsStatementCallbackNotifier : public AsyncStatementCallback {
 public:
  ExpireFaviconsStatementCallbackNotifier();
  NS_IMETHOD HandleCompletion(uint16_t aReason) override;
};

namespace {

/**
 * Extracts and filters native sizes from the given container, based on the
 * list of sizes we are supposed to retain.
 * All calculation is done considering square sizes and the largest side.
 * In case of multiple frames of the same size, only the first one is retained.
 */
nsresult GetFramesInfoForContainer(imgIContainer* aContainer,
                                   nsTArray<FrameData>& aFramesInfo) {
  // Don't extract frames from animated images.
  bool animated;
  nsresult rv = aContainer->GetAnimated(&animated);
  if (NS_FAILED(rv) || !animated) {
    nsTArray<nsIntSize> nativeSizes;
    rv = aContainer->GetNativeSizes(nativeSizes);
    if (NS_SUCCEEDED(rv) && nativeSizes.Length() > 1) {
      for (uint32_t i = 0; i < nativeSizes.Length(); ++i) {
        nsIntSize nativeSize = nativeSizes[i];
        // Only retain square frames.
        if (nativeSize.width != nativeSize.height) {
          continue;
        }
        // Check if it's one of the sizes we care about.
        const auto* end = std::end(gFaviconSizes);
        const uint16_t* matchingSize =
            std::find(std::begin(gFaviconSizes), end, nativeSize.width);
        if (matchingSize != end) {
          // We must avoid duped sizes, an image could contain multiple frames
          // of the same size, but we can only store one. We could use an
          // hashtable, but considered the average low number of frames, we'll
          // just do a linear search.
          bool dupe = false;
          for (const auto& frameInfo : aFramesInfo) {
            if (frameInfo.width == *matchingSize) {
              dupe = true;
              break;
            }
          }
          if (!dupe) {
            aFramesInfo.AppendElement(FrameData(i, *matchingSize));
          }
        }
      }
    }
  }

  if (aFramesInfo.Length() == 0) {
    // Always have at least the default size.
    int32_t width;
    rv = aContainer->GetWidth(&width);
    NS_ENSURE_SUCCESS(rv, rv);
    int32_t height;
    rv = aContainer->GetHeight(&height);
    NS_ENSURE_SUCCESS(rv, rv);
    // For non-square images, pick the largest side.
    aFramesInfo.AppendElement(FrameData(0, std::max(width, height)));
  }
  return NS_OK;
}

}  // namespace

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsFaviconService, gFaviconService)

NS_IMPL_CLASSINFO(nsFaviconService, nullptr, 0, NS_FAVICONSERVICE_CID)
NS_IMPL_ISUPPORTS_CI(nsFaviconService, nsIFaviconService, nsITimerCallback,
                     nsINamed)

nsFaviconService::nsFaviconService()
    : mUnassociatedIcons(UNASSOCIATED_FAVICONS_LENGTH),
      mDefaultIconURIPreferredSize(UINT16_MAX) {
  NS_ASSERTION(!gFaviconService,
               "Attempting to create two instances of the service!");
  gFaviconService = this;
}

nsFaviconService::~nsFaviconService() {
  NS_ASSERTION(gFaviconService == this,
               "Deleting a non-singleton instance of the service");
  if (gFaviconService == this) gFaviconService = nullptr;
}

Atomic<int64_t> nsFaviconService::sLastInsertedIconId(0);

void  // static
nsFaviconService::StoreLastInsertedId(const nsACString& aTable,
                                      const int64_t aLastInsertedId) {
  MOZ_ASSERT(aTable.EqualsLiteral("moz_icons"));
  sLastInsertedIconId = aLastInsertedId;
}

nsresult nsFaviconService::Init() {
  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  mExpireUnassociatedIconsTimer = NS_NewTimer();
  NS_ENSURE_STATE(mExpireUnassociatedIconsTimer);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ExpireAllFavicons() {
  NS_ENSURE_STATE(mDB);

  nsCOMPtr<mozIStorageAsyncStatement> removePagesStmt =
      mDB->GetAsyncStatement("DELETE FROM moz_pages_w_icons");
  NS_ENSURE_STATE(removePagesStmt);
  nsCOMPtr<mozIStorageAsyncStatement> removeIconsStmt =
      mDB->GetAsyncStatement("DELETE FROM moz_icons");
  NS_ENSURE_STATE(removeIconsStmt);
  nsCOMPtr<mozIStorageAsyncStatement> unlinkIconsStmt =
      mDB->GetAsyncStatement("DELETE FROM moz_icons_to_pages");
  NS_ENSURE_STATE(unlinkIconsStmt);

  nsTArray<RefPtr<mozIStorageBaseStatement>> stmts = {
      ToRefPtr(std::move(removePagesStmt)),
      ToRefPtr(std::move(removeIconsStmt)),
      ToRefPtr(std::move(unlinkIconsStmt))};
  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  if (!conn) {
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<mozIStoragePendingStatement> ps;
  RefPtr<ExpireFaviconsStatementCallbackNotifier> callback =
      new ExpireFaviconsStatementCallbackNotifier();
  return conn->ExecuteAsync(stmts, callback, getter_AddRefs(ps));
}

////////////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

NS_IMETHODIMP
nsFaviconService::Notify(nsITimer* timer) {
  if (timer != mExpireUnassociatedIconsTimer.get()) {
    return NS_ERROR_INVALID_ARG;
  }

  PRTime now = PR_Now();
  for (auto iter = mUnassociatedIcons.Iter(); !iter.Done(); iter.Next()) {
    UnassociatedIconHashKey* iconKey = iter.Get();
    if (now - iconKey->created >= UNASSOCIATED_ICON_EXPIRY_INTERVAL) {
      iter.Remove();
    }
  }

  // Re-init the expiry timer if the cache isn't empty.
  if (mUnassociatedIcons.Count() > 0) {
    mExpireUnassociatedIconsTimer->InitWithCallback(
        this, UNASSOCIATED_ICON_EXPIRY_INTERVAL, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsINamed

NS_IMETHODIMP
nsFaviconService::GetName(nsACString& aName) {
  aName.AssignLiteral("nsFaviconService");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIFaviconService

NS_IMETHODIMP
nsFaviconService::GetDefaultFavicon(nsIURI** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  // not found, use default
  if (!mDefaultIcon) {
    nsresult rv = NS_NewURI(getter_AddRefs(mDefaultIcon),
                            nsLiteralCString(FAVICON_DEFAULT_URL));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> uri = mDefaultIcon;
  uri.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetDefaultFaviconMimeType(nsACString& _retval) {
  _retval = nsLiteralCString(FAVICON_DEFAULT_MIMETYPE);
  return NS_OK;
}

void nsFaviconService::ClearImageCache(nsIURI* aImageURI) {
  MOZ_ASSERT(aImageURI, "Must pass a non-null URI");
  nsCOMPtr<imgICache> imgCache;
  nsresult rv =
      GetImgTools()->GetImgCacheForDocument(nullptr, getter_AddRefs(imgCache));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_SUCCEEDED(rv)) {
    Unused << imgCache->RemoveEntry(aImageURI, nullptr);
  }
}

NS_IMETHODIMP
nsFaviconService::SetAndFetchFaviconForPage(
    nsIURI* aPageURI, nsIURI* aFaviconURI, bool aForceReload,
    uint32_t aFaviconLoadType, nsIFaviconDataCallback* aCallback,
    nsIPrincipal* aLoadingPrincipal, uint64_t aRequestContextID,
    mozIPlacesPendingOperation** _canceler) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG_POINTER(_canceler);

  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadingPrincipal;
  MOZ_ASSERT(loadingPrincipal,
             "please provide aLoadingPrincipal for this favicon");
  if (!loadingPrincipal) {
    // Let's default to the nullPrincipal if no loadingPrincipal is provided.
    AutoTArray<nsString, 2> params = {
        u"nsFaviconService::setAndFetchFaviconForPage()"_ns,
        u"nsFaviconService::setAndFetchFaviconForPage(..., "
        "[optional aLoadingPrincipal])"_ns};
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, "Security by Default"_ns,
        nullptr,  // aDocument
        nsContentUtils::eNECKO_PROPERTIES, "APIDeprecationWarning", params);
    loadingPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
  }
  NS_ENSURE_TRUE(loadingPrincipal, NS_ERROR_FAILURE);

  bool loadPrivate =
      aFaviconLoadType == nsIFaviconService::FAVICON_LOAD_PRIVATE;

  // Build page data.
  PageData page;
  nsresult rv = aPageURI->GetSpec(page.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  // URIs can arguably lack a host.
  Unused << aPageURI->GetHost(page.host);
  if (StringBeginsWith(page.host, "www."_ns)) {
    page.host.Cut(0, 4);
  }
  bool canAddToHistory;
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);
  rv = navHistory->CanAddURI(aPageURI, &canAddToHistory);
  NS_ENSURE_SUCCESS(rv, rv);
  page.canAddToHistory = !!canAddToHistory && !loadPrivate;

  // Build icon data.
  IconData icon;
  // If we have an in-memory icon payload, it overwrites the actual request.
  UnassociatedIconHashKey* iconKey = mUnassociatedIcons.GetEntry(aFaviconURI);
  if (iconKey) {
    icon = iconKey->iconData;
    mUnassociatedIcons.RemoveEntry(iconKey);
  } else {
    icon.fetchMode = aForceReload ? FETCH_ALWAYS : FETCH_IF_MISSING;
    rv = aFaviconURI->GetSpec(icon.spec);
    NS_ENSURE_SUCCESS(rv, rv);
    // URIs can arguably lack a host.
    Unused << aFaviconURI->GetHost(icon.host);
    if (StringBeginsWith(icon.host, "www."_ns)) {
      icon.host.Cut(0, 4);
    }
  }

  // A root icon is when the icon and page have the same host and the path
  // is just /favicon.ico. These icons are considered valid for the whole
  // origin and expired with the origin through a trigger.
  nsAutoCString path;
  if (NS_SUCCEEDED(aFaviconURI->GetPathQueryRef(path)) &&
      !icon.host.IsEmpty() && icon.host.Equals(page.host) &&
      path.EqualsLiteral("/favicon.ico")) {
    icon.rootIcon = 1;
  }

  // If the page url points to an image, the icon's url will be the same.
  // TODO (Bug 403651): store a resample of the image.  For now avoid that
  // for database size and UX concerns.
  // Don't store favicons for error pages either.
  if (icon.spec.Equals(page.spec) ||
      icon.spec.EqualsLiteral(FAVICON_CERTERRORPAGE_URL) ||
      icon.spec.EqualsLiteral(FAVICON_ERRORPAGE_URL)) {
    return NS_OK;
  }

  RefPtr<AsyncFetchAndSetIconForPage> event = new AsyncFetchAndSetIconForPage(
      icon, page, loadPrivate, aCallback, aLoadingPrincipal, aRequestContextID);

  // Get the target thread and start the work.
  // DB will be updated and observers notified when data has finished loading.
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  // Return this event to the caller to allow aborting an eventual fetch.
  event.forget(_canceler);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ReplaceFaviconData(nsIURI* aFaviconURI,
                                     const nsTArray<uint8_t>& aData,
                                     const nsACString& aMimeType,
                                     PRTime aExpiration) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG(aData.Length() > 0);
  NS_ENSURE_ARG(aMimeType.Length() > 0);
  NS_ENSURE_ARG(imgLoader::SupportImageWithMimeType(
      aMimeType, AcceptedMimeTypes::IMAGES_AND_DOCUMENTS));

  PRTime now = PR_Now();
  if (aExpiration < now + MIN_FAVICON_EXPIRATION) {
    // Invalid input, just use the default.
    aExpiration = now + MAX_FAVICON_EXPIRATION;
  }

  UnassociatedIconHashKey* iconKey = mUnassociatedIcons.PutEntry(aFaviconURI);
  if (!iconKey) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  iconKey->created = now;

  // If the cache contains unassociated icons, an expiry timer should already
  // exist, otherwise there may be a timer left hanging around, so make sure we
  // fire a new one.
  uint32_t unassociatedCount = mUnassociatedIcons.Count();
  if (unassociatedCount == 1) {
    mExpireUnassociatedIconsTimer->Cancel();
    mExpireUnassociatedIconsTimer->InitWithCallback(
        this, UNASSOCIATED_ICON_EXPIRY_INTERVAL, nsITimer::TYPE_ONE_SHOT);
  }

  IconData* iconData = &(iconKey->iconData);
  iconData->expiration = aExpiration;
  iconData->status = ICON_STATUS_CACHED;
  iconData->fetchMode = FETCH_NEVER;
  nsresult rv = aFaviconURI->GetSpec(iconData->spec);
  NS_ENSURE_SUCCESS(rv, rv);
  // URIs can arguably lack a host.
  Unused << aFaviconURI->GetHost(iconData->host);
  if (StringBeginsWith(iconData->host, "www."_ns)) {
    iconData->host.Cut(0, 4);
  }

  // Note we can't set rootIcon here, because don't know the page it will be
  // associated with. We'll do that later in SetAndFetchFaviconForPage if the
  // icon doesn't exist; otherwise, if AsyncReplaceFaviconData updates an
  // existing icon, it will take care of not overwriting an existing
  // root = 1 value.

  IconPayload payload;
  payload.mimeType = aMimeType;
  payload.data.Assign(TO_CHARBUFFER(aData.Elements()), aData.Length());
  if (payload.mimeType.EqualsLiteral(SVG_MIME_TYPE)) {
    payload.width = UINT16_MAX;
  }
  // There may already be a previous payload, so ensure to only have one.
  iconData->payloads.Clear();
  iconData->payloads.AppendElement(payload);

  rv = OptimizeIconSizes(*iconData);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there's not valid payload, don't store the icon into to the database.
  if ((*iconData).payloads.Length() == 0) {
    // We cannot optimize this favicon size and we are over the maximum size
    // allowed, so we will not save data to the db to avoid bloating it.
    mUnassociatedIcons.RemoveEntry(aFaviconURI);
    return NS_ERROR_FAILURE;
  }

  // If the database contains an icon at the given url, we will update the
  // database immediately so that the associated pages are kept in sync.
  // Otherwise, do nothing and let the icon be picked up from the memory hash.
  RefPtr<AsyncReplaceFaviconData> event =
      new AsyncReplaceFaviconData(*iconData);
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ReplaceFaviconDataFromDataURL(
    nsIURI* aFaviconURI, const nsAString& aDataURL, PRTime aExpiration,
    nsIPrincipal* aLoadingPrincipal) {
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_TRUE(aDataURL.Length() > 0, NS_ERROR_INVALID_ARG);
  PRTime now = PR_Now();
  if (aExpiration < now + MIN_FAVICON_EXPIRATION) {
    // Invalid input, just use the default.
    aExpiration = now + MAX_FAVICON_EXPIRATION;
  }

  nsCOMPtr<nsIURI> dataURI;
  nsresult rv = NS_NewURI(getter_AddRefs(dataURI), aDataURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use the data: protocol handler to convert the data.
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIProtocolHandler> protocolHandler;
  rv = ioService->GetProtocolHandler("data", getter_AddRefs(protocolHandler));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadingPrincipal;
  MOZ_ASSERT(loadingPrincipal,
             "please provide aLoadingPrincipal for this favicon");
  if (!loadingPrincipal) {
    // Let's default to the nullPrincipal if no loadingPrincipal is provided.
    AutoTArray<nsString, 2> params = {
        u"nsFaviconService::ReplaceFaviconDataFromDataURL()"_ns,
        u"nsFaviconService::ReplaceFaviconDataFromDataURL(...,"
        " [optional aLoadingPrincipal])"_ns};
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, "Security by Default"_ns,
        nullptr,  // aDocument
        nsContentUtils::eNECKO_PROPERTIES, "APIDeprecationWarning", params);

    loadingPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
  }
  NS_ENSURE_TRUE(loadingPrincipal, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadInfo> loadInfo = new mozilla::net::LoadInfo(
      loadingPrincipal,
      nullptr,  // aTriggeringPrincipal
      nullptr,  // aLoadingNode
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT |
          nsILoadInfo::SEC_ALLOW_CHROME | nsILoadInfo::SEC_DISALLOW_SCRIPT,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON);

  nsCOMPtr<nsIChannel> channel;
  rv = protocolHandler->NewChannel(dataURI, loadInfo, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Blocking stream is OK for data URIs.
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t available64;
  rv = stream->Available(&available64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (available64 == 0 || available64 > UINT32_MAX / sizeof(uint8_t)) {
    return NS_ERROR_FILE_TOO_BIG;
  }
  uint32_t available = (uint32_t)available64;

  // Read all the decoded data.
  nsTArray<uint8_t> buffer;
  buffer.SetLength(available);
  uint32_t numRead;
  rv = stream->Read(TO_CHARBUFFER(buffer.Elements()), available, &numRead);
  if (NS_FAILED(rv) || numRead != available) {
    return rv;
  }

  nsAutoCString mimeType;
  rv = channel->GetContentType(mimeType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // ReplaceFaviconData can now do the dirty work.
  rv = ReplaceFaviconData(aFaviconURI, buffer, mimeType, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconURLForPage(nsIURI* aPageURI,
                                       nsIFaviconDataCallback* aCallback,
                                       uint16_t aPreferredWidth) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aCallback);
  // Use the default value, may be UINT16_MAX if a default is not set.
  if (aPreferredWidth == 0) {
    aPreferredWidth = mDefaultIconURIPreferredSize;
  }

  nsAutoCString pageSpec;
  nsresult rv = aPageURI->GetSpec(pageSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString pageHost;
  // It's expected that some domains may not have a host.
  Unused << aPageURI->GetHost(pageHost);

  RefPtr<AsyncGetFaviconURLForPage> event = new AsyncGetFaviconURLForPage(
      pageSpec, pageHost, aPreferredWidth, aCallback);

  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconDataForPage(nsIURI* aPageURI,
                                        nsIFaviconDataCallback* aCallback,
                                        uint16_t aPreferredWidth) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aCallback);
  // Use the default value, may be UINT16_MAX if a default is not set.
  if (aPreferredWidth == 0) {
    aPreferredWidth = mDefaultIconURIPreferredSize;
  }

  nsAutoCString pageSpec;
  nsresult rv = aPageURI->GetSpec(pageSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString pageHost;
  // It's expected that some domains may not have a host.
  Unused << aPageURI->GetHost(pageHost);

  RefPtr<AsyncGetFaviconDataForPage> event = new AsyncGetFaviconDataForPage(
      pageSpec, pageHost, aPreferredWidth, aCallback);
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::CopyFavicons(nsIURI* aFromPageURI, nsIURI* aToPageURI,
                               uint32_t aFaviconLoadType,
                               nsIFaviconDataCallback* aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aFromPageURI);
  NS_ENSURE_ARG(aToPageURI);
  NS_ENSURE_TRUE(
      aFaviconLoadType >= nsIFaviconService::FAVICON_LOAD_PRIVATE &&
          aFaviconLoadType <= nsIFaviconService::FAVICON_LOAD_NON_PRIVATE,
      NS_ERROR_INVALID_ARG);

  PageData fromPage;
  nsresult rv = aFromPageURI->GetSpec(fromPage.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  PageData toPage;
  rv = aToPageURI->GetSpec(toPage.spec);
  NS_ENSURE_SUCCESS(rv, rv);

  bool canAddToHistory;
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);
  rv = navHistory->CanAddURI(aToPageURI, &canAddToHistory);
  NS_ENSURE_SUCCESS(rv, rv);
  toPage.canAddToHistory =
      !!canAddToHistory &&
      aFaviconLoadType != nsIFaviconService::FAVICON_LOAD_PRIVATE;

  RefPtr<AsyncCopyFavicons> event =
      new AsyncCopyFavicons(fromPage, toPage, aCallback);

  // Get the target thread and start the work.
  // DB will be updated and observers notified when done.
  RefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  DB->DispatchToAsyncThread(event);

  return NS_OK;
}

nsresult nsFaviconService::GetFaviconLinkForIcon(nsIURI* aFaviconURI,
                                                 nsIURI** _retval) {
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG_POINTER(_retval);

  nsAutoCString spec;
  if (aFaviconURI) {
    // List of protocols for which it doesn't make sense to generate a favicon
    // uri since they can be directly loaded from disk or memory.
    static constexpr nsLiteralCString sDirectRequestProtocols[] = {
        // clang-format off
        "about"_ns,
        "cached-favicon"_ns,
        "chrome"_ns,
        "data"_ns,
        "file"_ns,
        "moz-page-thumb"_ns,
        "resource"_ns,
        // clang-format on
    };
    nsAutoCString iconURIScheme;
    if (NS_SUCCEEDED(aFaviconURI->GetScheme(iconURIScheme)) &&
        std::find(std::begin(sDirectRequestProtocols),
                  std::end(sDirectRequestProtocols),
                  iconURIScheme) != std::end(sDirectRequestProtocols)) {
      // Just return the input URL.
      *_retval = do_AddRef(aFaviconURI).take();
      return NS_OK;
    }
    nsresult rv = aFaviconURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return GetFaviconLinkForIconString(spec, _retval);
}

// nsFaviconService::GetFaviconLinkForIconString
//
//    This computes a favicon URL with string input and using the cached
//    default one to minimize parsing.

nsresult nsFaviconService::GetFaviconLinkForIconString(const nsCString& aSpec,
                                                       nsIURI** aOutput) {
  if (aSpec.IsEmpty()) {
    return GetDefaultFavicon(aOutput);
  }

  if (StringBeginsWith(aSpec, "chrome:"_ns)) {
    // pass through for chrome URLs, since they can be referenced without
    // this service
    return NS_NewURI(aOutput, aSpec);
  }

  nsAutoCString annoUri;
  annoUri.AssignLiteral("cached-favicon:");
  annoUri += aSpec;
  return NS_NewURI(aOutput, annoUri);
}

/**
 * Checks the icon and evaluates if it needs to be optimized.
 *
 * @param aIcon
 *        The icon to be evaluated.
 */
nsresult nsFaviconService::OptimizeIconSizes(IconData& aIcon) {
  // TODO (bug 1346139): move optimization to the async thread.
  MOZ_ASSERT(NS_IsMainThread());
  // There should only be a single payload at this point, it may have to be
  // split though, if it's an ico file.
  MOZ_ASSERT(aIcon.payloads.Length() == 1);

  // Even if the page provides a large image for the favicon (eg, a highres
  // image or a multiresolution .ico file), don't try to store more data than
  // needed.
  IconPayload payload = aIcon.payloads[0];
  if (payload.mimeType.EqualsLiteral(SVG_MIME_TYPE)) {
    // Nothing to optimize, but check the payload size.
    if (payload.data.Length() >= nsIFaviconService::MAX_FAVICON_BUFFER_SIZE) {
      aIcon.payloads.Clear();
    }
    return NS_OK;
  }

  // Make space for the optimized payloads.
  aIcon.payloads.Clear();

  // decode image
  nsCOMPtr<imgIContainer> container;
  nsresult rv = GetImgTools()->DecodeImageFromBuffer(
      payload.data.get(), payload.data.Length(), payload.mimeType,
      getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  // For ICO files, we must evaluate each of the frames we care about.
  nsTArray<FrameData> framesInfo;
  rv = GetFramesInfoForContainer(container, framesInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& frameInfo : framesInfo) {
    IconPayload newPayload;
    newPayload.mimeType = nsLiteralCString(PNG_MIME_TYPE);
    newPayload.width = frameInfo.width;
    for (uint16_t size : gFaviconSizes) {
      // The icon could be smaller than 16, that is our minimum.
      // Icons smaller than 16px are kept as-is.
      if (frameInfo.width >= 16) {
        if (size > frameInfo.width) {
          continue;
        }
        newPayload.width = size;
      }

      // If the original payload is png, the size is the same and not animated,
      // rescale the image only if it's larger than the maximum allowed.
      bool animated;
      if (newPayload.mimeType.Equals(payload.mimeType) &&
          newPayload.width == frameInfo.width &&
          payload.data.Length() < nsIFaviconService::MAX_FAVICON_BUFFER_SIZE &&
          (NS_FAILED(container->GetAnimated(&animated)) || !animated)) {
        newPayload.data = payload.data;
        break;
      }

      // Otherwise, scale and recompress. Rescaling will also take care of
      // extracting a static image from an animated one.
      // Since EncodeScaledImage uses SYNC_DECODE, it will pick the best
      // frame.
      nsCOMPtr<nsIInputStream> iconStream;
      rv = GetImgTools()->EncodeScaledImage(container, newPayload.mimeType,
                                            newPayload.width, newPayload.width,
                                            u""_ns, getter_AddRefs(iconStream));
      NS_ENSURE_SUCCESS(rv, rv);
      // Read the stream into the new buffer.
      rv = NS_ConsumeStream(iconStream, UINT32_MAX, newPayload.data);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the icon size is good, we are done, otherwise try the next size.
      if (newPayload.data.Length() <
          nsIFaviconService::MAX_FAVICON_BUFFER_SIZE) {
        break;
      }
    }

    MOZ_ASSERT(newPayload.data.Length() <
               nsIFaviconService::MAX_FAVICON_BUFFER_SIZE);
    if (newPayload.data.Length() < nsIFaviconService::MAX_FAVICON_BUFFER_SIZE) {
      aIcon.payloads.AppendElement(newPayload);
    }
  }

  return NS_OK;
}

nsresult nsFaviconService::GetFaviconDataAsync(
    const nsCString& aFaviconSpec, mozIStorageStatementCallback* aCallback) {
  MOZ_ASSERT(aCallback, "Doesn't make sense to call this without a callback");

  nsCOMPtr<mozIStorageAsyncStatement> stmt = mDB->GetAsyncStatement(
      "/*Do not warn (bug no: not worth adding an index */ "
      "SELECT data, width FROM moz_icons "
      "WHERE fixed_icon_url_hash = hash(fixup_url(:url)) AND icon_url = :url "
      "ORDER BY width DESC");
  NS_ENSURE_STATE(stmt);

  nsresult rv = URIBinder::Bind(stmt, "url"_ns, aFaviconSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStatement;
  return stmt->ExecuteAsync(aCallback, getter_AddRefs(pendingStatement));
}

NS_IMETHODIMP
nsFaviconService::SetDefaultIconURIPreferredSize(uint16_t aDefaultSize) {
  mDefaultIconURIPreferredSize = aDefaultSize > 0 ? aDefaultSize : UINT16_MAX;
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::PreferredSizeFromURI(nsIURI* aURI, uint16_t* _size) {
  NS_ENSURE_ARG(aURI);
  *_size = mDefaultIconURIPreferredSize;
  nsAutoCString ref;
  // Check for a ref first.
  if (NS_FAILED(aURI->GetRef(ref)) || ref.Length() == 0) return NS_OK;

  // Look for a "size=" fragment.
  int32_t start = ref.RFind("size=");
  if (start >= 0 && ref.Length() > static_cast<uint32_t>(start) + 5) {
    nsDependentCSubstring size;
    // This is safe regardless, since Rebind checks start is not over
    // Length().
    size.Rebind(ref, start + 5);
    // Check if the string contains any non-digit.
    auto begin = size.BeginReading(), end = size.EndReading();
    for (const auto* ch = begin; ch < end; ++ch) {
      if (*ch < '0' || *ch > '9') {
        // Not a digit.
        return NS_OK;
      }
    }
    // Convert the string to an integer value.
    nsresult rv;
    uint16_t val = PromiseFlatCString(size).ToInteger(&rv);
    if (NS_SUCCEEDED(rv)) {
      *_size = val;
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// ExpireFaviconsStatementCallbackNotifier

ExpireFaviconsStatementCallbackNotifier::
    ExpireFaviconsStatementCallbackNotifier() = default;

NS_IMETHODIMP
ExpireFaviconsStatementCallbackNotifier::HandleCompletion(uint16_t aReason) {
  // We should dispatch only if expiration has been successful.
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED) return NS_OK;

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    (void)observerService->NotifyObservers(
        nullptr, NS_PLACES_FAVICONS_EXPIRED_TOPIC_ID, nullptr);
  }

  return NS_OK;
}
