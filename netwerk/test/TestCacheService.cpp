/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TestCacheService.cpp, released February 7, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 7-February-2001
 */


#include "nspr.h"
#include "nscore.h"
#include "nsError.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIDNSService.h"
#include "nsISupportsPrimitives.h"
#include "nsSupportsPrimitives.h"



static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);


int
main(int argc, char* argv[])
{
    nsresult rv = NS_OK;

    // Start up XPCOM
    rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    printf("Test nsCacheService:\n");

    //    NS_WITH_SERVICE(nsICacheService, cacheService, kCacheServiceCID, &rv);
    nsCOMPtr<nsICacheService> cacheService = do_GetService(kCacheServiceCID, &rv);
    if (NS_FAILED(rv) || !cacheService) goto error_exit;

    nsCOMPtr<nsICacheSession> session;
    rv = cacheService->CreateSession("testClientID",
                                     nsICache::STORE_IN_MEMORY,
                                     nsICache::NOT_STREAM_BASED,
                                     getter_AddRefs(session));

    printf("nsCacheService::CreateSession : rv      = %x\n", rv);
    printf("                                session = %x\n\n",
           (unsigned int)session.get());

    nsCOMPtr<nsICacheEntryDescriptor> descriptor;
    rv = session->OpenCacheEntry("http://www.mozilla.org/pretty.jpg",
                                 nsICache::ACCESS_READ_WRITE,
                                 getter_AddRefs(descriptor));
    if (NS_FAILED(rv)) {
        printf("OpenCacheEntry failed: %x\n", rv);
    } else {
        printf("descriptor = %x\n", (unsigned int)descriptor.get());
    }
    
    nsCOMPtr<nsISupportsString> foo = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    foo->SetData("hello world");

    descriptor->SetCacheElement(foo);
    descriptor = nsnull;

    rv = session->OpenCacheEntry("http://www.mozilla.org/pretty.jpg",
                                 nsICache::ACCESS_READ_WRITE,
                                 getter_AddRefs(descriptor));

    nsCOMPtr<nsISupportsString> bar;
    descriptor->GetCacheElement(getter_AddRefs(bar));

    if (foo.get() != bar.get()) {
        printf("cache elements not the same\n");
    } else {
        printf("It works!");
    }
    
    rv = cacheService->Shutdown();
    printf("nsCacheService::Shutdown() :    rv      = %x\n", rv);
        
 error_exit:

    NS_ShutdownXPCOM(nsnull);

    printf("XPCOM shut down...\n");
    return rv;
}

    

