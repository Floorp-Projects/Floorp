 /* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nscore.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"

#include "nsINetDataCache.h"
#include "nsINetDataCacheManager.h"
#include "nsMemCacheCID.h"
#include "nsMemCache.h"
#include "nsNetDiskCache.h"
#include "nsNetDiskCacheCID.h"
#include "nsCacheManager.h"

// Factory method to create a new nsMemCache instance.  Used
// by nsNetDataCacheModule
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMemCache, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNetDiskCache, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCacheManager, Init)

static nsModuleComponentInfo components[] = {
    { "Memory Cache", NS_MEM_CACHE_FACTORY_CID, NS_NETWORK_MEMORY_CACHE_CONTRACTID, nsMemCacheConstructor },
    { "File Cache",   NS_NETDISKCACHE_CID,      NS_NETWORK_FILE_CACHE_CONTRACTID,   nsNetDiskCacheConstructor },
    { "Cache Manager",NS_CACHE_MANAGER_CID,     NS_NETWORK_CACHE_MANAGER_CONTRACTID,nsCacheManagerConstructor }
};

NS_IMPL_NSGETMODULE("Network Data Cache", components)
