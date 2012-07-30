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
#include "mozilla/Preferences.h"
#include "mozilla/Util.h"

// For large favicons optimization.
#include "imgITools.h"
#include "imgIContainer.h"

// Default value for mOptimizedIconDimension
#define OPTIMIZED_FAVICON_DIMENSION 16

#define MAX_FAVICON_CACHE_SIZE 256
#define FAVICON_CACHE_REDUCE_COUNT 64

#define MAX_UNASSOCIATED_FAVICONS 64

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
  ExpireFaviconsStatementCallbackNotifier(bool* aFaviconsExpirationRunning);
  NS_IMETHOD HandleCompletion(PRUint16 aReason);

private:
  bool* mFaviconsExpirationRunning;
};


PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsFaviconService, gFaviconService)

NS_IMPL_CLASSINFO(nsFaviconService, NULL, 0, NS_FAVICONSERVICE_CID)
NS_IMPL_ISUPPORTS3_CI(
  nsFaviconService
, nsIFaviconService
, mozIAsyncFavicons
, nsITimerCallback
)

nsFaviconService::nsFaviconService()
  : mFaviconsExpirationRunning(false)
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
    gFaviconService = nullptr;
}


nsresult
nsFaviconService::Init()
{
  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  mFailedFavicons.Init(MAX_FAVICON_CACHE_SIZE);
  mUnassociatedIcons.Init(MAX_UNASSOCIATED_FAVICONS);

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
  mFaviconsExpirationRunning = true;
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
    new ExpireFaviconsStatementCallbackNotifier(&mFaviconsExpirationRunning);
  nsresult rv = mDB->MainConn()->ExecuteAsync(
    stmts, ArrayLength(stmts), callback, getter_AddRefs(ps)
  );
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsITimerCallback

static PLDHashOperator
ExpireNonrecentUnassociatedIconsEnumerator(
  UnassociatedIconHashKey* aIconKey,
  void* aNow)
{
  PRTime now = *(reinterpret_cast<PRTime*>(aNow));
  if (now - aIconKey->created >= UNASSOCIATED_ICON_EXPIRY_INTERVAL) {
    return PL_DHASH_REMOVE;
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsFaviconService::Notify(nsITimer* timer)
{
  if (timer != mExpireUnassociatedIconsTimer.get()) {
    return NS_ERROR_INVALID_ARG;
  }

  PRTime now = PR_Now();
  mUnassociatedIcons.EnumerateEntries(
    ExpireNonrecentUnassociatedIconsEnumerator, &now);
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
nsFaviconService::SetFaviconUrlForPage(nsIURI* aPageURI, nsIURI* aFaviconURI)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  // If we are about to expire all favicons, don't bother setting a new one.
  if (mFaviconsExpirationRunning) {
    return NS_OK;
  }

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  if (history->InPrivateBrowsingMode()) {
    return NS_OK;
  }

  nsresult rv;
  PRInt64 iconId = -1;
  bool hasData = false;
  {
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT id, length(data), expiration FROM moz_favicons "
      "WHERE url = :icon_url"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasResult = false;
    if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
      // We already have an entry for this icon, just get its stats.
      rv = stmt->GetInt64(0, &iconId);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 dataSize;
      rv = stmt->GetInt32(1, &dataSize);
      NS_ENSURE_SUCCESS(rv, rv);
      if (dataSize > 0) {
        hasData = true;
      }
    }
  }

  mozStorageTransaction transaction(mDB->MainConn(), false);

  if (iconId == -1) {
    // We did not find any entry for this icon, so create a new one.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "INSERT INTO moz_favicons (id, url, data, mime_type, expiration, guid) "
      "VALUES (:icon_id, :icon_url, :data, :mime_type, :expiration, "
              "COALESCE(:guid, GENERATE_GUID()))"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("guid"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("icon_id"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("data"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("mime_type"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("expiration"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    {
      nsCOMPtr<mozIStorageStatement> getInfoStmt = mDB->GetStatement(
        "SELECT id, length(data), expiration FROM moz_favicons "
        "WHERE url = :icon_url"
      );
      NS_ENSURE_STATE(getInfoStmt);
      mozStorageStatementScoper scoper(getInfoStmt);

      rv = URIBinder::Bind(getInfoStmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
      NS_ENSURE_SUCCESS(rv, rv);
      bool hasResult;
      rv = getInfoStmt->ExecuteStep(&hasResult);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");
      iconId = getInfoStmt->AsInt64(0);
    }
  }

  // Now, link our icon entry with the page.
  PRInt64 pageId;
  nsCAutoString guid;
  rv = history->GetIdForPage(aPageURI, &pageId, guid);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!pageId) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "UPDATE moz_places SET favicon_id = :icon_id WHERE id = :page_id"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("icon_id"), iconId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Send favicon change notifications only if the icon has any data.
  if (hasData) {
    SendFaviconNotifications(aPageURI, aFaviconURI, guid);
  }

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

void
nsFaviconService::SendFaviconNotifications(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI,
                                           const nsACString& aGUID)
{
  nsCAutoString faviconSpec;
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  if (history && NS_SUCCEEDED(aFaviconURI->GetSpec(faviconSpec))) {
    history->SendPageChangedNotification(aPageURI,
                                         nsINavHistoryObserver::ATTRIBUTE_FAVICON,
                                         NS_ConvertUTF8toUTF16(faviconSpec),
                                         aGUID);
  }
}


NS_IMETHODIMP
nsFaviconService::SetAndLoadFaviconForPage(nsIURI* aPageURI,
                                           nsIURI* aFaviconURI,
                                           bool aForceReload,
                                           nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  if (mFaviconsExpirationRunning)
    return NS_OK;

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
    aCallback
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // DB will be updated and observers notified when data has finished loading.
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::SetAndFetchFaviconForPage(nsIURI* aPageURI,
                                            nsIURI* aFaviconURI,
                                            bool aForceReload,
                                            nsIFaviconDataCallback* aCallback)
{
  return SetAndLoadFaviconForPage(aPageURI, aFaviconURI,
                                  aForceReload, aCallback);
}

NS_IMETHODIMP
nsFaviconService::ReplaceFaviconData(nsIURI* aFaviconURI,
                                    const PRUint8* aData,
                                    PRUint32 aDataLen,
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

  if (mFaviconsExpirationRunning)
    return NS_OK;

  UnassociatedIconHashKey* iconKey = mUnassociatedIcons.PutEntry(aFaviconURI);
  if (!iconKey) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  iconKey->created = PR_Now();

  // If the cache contains unassociated icons, an expiry timer should already exist, otherwise
  // there may be a timer left hanging around, so make sure we fire a new one.
  PRInt32 unassociatedCount = mUnassociatedIcons.Count();
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

  nsCOMPtr<mozIStorageStatement> statement;
  {
    // this block forces the scoper to reset our statement: necessary for the
    // next statement
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT id, length(data), expiration FROM moz_favicons "
      "WHERE url = :icon_url"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasResult) {
      // Get id of the old entry and update it.
      PRInt64 id;
      rv = stmt->GetInt64(0, &id);
      NS_ENSURE_SUCCESS(rv, rv);
      statement = mDB->GetStatement(
        "UPDATE moz_favicons SET "
               "guid       = COALESCE(:guid, guid), "
               "data       = :data, "
               "mime_type  = :mime_type, "
               "expiration = :expiration "
        "WHERE id = :icon_id"
      );
      NS_ENSURE_STATE(statement);

      rv = statement->BindNullByName(NS_LITERAL_CSTRING("guid"));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("icon_id"), id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindBlobByName(NS_LITERAL_CSTRING("data"), data, dataLen);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("mime_type"), *mimeType);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("expiration"), aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Insert a new entry.
      statement = mDB->GetStatement(
       "INSERT INTO moz_favicons (id, url, data, mime_type, expiration, guid) "
       "VALUES (:icon_id, :icon_url, :data, :mime_type, :expiration, "
               "COALESCE(:guid, GENERATE_GUID()))");
      NS_ENSURE_STATE(statement);

      rv = statement->BindNullByName(NS_LITERAL_CSTRING("icon_id"));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = URIBinder::Bind(statement, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindBlobByName(NS_LITERAL_CSTRING("data"), data, dataLen);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("mime_type"), *mimeType);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("expiration"), aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindNullByName(NS_LITERAL_CSTRING("guid"));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  mozStorageStatementScoper statementScoper(statement);

  rv = statement->Execute();
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

  if (mFaviconsExpirationRunning)
    return NS_OK;

  nsCOMPtr<nsIURI> dataURI;
  nsresult rv = NS_NewURI(getter_AddRefs(dataURI), aDataURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use the data: protocol handler to convert the data.
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIProtocolHandler> protocolHandler;
  rv = ioService->GetProtocolHandler("data", getter_AddRefs(protocolHandler));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = protocolHandler->NewChannel(dataURI, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Blocking stream is OK for data URIs.
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 available;
  rv = stream->Available(&available);
  NS_ENSURE_SUCCESS(rv, rv);
  if (available == 0)
    return NS_ERROR_FAILURE;

  // Read all the decoded data.
  PRUint8* buffer = static_cast<PRUint8*>
                               (nsMemory::Alloc(sizeof(PRUint8) * available));
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 numRead;
  rv = stream->Read(TO_CHARBUFFER(buffer), available, &numRead);
  if (NS_FAILED(rv) || numRead != available) {
    nsMemory::Free(buffer);
    return rv;
  }

  nsCAutoString mimeType;
  rv = channel->GetContentType(mimeType);
  if (NS_FAILED(rv)) {
    nsMemory::Free(buffer);
    return rv;
  }

  // ReplaceFaviconData can now do the dirty work.
  rv = ReplaceFaviconData(aFaviconURI, buffer, available, mimeType, aExpiration);
  nsMemory::Free(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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

  // SetFaviconData can now do the dirty work.
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

  nsCOMPtr<nsIURI> defaultFaviconURI;
  nsresult rv = GetDefaultFavicon(getter_AddRefs(defaultFaviconURI));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDefaultFavicon = false;
  rv = defaultFaviconURI->Equals(aFaviconURI, &isDefaultFavicon);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're getting the default favicon, we need to handle it separately since
  // it's not in the database.
  if (isDefaultFavicon) {
    nsCAutoString defaultData;
    rv = GetDefaultFaviconData(defaultData);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint8* bytes = reinterpret_cast<PRUint8*>(ToNewCString(defaultData));
    NS_ENSURE_STATE(bytes);

    *aData = bytes;
    *aDataLen = defaultData.Length();
    aMimeType.AssignLiteral(DEFAULT_MIME_TYPE);

    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT f.data, f.mime_type FROM moz_favicons f WHERE url = :icon_url"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult = false;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    rv = stmt->GetUTF8String(1, aMimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    return stmt->GetBlob(0, aDataLen, aData);
  }
  return NS_ERROR_NOT_AVAILABLE;
}


nsresult
nsFaviconService::GetDefaultFaviconData(nsCString& byteStr)
{
  if (mDefaultFaviconData.IsEmpty()) {
    nsCOMPtr<nsIURI> defaultFaviconURI;
    nsresult rv = GetDefaultFavicon(getter_AddRefs(defaultFaviconURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> istream;
    rv = NS_OpenURI(getter_AddRefs(istream), defaultFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_ConsumeStream(istream, PR_UINT32_MAX, mDefaultFaviconData);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = istream->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    if (mDefaultFaviconData.IsEmpty())
      return NS_ERROR_UNEXPECTED;
  }

  byteStr.Assign(mDefaultFaviconData);
  return NS_OK;
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
    aDataURL.SetIsVoid(true);
    return NS_OK;
  }

  char* encoded = PL_Base64Encode(reinterpret_cast<const char*>(data),
                                  dataLen, nullptr);
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

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT f.id, f.url, length(f.data), f.expiration "
    "FROM moz_places h "
    "JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url = :page_url "
    "LIMIT 1"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCAutoString url;
    rv = stmt->GetUTF8String(1, url);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_NewURI(_retval, url);
  }
  return NS_ERROR_NOT_AVAILABLE;
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

NS_IMETHODIMP
nsFaviconService::GetFaviconImageForPage(nsIURI* aPageURI, nsIURI** _retval)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT f.id, f.url, length(f.data), f.expiration "
    "FROM moz_places h "
    "JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url = :page_url "
    "LIMIT 1"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  nsCOMPtr<nsIURI> faviconURI;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt32 dataLen;
    rv = stmt->GetInt32(2, &dataLen);
    NS_ENSURE_SUCCESS(rv, rv);
    if (dataLen > 0) {
      // this page has a favicon entry with data
      nsCAutoString favIconUri;
      rv = stmt->GetUTF8String(1, favIconUri);
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

  mFailedFavicons.Put(spec, mFailedFaviconSerial);
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
nsFaviconService::IsFailedFavicon(nsIURI* aFaviconURI, bool* _retval)
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
  rv = NS_ConsumeStream(iconStream, PR_UINT32_MAX, aNewData);
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

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStatement;
  return stmt->ExecuteAsync(aCallback, getter_AddRefs(pendingStatement));
}

////////////////////////////////////////////////////////////////////////////////
//// ExpireFaviconsStatementCallbackNotifier

ExpireFaviconsStatementCallbackNotifier::ExpireFaviconsStatementCallbackNotifier(
  bool* aFaviconsExpirationRunning)
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
    mozilla::services::GetObserverService();
  if (observerService) {
    (void)observerService->NotifyObservers(nullptr,
                                           NS_PLACES_FAVICONS_EXPIRED_TOPIC_ID,
                                           nullptr);
  }

  return NS_OK;
}
