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
 * The Original Code is nsCacheModule.cpp, released February 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *  Patrick C. Beard <beard@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsCacheService.h"

// nsCacheService
//
#define NS_CACHESERVICE_CLASSNAME "Cache Service"

#define NS_CACHESERVICE_CONTRACTID "@mozilla.org/network/cache-service;1"

#define NS_CACHESERVICE_CID                          \
{ /* 6c84aec9-29a5-4264-8fbc-bee8f922ea67 */         \
    0x6c84aec9,                                      \
    0x29a5,                                          \
    0x4264,                                          \
    {0x8f, 0xbc, 0xbe, 0xe8, 0xf9, 0x22, 0xea, 0x67} \
}

static nsModuleComponentInfo gResComponents[] = {
    {
        NS_CACHESERVICE_CLASSNAME,
        NS_CACHESERVICE_CID,
        NS_CACHESERVICE_CONTRACTID,
        nsCacheService::Create
    }
};

NS_IMPL_NSGETMODULE("cache", gResComponents)
