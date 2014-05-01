/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutCache_h__
#define nsAboutCache_h__

#include "nsIAboutModule.h"
#include "nsICacheStorageVisitor.h"
#include "nsICacheStorage.h"

#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsILoadContextInfo.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsAboutCache : public nsIAboutModule 
                   , public nsICacheStorageVisitor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSICACHESTORAGEVISITOR

    nsAboutCache() {}
    virtual ~nsAboutCache() {}

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    static nsresult
    GetStorage(nsACString const & storageName, nsILoadContextInfo* loadInfo,
               nsICacheStorage **storage);

protected:
    nsresult ParseURI(nsIURI * uri, nsACString & storage);

    // Finds a next storage we wish to visit (we use this method
    // even there is a specified storage name, which is the only
    // one in the list then.)  Posts FireVisitStorage() when found.
    nsresult VisitNextStorage();
    // Helper method that calls VisitStorage() for the current storage.
    // When it fails, OnCacheEntryVisitCompleted is simlated to close
    // the output stream and thus the about:cache channel.
    void FireVisitStorage();
    // Kiks the visit cycle for the given storage, names can be:
    // "disk", "memory", "appcache"
    // Note: any newly added storage type has to be manually handled here.
    nsresult VisitStorage(nsACString const & storageName);

    // Writes content of mBuffer to mStream and truncates
    // the buffer.
    void FlushBuffer();

    // Whether we are showing overview status of all available
    // storages.
    bool mOverview;

    // Flag initially false, that indicates the entries header has
    // been added to the output HTML.
    bool mEntriesHeaderAdded;

    // The context we are working with.
    nsCOMPtr<nsILoadContextInfo> mLoadInfo;
    nsCString mContextString;

    // The list of all storage names we want to visit
    nsTArray<nsCString> mStorageList;
    nsCString mStorageName;
    nsCOMPtr<nsICacheStorage> mStorage;

    // Output data buffering and streaming output
    nsCString mBuffer;
    nsCOMPtr<nsIOutputStream> mStream;
};

#define NS_ABOUT_CACHE_MODULE_CID                    \
{ /* 9158c470-86e4-11d4-9be2-00e09872a416 */         \
    0x9158c470,                                      \
    0x86e4,                                          \
    0x11d4,                                          \
    {0x9b, 0xe2, 0x00, 0xe0, 0x98, 0x72, 0xa4, 0x16} \
}

#endif // nsAboutCache_h__
