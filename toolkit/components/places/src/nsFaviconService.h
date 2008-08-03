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

#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIFaviconService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"

// forward definition for friend class
class FaviconLoadListener;

class nsFaviconService : public nsIFaviconService
{
public:
  nsFaviconService();
  nsresult Init();

  // called by nsNavHistory::Init
  static nsresult InitTables(mozIStorageConnection* aDBConn);

  /**
   * Returns a cached pointer to the favicon service for consumers in the
   * places directory.
   */
  static nsFaviconService* GetFaviconService()
  {
    if (! gFaviconService) {
      // note that we actually have to set the service to a variable here
      // because the work in do_GetService actually happens during assignment >:(
      nsresult rv;
      nsCOMPtr<nsIFaviconService> serv(do_GetService("@mozilla.org/browser/favicon-service;1", &rv));
      NS_ENSURE_SUCCESS(rv, nsnull);

      // our constructor should have set the static variable. If it didn't,
      // something is wrong.
      NS_ASSERTION(gFaviconService, "Favicon service creation failed");
    }
    // the service manager will keep the pointer to our service around, so
    // this should always be valid even if nobody currently has a reference.
    return gFaviconService;
  }

  // internal version called by history when done lazily
  nsresult DoSetAndLoadFaviconForPage(nsIURI* aPage, nsIURI* aFavicon,
                                      PRBool aForceReload);

  // addition to API for strings to prevent excessive parsing of URIs
  nsresult GetFaviconLinkForIconString(const nsCString& aIcon, nsIURI** aOutput);
  void GetFaviconSpecForIconString(const nsCString& aIcon, nsACString& aOutput);

  static nsresult OptimizeFaviconImage(const PRUint8* aData, PRUint32 aDataLen,
                                       const nsACString& aMimeType,
                                       nsACString& aNewData, nsACString& aNewMimeType);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONSERVICE


private:
  ~nsFaviconService();

  nsCOMPtr<mozIStorageConnection> mDBConn; // from history service

  nsCOMPtr<mozIStorageStatement> mDBGetURL; // returns URL, data len given page
  nsCOMPtr<mozIStorageStatement> mDBGetData; // returns actual data given URL
  nsCOMPtr<mozIStorageStatement> mDBGetIconInfo;
  nsCOMPtr<mozIStorageStatement> mDBInsertIcon;
  nsCOMPtr<mozIStorageStatement> mDBUpdateIcon;
  nsCOMPtr<mozIStorageStatement> mDBSetPageFavicon;

  static nsFaviconService* gFaviconService;

  /**
   * A cached URI for the default icon. We return this a lot, and don't want to
   * re-parse and normalize our unchanging string many times.  Important: do
   * not return this directly; use Clone() since callers may change the object
   * they get back. May be null, in which case it needs initialization.
   */
  nsCOMPtr<nsIURI> mDefaultIcon;

  PRUint32 mFailedFaviconSerial;
  nsDataHashtable<nsCStringHashKey, PRUint32> mFailedFavicons;

  nsresult SetFaviconUrlForPageInternal(nsIURI* aURI, nsIURI* aFavicon,
                                        PRBool* aHasData, PRTime* aExpiration);

  nsresult UpdateBookmarkRedirectFavicon(nsIURI* aPage, nsIURI* aFavicon);
  void SendFaviconNotifications(nsIURI* aPage, nsIURI* aFaviconURI);

  friend class FaviconLoadListener;
};

#define FAVICON_DEFAULT_URL "chrome://mozapps/skin/places/defaultFavicon.png"
#define FAVICON_ERRORPAGE_URL "chrome://global/skin/icons/warning-16.png"
#define FAVICON_ANNOTATION_NAME "favicon"
