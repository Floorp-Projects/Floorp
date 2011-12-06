/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
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

#ifndef AsyncFaviconHelpers_h_
#define AsyncFaviconHelpers_h_

#include "nsIFaviconService.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"

#include "Database.h"
#include "mozilla/storage.h"

#define ICON_STATUS_UNKNOWN 0
#define ICON_STATUS_CHANGED 1 << 0
#define ICON_STATUS_SAVED 1 << 1
#define ICON_STATUS_ASSOCIATED 1 << 2

namespace mozilla {
namespace places {

/**
 * Indicates when a icon should be fetched from network.
 */
enum AsyncFaviconFetchMode {
  FETCH_NEVER = 0
, FETCH_IF_MISSING
, FETCH_ALWAYS
};

/**
 * Data cache for a icon entry.
 */
struct IconData
{
  IconData()
  : id(0)
  , expiration(0)
  , fetchMode(FETCH_NEVER)
  , status(ICON_STATUS_UNKNOWN)
  {
    guid.SetIsVoid(PR_TRUE);
  }

  PRInt64 id;
  nsCString spec;
  nsCString data;
  nsCString mimeType;
  PRTime expiration;
  enum AsyncFaviconFetchMode fetchMode;
  PRUint16 status; // This is a bitset, see ICON_STATUS_* defines above.
  nsCString guid;
};

/**
 * Data cache for a page entry.
 */
struct PageData
{
  PageData()
  : id(0)
  , canAddToHistory(true)
  , iconId(0)
  {
    guid.SetIsVoid(true);
  }

  PRInt64 id;
  nsCString spec;
  nsCString bookmarkedSpec;
  nsString revHost;
  bool canAddToHistory; // False for disabled history and unsupported schemas.
  PRInt64 iconId;
  nsCString guid;
};

/**
 * Base class for events declared in this file.  This class's main purpose is
 * to declare a destructor which releases mCallback on the main thread.
 */
class AsyncFaviconHelperBase : public nsRunnable
{
protected:
  AsyncFaviconHelperBase(nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncFaviconHelperBase();

  nsRefPtr<Database> mDB;
  // Strong reference since we are responsible for its existence.
  nsCOMPtr<nsIFaviconDataCallback> mCallback;
};

/**
 * Async fetches icon from database or network, associates it with the required
 * page and finally notifies the change.
 */
class AsyncFetchAndSetIconForPage : public AsyncFaviconHelperBase
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Creates the event and dispatches it to the async thread.
   *
   * @param aFaviconURI
   *        URI of the icon to be fetched and associated.
   * @param aPageURI
   *        URI of the page to which associate the icon.
   * @param aFetchMode
   *        Specifies whether a icon should be fetched from network if not found
   *        in the database.
   * @param aCallback
   *        Function to be called when the fetch-and-associate process finishes.
   */
  static nsresult start(nsIURI* aFaviconURI,
                        nsIURI* aPageURI,
                        enum AsyncFaviconFetchMode aFetchMode,
                        nsIFaviconDataCallback* aCallback);

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be fetched and associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aCallback
   *        Function to be called when the fetch-and-associate process finishes.
   */
  AsyncFetchAndSetIconForPage(IconData& aIcon,
                              PageData& aPage,
                              nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncFetchAndSetIconForPage();

protected:
  IconData mIcon;
  PageData mPage;
};

/**
 * If needed will asynchronously fetch the icon from the network.  It will
 * finally dispatch an event to the async thread to associate the icon with
 * the required page.
 */
class AsyncFetchAndSetIconFromNetwork : public AsyncFaviconHelperBase
                                      , public nsIStreamListener
                                      , public nsIInterfaceRequestor
                                      , public nsIChannelEventSink
{
public:
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIRUNNABLE
  NS_DECL_ISUPPORTS_INHERITED

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be fetched and associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aCallback
   *        Function to be called when the fetch-and-associate process finishes.
   */
  AsyncFetchAndSetIconFromNetwork(IconData& aIcon,
                                  PageData& aPage,
                                  nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncFetchAndSetIconFromNetwork();

protected:
  IconData mIcon;
  PageData mPage;
  nsCOMPtr<nsIChannel> mChannel;
};

/**
 * Associates the icon to the required page, finally dispatches an event to the
 * main thread to notify the change to observers.
 */
class AsyncAssociateIconToPage : public AsyncFaviconHelperBase
{
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
  AsyncAssociateIconToPage(IconData& aIcon,
                           PageData& aPage,
                           nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncAssociateIconToPage();

protected:
  IconData mIcon;
  PageData mPage;
};

/**
 * Asynchronously tries to get the URL of a page's favicon.  If this succeeds,
 * notifies the given observer.
 */
class AsyncGetFaviconURLForPage : public AsyncFaviconHelperBase
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Creates the event and dispatches it to the I/O thread.
   *
   * @param aPageURI
   *        URL of the page whose favicon's URL we're fetching
   * @param aCallback
   *        function to be called once the URL is retrieved from the database
   */
  static nsresult start(nsIURI* aPageURI,
                        nsIFaviconDataCallback* aCallback);

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon's URL we're fetching
   * @param aCallback
   *        function to be called once the URL is retrieved from the database
   */
  AsyncGetFaviconURLForPage(const nsACString& aPageSpec,
                            nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncGetFaviconURLForPage();

private:
  nsCString mPageSpec;
};


/**
 * Asynchronously tries to get the URL and data of a page's favicon.  
 * If this succeeds, notifies the given observer.
 */
class AsyncGetFaviconDataForPage : public AsyncFaviconHelperBase
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Creates the event and dispatches it to the I/O thread.
   *
   * @param aPageURI
   *        URL of the page whose favicon URL and data we're fetching
   * @param aCallback
   *        function to be called once the URL and data is retrieved from the database
   */
  static nsresult start(nsIURI* aPageURI,
                        nsIFaviconDataCallback* aCallback);

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon URL and data we're fetching
   * @param aCallback
   *        function to be called once the URL is retrieved from the database
   */
  AsyncGetFaviconDataForPage(const nsACString& aPageSpec,
                             nsCOMPtr<nsIFaviconDataCallback>& aCallback);

  virtual ~AsyncGetFaviconDataForPage();

private:
  nsCString mPageSpec;
};

/**
 * Notifies the icon change to favicon observers.
 */
class NotifyIconObservers : public AsyncFaviconHelperBase
{
public:
  NS_DECL_NSIRUNNABLE

  NotifyIconObservers(IconData& aIcon,
                      PageData& aPage,
                      nsCOMPtr<nsIFaviconDataCallback>& aCallback);
  virtual ~NotifyIconObservers();

protected:
  IconData mIcon;
  PageData mPage;
};

} // namespace places
} // namespace mozilla

#endif // AsyncFaviconHelpers_h_
