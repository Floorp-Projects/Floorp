/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsAboutCacheEntry_h__
#define nsAboutCacheEntry_h__

#include "nsIAboutModule.h"
#include "nsIChannel.h"
#include "nsICacheListener.h"
#include "nsICacheSession.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsICacheEntryDescriptor;

class nsAboutCacheEntry : public nsIAboutModule
                        , public nsIChannel
                        , public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSICACHELISTENER

    nsAboutCacheEntry() { NS_INIT_ISUPPORTS(); }
    virtual ~nsAboutCacheEntry() {}

private:
    nsresult WriteCacheEntryDescription(nsIOutputStream *, nsICacheEntryDescriptor *);
    nsresult WriteCacheEntryUnavailable(nsIOutputStream *, nsresult);
    nsresult ParseURI(nsCString &, nsCString &);

private:
    nsCOMPtr<nsIChannel>            mStreamChannel;
    nsCOMPtr<nsIStreamListener>     mListener;
    nsCOMPtr<nsISupports>           mListenerContext;
    nsCOMPtr<nsICacheSession>       mCacheSession;
};

#define NS_ABOUT_CACHE_ENTRY_MODULE_CID              \
{ /* 7fa5237d-b0eb-438f-9e50-ca0166e63788 */         \
    0x7fa5237d,                                      \
    0xb0eb,                                          \
    0x438f,                                          \
    {0x9e, 0x50, 0xca, 0x01, 0x66, 0xe6, 0x37, 0x88} \
}

#endif // nsAboutCacheEntry_h__
