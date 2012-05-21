/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutCacheEntry_h__
#define nsAboutCacheEntry_h__

#include "nsIAboutModule.h"
#include "nsIChannel.h"
#include "nsICacheListener.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIInputStreamChannel.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsICacheEntryDescriptor;

class nsAboutCacheEntry : public nsIAboutModule
                        , public nsICacheMetaDataVisitor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSICACHEMETADATAVISITOR

    nsAboutCacheEntry()
        : mBuffer(nsnull)
    {}

    virtual ~nsAboutCacheEntry() {}

private:
    nsresult GetContentStream(nsIURI *, nsIInputStream **);
    nsresult OpenCacheEntry(nsIURI *, nsICacheEntryDescriptor **);
    nsresult WriteCacheEntryDescription(nsIOutputStream *, nsICacheEntryDescriptor *);
    nsresult WriteCacheEntryUnavailable(nsIOutputStream *);
    nsresult ParseURI(nsIURI *, nsCString &, bool &, nsCString &);

private:
    nsCString *mBuffer;
};

#define NS_ABOUT_CACHE_ENTRY_MODULE_CID              \
{ /* 7fa5237d-b0eb-438f-9e50-ca0166e63788 */         \
    0x7fa5237d,                                      \
    0xb0eb,                                          \
    0x438f,                                          \
    {0x9e, 0x50, 0xca, 0x01, 0x66, 0xe6, 0x37, 0x88} \
}

#endif // nsAboutCacheEntry_h__
