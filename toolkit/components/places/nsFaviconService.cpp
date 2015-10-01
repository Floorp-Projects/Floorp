/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "AsyncFaviconHelpers.h"

#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "plbase64.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "nsILoadInfo.h"
#include "nsIContentPolicy.h"
#include "nsContentUtils.h"

// For large favicons optimization.
#include "imgITools.h"
#include "imgIContainer.h"

// Default value for mOptimizedIconDimension
#define OPTIMIZED_FAVICON_DIMENSION 16

#define MAX_FAILED_FAVICONS 256
#define FAVICON_CACHE_REDUCE_COUNT 64

#define UNASSOCIATED_FAVICONS_LENGTH 32

// When replaceFaviconData is called, we store the icons in an in-memory cache
// instead of in storage. Icons in the cache are expired according to this
// interval.
#define UNASSOCIATED_ICON_EXPIRY_INTERVAL 60000

// The MIME type of the default favicon and favicons created by
// OptimizeFaviconImage.
#define DEFAULT_MIME_TYPE "image/png"

using namespace mozilla;
using namespace mozilla::places;

/**
 * Used to notify a topic to system observers on async execute completion.
 * Will throw on error.
 */
class ExpireFaviconsStatementCallbackNotifier : public AsyncStatementCallback
{
public:
  ExpireFaviconsStatementCallbackNotifier();
  NS_IMETHOD HandleCompletion(uint16_t aReason);
};


PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsFaviconService, gFaviconService)

NS_IMPL_CLASSINFO(nsFaviconService, nullptr, 0, NS_FAVICONSERVICE_CID)
NS_IMPL_ISUPPORTS_CI(
  nsFaviconService
, nsIFaviconService
, mozIAsyncFavicons
, nsITimerCallback
)

nsFaviconService::nsFaviconService()
  : mOptimizedIconDimension(OPTIMIZED_FAVICON_DIMENSION)
  , mFailedFaviconSerial(0)
  , mFailedFavicons(MAX_FAILED_FAVICONS / 2)
  , mUnassociatedIcons(UNASSOCIATED_FAVICONS_LENGTH)
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
    gFaviconService = nullptr;
}


nsresult
nsFaviconService::Init()
{
  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  mOptimizedIconDimension = Preferences::GetInt(
    "places.favicons.optimizeToDimension", OPTIMIZED_FAVICON_DIMENSION
  );

  mExpireUnassociatedIconsTimer = do_CreateInstance("@mozilla.org/timer;1");
  NS_ENSURE_STATE(mExpireUnassociatedIconsTimer);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ExpireAllFavicons()
{
  nsCOMPtr<mozIStorageAsyncStatement> unlinkIconsStmt = mDB->GetAsyncStatement(
    "UPDATE moz_places "
    "SET favicon_id = NULL "
    "WHERE favicon_id NOT NULL"
  );
  NS_ENSURE_STATE(unlinkIconsStmt);
  nsCOMPtr<mozIStorageAsyncStatement> removeIconsStmt = mDB->GetAsyncStatement(
    "DELETE FROM moz_favicons WHERE id NOT IN ("
      "SELECT favicon_id FROM moz_places WHERE favicon_id NOT NULL "
    ")"
  );
  NS_ENSURE_STATE(removeIconsStmt);

  mozIStorageBaseStatement* stmts[] = {
    unlinkIconsStmt.get()
  , removeIconsStmt.get()
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  nsRefPtr<ExpireFaviconsStatementCallbackNotifier> callback =
    new ExpireFaviconsStatementCallbackNotifier();
  nsresult rv = mDB->MainConn()->ExecuteAsync(
    stmts, ArrayLength(stmts), callback, getter_AddRefs(ps)
  );
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

NS_IMETHODIMP
nsFaviconService::Notify(nsITimer* timer)
{
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

////////////////////////////////////////////////////////////////////////////////
//// nsIFaviconService

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

void
nsFaviconService::SendFaviconNotifications(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI,
                                           const nsACString& aGUID)
{
  nsAutoCString faviconSpec;
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  if (history && NS_SUCCEEDED(aFaviconURI->GetSpec(faviconSpec))) {
    history->SendPageChangedNotification(aPageURI,
                                         nsINavHistoryObserver::ATTRIBUTE_FAVICON,
                                         NS_ConvertUTF8toUTF16(faviconSpec),
                                         aGUID);
  }
}

NS_IMETHODIMP
nsFaviconService::SetAndFetchFaviconForPage(nsIURI* aPageURI,
                                            nsIURI* aFaviconURI,
                                            bool aForceReload,
                                            uint32_t aFaviconLoadType,
                                            nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  // If a favicon is in the failed cache, only load it during a forced reload.
  bool previouslyFailed;
  nsresult rv = IsFailedFavicon(aFaviconURI, &previouslyFailed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (previouslyFailed) {
    if (aForceReload)
      RemoveFailedFavicon(aFaviconURI);
    else
      return NS_OK;
  }

  // Check if the icon already exists and fetch it from the network, if needed.
  // Finally associate the icon to the requested page if not yet associated.
  rv = AsyncFetchAndSetIconForPage::start(
    aFaviconURI, aPageURI, aForceReload ? FETCH_ALWAYS : FETCH_IF_MISSING,
    aFaviconLoadType, aCallback
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // DB will be updated and observers notified when data has finished loading.
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ReplaceFaviconData(nsIURI* aFaviconURI,
                                    const uint8_t* aData,
                                    uint32_t aDataLen,
                                    const nsACString& aMimeType,
                                    PRTime aExpiration)
{
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG(aData);
  NS_ENSURE_TRUE(aDataLen > 0, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aMimeType.Length() > 0, NS_ERROR_INVALID_ARG);
  if (aExpiration == 0) {
    aExpiration = PR_Now() + MAX_FAVICON_EXPIRATION;
  }

  UnassociatedIconHashKey* iconKey = mUnassociatedIcons.PutEntry(aFaviconURI);
  if (!iconKey) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  iconKey->created = PR_Now();

  // If the cache contains unassociated icons, an expiry timer should already exist, otherwise
  // there may be a timer left hanging around, so make sure we fire a new one.
  int32_t unassociatedCount = mUnassociatedIcons.Count();
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

  // If the page provided a large image for the favicon (eg, a highres image
  // or a multiresolution .ico file), we don't want to store more data than
  // needed.
  if (aDataLen > MAX_ICON_FILESIZE(mOptimizedIconDimension)) {
    rv = OptimizeFaviconImage(aData, aDataLen, aMimeType, iconData->data, iconData->mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (iconData->data.Length() > MAX_FAVICON_SIZE) {
      // We cannot optimize this favicon size and we are over the maximum size
      // allowed, so we will not save data to the db to avoid bloating it.
      mUnassociatedIcons.RemoveEntry(aFaviconURI);
      return NS_ERROR_FAILURE;
    }
  } else {
    iconData->mimeType.Assign(aMimeType);
    iconData->data.Assign(TO_CHARBUFFER(aData), aDataLen);
  }

  // If the database contains an icon at the given url, we will update the
  // database immediately so that the associated pages are kept in sync.
  // Otherwise, do nothing and let the icon be picked up from the memory hash.
  rv = AsyncReplaceFaviconData::start(iconData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::ReplaceFaviconDataFromDataURL(nsIURI* aFaviconURI,
                                               const nsAString& aDataURL,
                                               PRTime aExpiration)
{
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_TRUE(aDataURL.Length() > 0, NS_ERROR_INVALID_ARG);
  if (aExpiration == 0) {
    aExpiration = PR_Now() + MAX_FAVICON_EXPIRATION;
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

  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(nsContentUtils::GetSystemPrincipal(),
                          nullptr, // aTriggeringPrincipal
                          nullptr, // aLoadingNode
                          nsILoadInfo::SEC_NORMAL,
                          nsIContentPolicy::TYPE_INTERNAL_IMAGE);

  nsCOMPtr<nsIChannel> channel;
  rv = protocolHandler->NewChannel2(dataURI, loadInfo, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Blocking stream is OK for data URIs.
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t available64;
  rv = stream->Available(&available64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (available64 == 0 || available64 > UINT32_MAX / sizeof(uint8_t))
    return NS_ERROR_FILE_TOO_BIG;
  uint32_t available = (uint32_t)available64;

  // Read all the decoded data.
  uint8_t* buffer = static_cast<uint8_t*>
                               (moz_xmalloc(sizeof(uint8_t) * available));
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;
  uint32_t numRead;
  rv = stream->Read(TO_CHARBUFFER(buffer), available, &numRead);
  if (NS_FAILED(rv) || numRead != available) {
    free(buffer);
    return rv;
  }

  nsAutoCString mimeType;
  rv = channel->GetContentType(mimeType);
  if (NS_FAILED(rv)) {
    free(buffer);
    return rv;
  }

  // ReplaceFaviconData can now do the dirty work.
  rv = ReplaceFaviconData(aFaviconURI, buffer, available, mimeType, aExpiration);
  free(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconURLForPage(nsIURI *aPageURI,
                                       nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv = AsyncGetFaviconURLForPage::start(aPageURI, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::GetFaviconDataForPage(nsIURI* aPageURI,
                                        nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv = AsyncGetFaviconDataForPage::start(aPageURI, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsFaviconService::GetFaviconLinkForIcon(nsIURI* aFaviconURI,
                                        nsIURI** aOutputURI)
{
  NS_ENSURE_ARG(aFaviconURI);
  NS_ENSURE_ARG_POINTER(aOutputURI);

  nsAutoCString spec;
  if (aFaviconURI) {
    nsresult rv = aFaviconURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return GetFaviconLinkForIconString(spec, aOutputURI);
}


static PLDHashOperator
ExpireFailedFaviconsCallback(nsCStringHashKey::KeyType aKey,
                             uint32_t& aData,
                             void* userArg)
{
  uint32_t* threshold = reinterpret_cast<uint32_t*>(userArg);
  if (aData < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsFaviconService::AddFailedFavicon(nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aFaviconURI);

  nsAutoCString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  mFailedFavicons.Put(spec, mFailedFaviconSerial);
  mFailedFaviconSerial ++;

  if (mFailedFavicons.Count() > MAX_FAILED_FAVICONS) {
    // need to expire some entries, delete the FAVICON_CACHE_REDUCE_COUNT number
    // of items that are the oldest
    uint32_t threshold = mFailedFaviconSerial -
                         MAX_FAILED_FAVICONS + FAVICON_CACHE_REDUCE_COUNT;
    mFailedFavicons.Enumerate(ExpireFailedFaviconsCallback, &threshold);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsFaviconService::RemoveFailedFavicon(nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aFaviconURI);

  nsAutoCString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // we silently do nothing and succeed if the icon is not in the cache
  mFailedFavicons.Remove(spec);
  return NS_OK;
}


NS_IMETHODIMP
nsFaviconService::IsFailedFavicon(nsIURI* aFaviconURI, bool* _retval)
{
  NS_ENSURE_ARG(aFaviconURI);
  nsAutoCString spec;
  nsresult rv = aFaviconURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t serial;
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

  nsAutoCString annoUri;
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
nsFaviconService::OptimizeFaviconImage(const uint8_t* aData, uint32_t aDataLen,
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

  aNewMimeType.AssignLiteral(DEFAULT_MIME_TYPE);

  // scale and recompress
  nsCOMPtr<nsIInputStream> iconStream;
  rv = imgtool->EncodeScaledImage(container, aNewMimeType,
                                  mOptimizedIconDimension,
                                  mOptimizedIconDimension,
                                  EmptyString(),
                                  getter_AddRefs(iconStream));
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the stream into a new buffer.
  rv = NS_ConsumeStream(iconStream, UINT32_MAX, aNewData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFaviconService::GetFaviconDataAsync(nsIURI* aFaviconURI,
                                      mozIStorageStatementCallback *aCallback)
{
  NS_ASSERTION(aCallback, "Doesn't make sense to call this without a callback");
  nsCOMPtr<mozIStorageAsyncStatement> stmt = mDB->GetAsyncStatement(
    "SELECT f.data, f.mime_type FROM moz_favicons f WHERE url = :icon_url"
  );
  NS_ENSURE_STATE(stmt);

  // Ignore the ref part of the URI before querying the database because
  // we may have added a media fragment for rendering purposes.

  nsAutoCString faviconURI;
  aFaviconURI->GetSpecIgnoringRef(faviconURI);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), faviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStatement;
  return stmt->ExecuteAsync(aCallback, getter_AddRefs(pendingStatement));
}

////////////////////////////////////////////////////////////////////////////////
//// ExpireFaviconsStatementCallbackNotifier

ExpireFaviconsStatementCallbackNotifier::ExpireFaviconsStatementCallbackNotifier()
{
}


NS_IMETHODIMP
ExpireFaviconsStatementCallbackNotifier::HandleCompletion(uint16_t aReason)
{
  // We should dispatch only if expiration has been successful.
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
    return NS_OK;

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    (void)observerService->NotifyObservers(nullptr,
                                           NS_PLACES_FAVICONS_EXPIRED_TOPIC_ID,
                                           nullptr);
  }

  return NS_OK;
}
