/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURILoader_h__
#define nsURILoader_h__

#include "nsCURILoader.h"
#include "nsISupportsUtils.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsIWeakReference.h"
#include "mozilla/Attributes.h"

#ifdef MOZ_LOGGING
// Uncomment the next line to force logging on in release builds
// #define FORCE_PR_LOG
#endif
#include "prlog.h"

class nsDocumentOpenInfo;

class nsURILoader MOZ_FINAL : public nsIURILoader
{
public:
  NS_DECL_NSIURILOADER
  NS_DECL_ISUPPORTS

  nsURILoader();
  ~nsURILoader();

protected:
  /**
   * Equivalent to nsIURILoader::openChannel, but allows specifying whether the
   * channel is opened already.
   */
  NS_HIDDEN_(nsresult) OpenChannel(nsIChannel* channel,
                                   PRUint32 aFlags,
                                   nsIInterfaceRequestor* aWindowContext,
                                   bool aChannelOpen,
                                   nsIStreamListener** aListener);

  /**
   * we shouldn't need to have an owning ref count on registered
   * content listeners because they are supposed to unregister themselves
   * when they go away. This array stores weak references
   */
  nsCOMArray<nsIWeakReference> m_listeners;

#ifdef PR_LOGGING
  /**
   * NSPR logging.  The module is called "URILoader"
   */
  static PRLogModuleInfo* mLog;
#endif
  
  friend class nsDocumentOpenInfo;
};

/**
 * The load has been cancelled because it was found on a malware or phishing blacklist.
 * XXX: this belongs in an nsDocShellErrors.h file of some sort.
 */
#define NS_ERROR_MALWARE_URI   NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_URILOADER, 30)
#define NS_ERROR_PHISHING_URI  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_URILOADER, 31)

/**
 * Used when "Save Link As..." doesn't see the headers quickly enough to choose
 * a filename.  See nsContextMenu.js. 
 */
#define NS_ERROR_SAVE_LINK_AS_TIMEOUT  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_URILOADER, 32)

/** Used when the data from a channel has already been parsed and cached
 *  so it doesn't need to be reparsed from the original source.
 */
#define NS_ERROR_PARSED_DATA_CACHED    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_URILOADER, 33)

#endif /* nsURILoader_h__ */
