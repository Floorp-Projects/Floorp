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
 *  Gordon Sheridan  <gordon@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsCacheService.h"
#include "nsNetCID.h"

// nsCacheService
//


#ifdef DEBUG

// Provide factories for some internal classes (debug only)
// 

#include "nsMemoryCacheTransport.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMemoryCacheTransport)

#define NS_MEMORYCACHETRANSPORT_CLASSNAME \
    "Memory Cache Transport"
#define NS_MEMORYCACHETRANSPORT_CONTRACTID \
    "@mozilla.org/network/memory-cache-transport;1"
#define NS_MEMORYCACHETRANSPORT_CID                  \
{ /* 31cb2052-d906-4b53-b3d2-6905c6c6d3b1 */         \
    0x31cb2052,                                      \
    0xd906,                                          \
    0x4b53,                                          \
    {0xb3, 0xd2, 0x69, 0x05, 0xc6, 0xc6, 0xd3, 0xb1} \
}

#endif


static nsModuleComponentInfo gResComponents[] = {
    {
        NS_CACHESERVICE_CLASSNAME,
        NS_CACHESERVICE_CID,
        NS_CACHESERVICE_CONTRACTID,
        nsCacheService::Create
    }
#ifdef DEBUG
   ,{
        NS_MEMORYCACHETRANSPORT_CLASSNAME,
        NS_MEMORYCACHETRANSPORT_CID,
        NS_MEMORYCACHETRANSPORT_CONTRACTID,
        nsMemoryCacheTransportConstructor
    }
#endif
};

NS_IMPL_NSGETMODULE("cacheservice", gResComponents)
