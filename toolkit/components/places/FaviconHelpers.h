/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsIFaviconService.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "mozIPlacesPendingOperation.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "imgLoader.h"

class nsIPrincipal;

#include "Database.h"
#include "mozilla/storage.h"

#define ICON_STATUS_UNKNOWN 0
#define ICON_STATUS_CHANGED 1 << 0
#define ICON_STATUS_SAVED 1 << 1
#define ICON_STATUS_ASSOCIATED 1 << 2
#define ICON_STATUS_CACHED 1 << 3

#define TO_CHARBUFFER(_buffer) \
  reinterpret_cast<char*>(const_cast<uint8_t*>(_buffer))
#define TO_INTBUFFER(_string) \
  reinterpret_cast<uint8_t*>(const_cast<char*>(_string.get()))

#define PNG_MIME_TYPE "image/png"
#define SVG_MIME_TYPE "image/svg+xml"

// Always ensure a minimum expiration time, so icons are not already expired
// on addition.
#define MIN_FAVICON_EXPIRATION ((PRTime)1 * 24 * 60 * 60 * PR_USEC_PER_SEC)
// The maximum time we will keep a favicon around.  We always ask the cache
// first and default to this value if we can't get a time, or the time we get
// is far in the future.
#define MAX_FAVICON_EXPIRATION ((PRTime)7 * 24 * 60 * 60 * PR_USEC_PER_SEC)

namespace mozilla {
namespace places {

/**
 * Indicates when a icon should be fetched from network.
 */
enum AsyncFaviconFetchMode { FETCH_NEVER = 0, FETCH_IF_MISSING, FETCH_ALWAYS };

/**
 * Represents one of the payloads (frames) of an icon entry.
 */
struct IconPayload {
  IconPayload() : id(0), width(0) {
    data.SetIsVoid(true);
    mimeType.SetIsVoid(true);
  }

  int64_t id;
  uint16_t width;
  nsCString data;
  nsCString mimeType;
};

/**
 * Represents an icon entry.
 */
struct IconData {
  IconData()
      : expiration(0),
        fetchMode(FETCH_NEVER),
        status(ICON_STATUS_UNKNOWN),
        rootIcon(0) {}

  nsCString spec;
  nsCString host;
  PRTime expiration;
  enum AsyncFaviconFetchMode fetchMode;
  uint16_t status;  // This is a bitset, see ICON_STATUS_* defines above.
  uint8_t rootIcon;
  CopyableTArray<IconPayload> payloads;
};

/**
 * Data cache for a page entry.
 */
struct PageData {
  PageData() : id(0), placeId(0), canAddToHistory(true) {
    guid.SetIsVoid(true);
  }

  int64_t id;       // This is the moz_pages_w_icons id.
  int64_t placeId;  // This is the moz_places page id.
  nsCString spec;
  nsCString host;
  nsCString bookmarkedSpec;
  bool canAddToHistory;  // False for disabled history and unsupported schemas.
  nsCString guid;
};

/**
 * Info for a frame.
 */
struct FrameData {
  FrameData(uint16_t aIndex, uint16_t aWidth) : index(aIndex), width(aWidth) {}

  uint16_t index;
  uint16_t width;
};

/**
 * Async fetches icon from database or network, associates it with the required
 * page and finally notifies the change.
 */
class AsyncFetchAndSetIconForPage final : public Runnable,
                                          public nsIStreamListener,
                                          public nsIInterfaceRequestor,
                                          public nsIChannelEventSink,
                                          public mozIPlacesPendingOperation {
 public:
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_MOZIPLACESPENDINGOPERATION
  NS_DECL_ISUPPORTS_INHERITED

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be fetched and associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aFaviconLoadPrivate
   *        Whether this favicon load is in private browsing.
   * @param aCallback
   *        Function to be called when the fetch-and-associate process finishes.
   * @param aLoadingPrincipal
   *        LoadingPrincipal of the icon to be fetched.
   */
  AsyncFetchAndSetIconForPage(IconData& aIcon, PageData& aPage,
                              bool aFaviconLoadPrivate,
                              nsIFaviconDataCallback* aCallback,
                              nsIPrincipal* aLoadingPrincipal,
                              uint64_t aRequestContextID);

 private:
  nsresult FetchFromNetwork();
  virtual ~AsyncFetchAndSetIconForPage() = default;

  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;
  const bool mFaviconLoadPrivate;
  nsMainThreadPtrHandle<nsIPrincipal> mLoadingPrincipal;
  bool mCanceled;
  nsCOMPtr<nsIRequest> mRequest;
  uint64_t mRequestContextID;
};

/**
 * Associates the icon to the required page, finally dispatches an event to the
 * main thread to notify the change to observers.
 */
class AsyncAssociateIconToPage final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aCallback
   *        Function to be called when the associate process finishes.
   */
  AsyncAssociateIconToPage(
      const IconData& aIcon, const PageData& aPage,
      const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback);

 private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;
};

/**
 * Asynchronously tries to get the URL of a page's favicon, then notifies the
 * given observer.
 */
class AsyncGetFaviconURLForPage final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon's URL we're fetching
   * @param aPageHost
   *        Host of the page whose favicon's URL we're fetching
   * @param aCallback
   *        function to be called once finished
   * @param aPreferredWidth
   *        The preferred size for the icon
   */
  AsyncGetFaviconURLForPage(const nsACString& aPageSpec,
                            const nsACString& aPageHost,
                            uint16_t aPreferredWidth,
                            nsIFaviconDataCallback* aCallback);

 private:
  uint16_t mPreferredWidth;
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  nsCString mPageSpec;
  nsCString mPageHost;
};

/**
 * Asynchronously tries to get the URL and data of a page's favicon, then
 * notifies the given observer.
 */
class AsyncGetFaviconDataForPage final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon URL and data we're fetching
   * @param aPageHost
   *        Host of the page whose favicon's URL we're fetching
   * @param aPreferredWidth
   *        The preferred size of the icon.  We will try to return an icon close
   *        to this size.
   * @param aCallback
   *        function to be called once finished
   */
  AsyncGetFaviconDataForPage(const nsACString& aPageSpec,
                             const nsACString& aPageHost,
                             uint16_t aPreferredWidth,
                             nsIFaviconDataCallback* aCallback);

 private:
  uint16_t mPreferredWidth;
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  nsCString mPageSpec;
  nsCString mPageHost;
};

class AsyncReplaceFaviconData final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  explicit AsyncReplaceFaviconData(const IconData& aIcon);

 private:
  nsresult RemoveIconDataCacheEntry();

  IconData mIcon;
};

/**
 * Notifies the icon change to favicon observers.
 */
class NotifyIconObservers final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon information. Can be empty if no icon is associated to the page.
   * @param aPage
   *        Page to which the icon information applies.
   * @param aCallback
   *        Function to be notified in all cases.
   */
  NotifyIconObservers(
      const IconData& aIcon, const PageData& aPage,
      const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback);

 private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;
};

/**
 * Copies Favicons from one page to another one.
 */
class AsyncCopyFavicons final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aFromPage
   *        The originating page.
   * @param aToPage
   *        The destination page.
   * @param aFaviconLoadPrivate
   *        Whether this favicon load is in private browsing.
   * @param aCallback
   *        An optional callback to invoke when done.
   */
  AsyncCopyFavicons(PageData& aFromPage, PageData& aToPage,
                    nsIFaviconDataCallback* aCallback);

 private:
  PageData mFromPage;
  PageData mToPage;
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
};

}  // namespace places
}  // namespace mozilla
