/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef MOZ_LOGGING
// Uncomment the next line to force logging on in release builds
// #define FORCE_PR_LOG
#endif
#include "prlog.h"

class nsDocumentOpenInfo;

class nsURILoader : public nsIURILoader
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
