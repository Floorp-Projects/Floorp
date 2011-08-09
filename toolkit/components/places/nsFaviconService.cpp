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
 *   Richard Newman <rnewman@mozilla.com>
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

#include "nsPlacesTables.h"
#include "nsPlacesMacros.h"
#include "Helpers.h"
#include "AsyncFaviconHelpers.h"

#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "nsIPrefService.h"

#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "plbase64.h"
#include "nsIClassInfoImpl.h"

// For large favicons optimization.
#include "imgITools.h"
#include "imgIContainer.h"

// Default value for mOptimizedIconDimension
#define OPTIMIZED_FAVICON_DIMENSION 16

#define MAX_FAVICON_CACHE_SIZE 256
#define FAVICON_CACHE_REDUCE_COUNT 64

/**
 * The maximum time we will keep a favicon around.  We always ask the cache, if
 * we can, but default to this value if we do not get a time back, or the time
 * is more in the future than this.
 * Currently set to one week from now.
 */
#define MAX_FAVICON_EXPIRATION ((PRTime)7 * 24 * 60 * 60 * PR_USEC_PER_SEC)

// The MIME type of the default favicon and favicons created by
// OptimizeFaviconImage.
#define DEFAULT_MIME_TYPE "image/png"

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
NS_IMPL_ISUPPORTS2_CI(
  nsFaviconService
, nsIFaviconService
, mozIAsyncFavicons
)

nsFaviconService::nsFaviconService()
: mSyncStatements(mDBConn)
, mFaviconsExpirationRunning(false)
, mOptimizedIconDimension(OPTIMIZED_FAVICON_DIMENSION)
, mFailedFaviconSerial(0)
, mShuttingDown(false)
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
  // Creation of history service will call InitTables.
  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_OUT_OF_MEMORY);
  mDBConn = historyService->GetStorageConnection();
  NS_ENSURE_TRUE(mDBConn, NS_ERROR_FAILURE);

  // Init failed favicon cache.
  if (!mFailedFavicons.Init(MAX_FAVICON_CACHE_SIZE))
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIPrefBranch> pb = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pb) {
    (void)pb->GetIntPref("places.favicons.optimizeToDimension",
                         &mOptimizedIconDimension);
  }

  return NS_OK;
}


mozIStorageStatement*
nsFaviconService::GetStatement(const nsCOMPtr<mozIStorageStatement>& aStmt)
{
  if (mShuttingDown)
    return nsnull;

  RETURN_IF_STMT(mDBGetIconInfo, NS_LITERAL_CSTRING(
    "SELECT id, length(data), expiration FROM moz_favicons "
    "WHERE url = :icon_url"));

  RETURN_IF_STMT(mDBGetURL, NS_LITERAL_CSTRING(
    "SELECT f.id, f.url, length(f.data), f.expiration "
    "FROM moz_places h "
    "JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url = :page_url "
    "LIMIT 1"));

  RETURN_IF_STMT(mDBGetData, NS_LITERAL_CSTRING(
    "SELECT f.data, f.mime_type FROM moz_favicons f WHERE url = :icon_url"));

  RETURN_IF_STMT(mDBInsertIcon, NS_LITERAL_CSTRING(
    "INSERT INTO moz_favicons (id, url, data, mime_type, expiration) "
      "VALUES (:icon_id, :icon_url, :data, :mime_type, :expiration)"));

  RETURN_IF_STMT(mDBUpdateIcon, NS_LITERAL_CSTRING(
    "UPDATE moz_favicons SET data = :data, mime_type = :mime_type, "
                            "expiration = :expiration "
    "WHERE id = :icon_id"));

  RETURN_IF_STMT(mDBSetPageFavicon, NS_LITERAL_CSTRING(
    "UPDATE moz_places SET favicon_id = :icon_id WHERE id = :page_id"));

  RETURN_IF_STMT(mDBRemoveOnDiskReferences, NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET favicon_id = NULL "
    "WHERE favicon_id NOT NULL"));

  RETURN_IF_STMT(mDBRemoveAllFavicons, NS_LITERAL_CSTRING(
    "DELETE FROM moz_favicons WHERE id NOT IN ("
      "SELECT favicon_id FROM moz_places WHERE favicon_id NOT NULL "
    ")"));

  return nsnull;
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
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_FAVICONS);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsFaviconService::ExpireAllFavicons()
{
  mFaviconsExpirationRunning = true;

  mozIStorageBaseStatement *stmts[] = {
    GetStatement(mDBRemoveOnDiskReferences),
    GetStatement(mDBRemoveAllFavicons),
  };
  NS_ENSURE_STATE(stmts[0] && stmts[1]);
  nsCOMPtr<mozIStoragePendingStatement> ps;
  nsCOMPtr<ExpireFaviconsStatementCallbackNotifier> callback =
    new ExpireFaviconsStatementCallbackNotifier(&mFaviconsExpirationRunning);
  nsresult rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts), callback,
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
  PRBool hasData = PR_FALSE;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetIconInfo);
    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult = PR_FALSE;
    if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
      // We already have an entry for this icon, just get its stats.
      rv = stmt->GetInt64(0, &iconId);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 dataSize;
      rv = stmt->GetInt32(1, &dataSize);
      NS_ENSURE_SUCCESS(rv, rv);
      if (dataSize > 0) {
        hasData = PR_TRUE;
      }
    }
  }

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  if (iconId == -1) {
    // We did not find any entry for this icon, so create a new one.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBInsertIcon);
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
      DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetIconInfo);
      rv = URIBinder::Bind(getInfoStmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
      NS_ENSURE_SUCCESS(rv, rv);
      PRBool hasResult;
      rv = getInfoStmt->ExecuteStep(&hasResult);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");
      iconId = getInfoStmt->AsInt64(0);
    }
  }

  // Now, link our icon entry with the page.
  PRInt64 pageId;
  nsCAutoString guid;
  rv = history->GetOrCreateIdForPage(aPageURI, &pageId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBSetPageFavicon);
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
                                           PRBool aForceReload,
                                           nsIFaviconDataCallback* aCallback)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG(aFaviconURI);

  if (mFaviconsExpirationRunning)
    return NS_OK;

  // If a favicon is in the failed cache, only load it during a forced reload.
  PRBool previouslyFailed;
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
    mDBConn, aCallback
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // DB will be updated and observers notified when data has finished loading.
  return NS_OK;
}

NS_IMETHODIMP
nsFaviconService::SetAndFetchFaviconForPage(nsIURI* aPageURI,
                                            nsIURI* aFaviconURI,
                                            PRBool aForceReload,
                                            nsIFaviconDataCallback* aCallback)
{
  return SetAndLoadFaviconForPage(aPageURI, aFaviconURI,
                                  aForceReload, aCallback);
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
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetIconInfo);
    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasResult) {
      // Get id of the old entry and update it.
      PRInt64 id;
      rv = stmt->GetInt64(0, &id);
      NS_ENSURE_SUCCESS(rv, rv);
      statement = GetStatement(mDBUpdateIcon);
      NS_ENSURE_STATE(statement);
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
      statement = GetStatement(mDBInsertIcon);
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
    }
  }

  mozStorageStatementScoper scoper(statement);

  rv = statement->Execute();
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

  nsCOMPtr<nsIURI> defaultFaviconURI;
  nsresult rv = GetDefaultFavicon(getter_AddRefs(defaultFaviconURI));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isDefaultFavicon = PR_FALSE;
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

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetData);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("icon_url"), aFaviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult = PR_FALSE;
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

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetURL);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
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

  nsresult rv = AsyncGetFaviconURLForPage::start(aPageURI, mDBConn, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsFaviconService::GetFaviconImageForPage(nsIURI* aPageURI, nsIURI** _retval)
{
  NS_ENSURE_ARG(aPageURI);
  NS_ENSURE_ARG_POINTER(_retval);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetURL);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aPageURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
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

  aNewMimeType.AssignLiteral(DEFAULT_MIME_TYPE);

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
  mShuttingDown = true;

  mozIStorageStatement* stmts[] = {
    mDBGetURL,
    mDBGetData,
    mDBGetIconInfo,
    mDBInsertIcon,
    mDBUpdateIcon,
    mDBSetPageFavicon,
    mDBRemoveOnDiskReferences,
    mDBRemoveAllFavicons,
  };

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Finalize the statementCache on the correct thread.
  nsRefPtr<FinalizeStatementCacheProxy<mozIStorageStatement> > event =
    new FinalizeStatementCacheProxy<mozIStorageStatement>(
        mSyncStatements, NS_ISUPPORTS_CAST(nsIFaviconService*, this));
  nsCOMPtr<nsIEventTarget> target = do_GetInterface(mDBConn);
  NS_ENSURE_TRUE(target, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsFaviconService::GetFaviconDataAsync(nsIURI* aFaviconURI,
                                      mozIStorageStatementCallback *aCallback)
{
  NS_ASSERTION(aCallback, "Doesn't make sense to call this without a callback");
  DECLARE_AND_ASSIGN_LAZY_STMT(stmt, mDBGetData);
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
    (void)observerService->NotifyObservers(nsnull,
                                           NS_PLACES_FAVICONS_EXPIRED_TOPIC_ID,
                                           nsnull);
  }

  return NS_OK;
}
