/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFaviconService_h_
#define nsFaviconService_h_

#include "nsIFaviconService.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDataHashtable.h"
#include "nsServiceManagerUtils.h"
#include "nsTHashtable.h"
#include "nsToolkitCompsCID.h"
#include "nsURIHashKey.h"
#include "nsINamed.h"
#include "nsITimer.h"
#include "Database.h"
#include "imgITools.h"
#include "mozilla/storage.h"
#include "mozilla/Attributes.h"

#include "FaviconHelpers.h"

// The target dimension in pixels for favicons we store, in reverse order.
// When adding/removing sizes from here, make sure to update the vector size.
static uint16_t sFaviconSizes[7] = {
  192, 144, 96, 64, 48, 32, 16
};

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
    MOZ_ASSERT_UNREACHABLE("Do not call me!");
  }
  mozilla::places::IconData iconData;
  PRTime created;
};

class nsFaviconService final : public nsIFaviconService
                             , public nsITimerCallback
                             , public nsINamed
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

  /**
   * Fetch and migrate favicons from an unsupported payload to a supported one.
   */
  static void ConvertUnsupportedPayloads(mozIStorageConnection* aDBConn);

  // addition to API for strings to prevent excessive parsing of URIs
  nsresult GetFaviconLinkForIconString(const nsCString& aIcon, nsIURI** aOutput);

  nsresult OptimizeIconSizes(mozilla::places::IconData& aIcon);

  /**
   * Obtains the favicon data asynchronously.
   *
   * @param aFaviconSpec
   *        The spec of the URI representing the favicon we are looking for.
   * @param aCallback
   *        The callback where results or errors will be dispatch to.  In the
   *        returned result, the favicon binary data will be at index 0, and the
   *        mime type will be at index 1.
   */
  nsresult GetFaviconDataAsync(const nsCString& aFaviconSpec,
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

  static mozilla::Atomic<int64_t> sLastInsertedIconId;
  static void StoreLastInsertedId(const nsACString& aTable,
                                  const int64_t aLastInsertedId);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONSERVICE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

private:
  imgITools* GetImgTools() {
    if (!mImgTools) {
      mImgTools = do_CreateInstance("@mozilla.org/image/tools;1");
    }
    return mImgTools;
  }

  ~nsFaviconService();

  RefPtr<mozilla::places::Database> mDB;

  nsCOMPtr<nsITimer> mExpireUnassociatedIconsTimer;
  nsCOMPtr<imgITools> mImgTools;

  static nsFaviconService* gFaviconService;

  /**
   * A cached URI for the default icon. We return this a lot, and don't want to
   * re-parse and normalize our unchanging string many times.  Important: do
   * not return this directly; use Clone() since callers may change the object
   * they get back. May be null, in which case it needs initialization.
   */
  nsCOMPtr<nsIURI> mDefaultIcon;

  // This class needs access to the icons cache.
  friend class mozilla::places::AsyncReplaceFaviconData;
  nsTHashtable<UnassociatedIconHashKey> mUnassociatedIcons;

  uint16_t mDefaultIconURIPreferredSize;
};

#define FAVICON_ANNOTATION_NAME "favicon"

#endif // nsFaviconService_h_
