/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nspr.h"
#include "nscore.h"
#include "nsError.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"
#include "nsIDNSService.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsSupportsPrimitives.h"
#include "nsIEventQueueService.h"


static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID,           NS_CACHESERVICE_CID);

nsCOMPtr<nsIEventQueue>   gEventQ;
nsCOMPtr<nsICacheService> gCacheService;

class AsyncCacheRequest
{
public:
    AsyncCacheRequest(const char * key);
    ~AsyncCacheRequest();

    const char * mKey;
};


nsresult
MakeCacheSession(const char * clientID, nsICacheSession **session)
{
    nsresult rv;

    if (!gCacheService) {
        //    nsCOMPtr<nsICacheService> cacheService = 
        //             do_GetService(kCacheServiceCID, &rv);
        gCacheService = do_GetService(kCacheServiceCID, &rv);
        if (NS_FAILED(rv) || !gCacheService) {
            printf("do_GetService(kCacheServiceCID) failed : %x\n", rv);
            goto error_exit;
        }
    }

    rv = gCacheService->CreateSession(clientID,
                                     nsICache::STORE_IN_MEMORY,
                                     nsICache::NOT_STREAM_BASED,
                                     session);
    if (NS_FAILED(rv))
        printf("nsCacheService::CreateSession() failed : %x\n", rv);

 error_exit:
    return rv;
}


void
TestMemoryObjectCache()
{
    printf("\nTesting Memory Object Cache:\n");
    nsCOMPtr<nsICacheSession> session;
    nsresult rv = MakeCacheSession("testClientID", getter_AddRefs(session));
    if (NS_FAILED(rv))  return;

    nsCOMPtr<nsICacheEntryDescriptor> descriptor;

    // Test ACCESS_READ for nonexistent entry
    printf("\nTest ACCESS_READ:\n");
    rv = session->OpenCacheEntry(NS_LITERAL_CSTRING("nonexistent entry"),
                                 nsICache::ACCESS_READ,
                                 nsICache::BLOCKING,
                                 getter_AddRefs(descriptor));
    if (rv != NS_ERROR_CACHE_KEY_NOT_FOUND)
        printf("OpenCacheEntry(ACCESS_READ) returned: %x for nonexistent entry\n", rv);

    NS_NAMED_LITERAL_CSTRING(cacheKey, "http://www.mozilla.org/somekey");

    // Test creating new entry
    printf("\nTest ACCESS_READ_WRITE:\n");
    rv = session->OpenCacheEntry(cacheKey,
                                 nsICache::ACCESS_READ_WRITE,
                                 nsICache::BLOCKING,
                                 getter_AddRefs(descriptor));
    if (NS_FAILED(rv)) {
        printf("OpenCacheEntry(ACCESS_READ_WRITE) failed: %x\n", rv);
        goto error_exit;
    }

    nsCOMPtr<nsISupportsCString> foo =
        do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);

    foo->SetData(NS_LITERAL_CSTRING("hello world"));

    rv = descriptor->SetCacheElement(foo);
    rv = descriptor->SetDataSize(11);
    rv = descriptor->SetMetaDataElement("itemOne", "metaData works");
    descriptor = nullptr;

    // Test refetching entry

    rv = session->OpenCacheEntry(cacheKey,
                                 nsICache::ACCESS_READ_WRITE,
                                 nsICache::BLOCKING,
                                 getter_AddRefs(descriptor));
    if (NS_FAILED(rv)) {
        printf("OpenCacheEntry(ACCESS_READ_WRITE #2) failed: %x", rv);
        goto error_exit;
    }

    nsCOMPtr<nsISupportsCString> bar;
    descriptor->GetCacheElement(getter_AddRefs(bar));
    if (foo.get() != bar.get()) {
        printf("cache elements not the same\n");
    } else {
        printf("data matches...\n");
    }

    char * metaData;
    rv = descriptor->GetMetaDataElement("itemOne", &metaData);
    if (NS_SUCCEEDED(rv))   printf("metaData = %s\n", metaData);
    else printf("GetMetaDataElement failed : rv = %x\n", rv);
    descriptor = nullptr;

    // Test ACCESS_WRITE entry
    printf("\nTest ACCESS_WRITE:\n");
    rv = session->OpenCacheEntry(cacheKey,
                                 nsICache::ACCESS_WRITE,
                                 nsICache::BLOCKING,
                                 getter_AddRefs(descriptor));
    if (NS_FAILED(rv)) {
        printf("OpenCacheEntry(ACCESS_WRITE) failed: %x", rv);
        goto error_exit;
    }
    rv = descriptor->GetCacheElement(getter_AddRefs(bar));
    if (bar)
        printf("FAILED: we didn't get new entry.\n");
    if (NS_FAILED(rv))
        printf("GetCacheElement failed : %x\n", rv);

    rv = descriptor->GetMetaDataElement("itemOne", &metaData);
    if (NS_SUCCEEDED(rv))
        if (metaData)  printf("metaData = %s\n", metaData);
        else           printf("metaData = nullptr\n");
    else printf("GetMetaDataElement failed : rv = %x\n", rv);

    printf("\n");
 error_exit:

    return;
}


int
main(int argc, char* argv[])
{
    nsresult rv = NS_OK;

    // Start up XPCOM
    rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) return rv;

    /**
     * Create event queue for this thread
     */
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) goto error_exit;

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(gEventQ));

    /**
     * Test Cache
     */
    TestMemoryObjectCache();


 error_exit:
    gEventQ = nullptr;
    eventQService = nullptr;

    NS_ShutdownXPCOM(nullptr);

    printf("XPCOM shut down.\n\n");
    return rv;
}



