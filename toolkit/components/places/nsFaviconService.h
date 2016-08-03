/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFaviconService_h_
#define nsFaviconService_h_

#include "nsIFaviconService.h"
#include "mozIAsyncFavicons.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDataHashtable.h"
#include "nsServiceManagerUtils.h"
#include "nsTHashtable.h"
#include "nsToolkitCompsCID.h"
#include "nsURIHashKey.h"
#include "nsITimer.h"
#include "Database.h"
#include "mozilla/storage.h"
#include "mozilla/Attributes.h"

#include "FaviconHelpers.h"

// Favicons bigger than this (in bytes) will not be stored in the database.  We
// expect that most 32x32 PNG favicons will be no larger due to compression.
#define MAX_FAVICON_FILESIZE 3072 /* 3 KiB */

// forward class definitions
class mozIStorageStatementCallback;

class UnassociatedIconHashKey : public nsURIHashKey
{
public:
  explicit UnassociatedIconHashKey(const nsIURI* aURI)
  : nsURIHashKey(aURI)
  {
  }
  UnassociatedIconHashKey(const UnassociatedIconHashKey& aOther)
  : nsURIHashKey(aOther)
  {
    NS_NOTREACHED("Do not call me!");
  }
  mozilla::places::IconData iconData;
  PRTime created;
};

class nsFaviconService final : public nsIFaviconService
                             , public mozIAsyncFavicons
                             , public nsITimerCallback
{
public:
  nsFaviconService();

  /**
   * Obtains the service's object.
   */
  static already_AddRefed<nsFaviconService> GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  /**
   * Returns a cached pointer to the favicon service for consumers in the
   * places directory.
   */
  static nsFaviconService* GetFaviconService()
  {
    if (!gFaviconService) {
      nsCOMPtr<nsIFaviconService> serv =
        do_GetService(NS_FAVICONSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nullptr);
      NS_ASSERTION(gFaviconService, "Should have static instance pointer now");
    }
    return gFaviconService;
  }

  // addition to API for strings to prevent excessive parsing of URIs
  nsresult GetFaviconLinkForIconString(const nsCString& aIcon, nsIURI** aOutput);
  void GetFaviconSpecForIconString(const nsCString& aIcon, nsACString& aOutput);

  nsresult OptimizeFaviconImage(const uint8_t* aData, uint32_t aDataLen,
                                const nsACString& aMimeType,
                                nsACString& aNewData, nsACString& aNewMimeType);

  /**
   * Obtains the favicon data asynchronously.
   *
   * @param aFaviconURI
   *        The URI representing the favicon we are looking for.
   * @param aCallback
   *        The callback where results or errors will be dispatch to.  In the
   *        returned result, the favicon binary data will be at index 0, and the
   *        mime type will be at index 1.
   */
  nsresult GetFaviconDataAsync(nsIURI* aFaviconURI,
                               mozIStorageStatementCallback* aCallback);

  /**
   * Call to send out favicon changed notifications. Should only be called
   * when there is data loaded for the favicon.
   * @param aPageURI
   *        The URI of the page to notify about.
   * @param aFaviconURI
   *        The moz-anno:favicon URI of the icon.
   * @param aGUID
   *        The unique ID associated with the page.
   */
  void SendFaviconNotifications(nsIURI* aPageURI, nsIURI* aFaviconURI,
                                const nsACString& aGUID);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONSERVICE
  NS_DECL_MOZIASYNCFAVICONS
  NS_DECL_NSITIMERCALLBACK

private:
  ~nsFaviconService();

  RefPtr<mozilla::places::Database> mDB;

  nsCOMPtr<nsITimer> mExpireUnassociatedIconsTimer;

  static nsFaviconService* gFaviconService;

  /**
   * A cached URI for the default icon. We return this a lot, and don't want to
   * re-parse and normalize our unchanging string many times.  Important: do
   * not return this directly; use Clone() since callers may change the object
   * they get back. May be null, in which case it needs initialization.
   */
  nsCOMPtr<nsIURI> mDefaultIcon;

  uint32_t mFailedFaviconSerial;
  nsDataHashtable<nsCStringHashKey, uint32_t> mFailedFavicons;

  // This class needs access to the icons cache.
  friend class mozilla::places::AsyncReplaceFaviconData;
  nsTHashtable<UnassociatedIconHashKey> mUnassociatedIcons;
};

#define FAVICON_ANNOTATION_NAME "favicon"

#endif // nsFaviconService_h_
