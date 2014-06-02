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
  nsresult OpenChannel(nsIChannel* channel,
                                   uint32_t aFlags,
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

#endif /* nsURILoader_h__ */
