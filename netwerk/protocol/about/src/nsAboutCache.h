/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsAboutCache_h__
#define nsAboutCache_h__

#include "nsIAboutModule.h"

#include "nsString.h"
#include "nsIOutputStream.h"

#include "nsINetDataCacheManager.h"
#include "nsINetDataCache.h"

class nsAboutCache : public nsIAboutModule 
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAboutCache() { NS_INIT_REFCNT(); }
    virtual ~nsAboutCache() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    void DumpCacheInfo(nsIOutputStream *aStream,
                       nsINetDataCache *aCache);

    void DumpCacheEntryInfo(nsIOutputStream *aStream,
                            nsINetDataCacheManager *aCacheMgr,
                            char * aKey,
                            char *aSecondaryKey,
                            PRUint32 aSecondaryKeyLen);

    nsresult DumpCacheEntries(nsIOutputStream *aStream,
                              nsINetDataCacheManager *aCacheMgr,
                              nsINetDataCache *aCache);
    nsCString mBuffer;
};

#define NS_ABOUT_CACHE_MODULE_CID                    \
{ /* 9158c470-86e4-11d4-9be2-00e09872a416 */         \
    0x9158c470,                                      \
    0x86e4,                                          \
    0x11d4,                                          \
    {0x9b, 0xe2, 0x00, 0xe0, 0x98, 0x72, 0xa4, 0x16} \
}

#endif // nsAboutCache_h__
