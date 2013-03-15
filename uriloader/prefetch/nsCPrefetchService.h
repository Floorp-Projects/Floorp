/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCPrefetchService_h__
#define nsCPrefetchService_h__

#include "nsIPrefetchService.h"

/**
 * nsPrefetchService : nsIPrefetchService
 */
#define NS_PREFETCHSERVICE_CONTRACTID \
    "@mozilla.org/prefetch-service;1"
#define NS_PREFETCHSERVICE_CID                       \
{ /* 6b8bdffc-3394-417d-be83-a81b7c0f63bf */         \
    0x6b8bdffc,                                      \
    0x3394,                                          \
    0x417d,                                          \
    {0xbe, 0x83, 0xa8, 0x1b, 0x7c, 0x0f, 0x63, 0xbf} \
}

/**
 * nsOfflineCacheUpdateService : nsIOfflineCacheUpdateService
 */

#define NS_OFFLINECACHEUPDATESERVICE_CONTRACTID \
  "@mozilla.org/offlinecacheupdate-service;1"
#define NS_OFFLINECACHEUPDATESERVICE_CID             \
{ /* ec06f3fc-70db-4ecd-94e0-a6e91ca44d8a */         \
    0xec06f3fc,                                      \
    0x70db,                                          \
    0x4ecd ,                                         \
    {0x94, 0xe0, 0xa6, 0xe9, 0x1c, 0xa4, 0x4d, 0x8a} \
}

/**
 * nsOfflineCacheUpdate : nsIOfflineCacheUpdate
 */

#define NS_OFFLINECACHEUPDATE_CONTRACTID \
  "@mozilla.org/offlinecacheupdate;1"
#define NS_OFFLINECACHEUPDATE_CID                    \
{ /* e56f5e01-c7cc-4675-a9d7-b8f1e4127295 */         \
    0xe56f5e01,                                      \
    0xc7cc,                                          \
    0x4675,                                          \
    {0xa9, 0xd7, 0xb8, 0xf1, 0xe4, 0x12, 0x72, 0x95} \
}


#endif // !nsCPrefetchService_h__
