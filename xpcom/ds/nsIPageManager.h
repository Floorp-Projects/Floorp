/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIPageManager_h__
#define nsIPageManager_h__

#include "nsISupports.h"

#define NS_PAGEMGR_PAGE_BITS            12      // 4k pages
#define NS_PAGEMGR_PAGE_SIZE            (1 << NS_PAGEMGR_PAGE_BITS)
#define NS_PAGEMGR_PAGE_MASK            (NS_PAGEMGR_PAGE_SIZE - 1)
#define NS_PAGEMGR_PAGE_COUNT(bytes)    (((bytes) + NS_PAGEMGR_PAGE_MASK) >> NS_PAGEMGR_PAGE_BITS)

#define NS_IPAGEMANAGER_IID                          \
{ /* bea98210-fb7b-11d2-9324-00104ba0fd40 */         \
    0xbea98210,                                      \
    0xfb7b,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_PAGEMANAGER_CID                           \
{ /* cac907e0-fb7b-11d2-9324-00104ba0fd40 */         \
    0xcac907e0,                                      \
    0xfb7b,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}
#define NS_PAGEMANAGER_PROGID "component://netscape/page-manager"
#define NS_PAGEMANAGER_CLASSNAME "Page Manager"

class nsIPageManager : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPAGEMANAGER_IID);

    NS_IMETHOD AllocPages(PRUint32 pageCount, void* *result) = 0;

    NS_IMETHOD DeallocPages(PRUint32 pageCount, void* pages) = 0;

};

#endif // nsIPageManager_h__
