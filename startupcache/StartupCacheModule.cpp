/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "nscore.h"

#include "nsID.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/scache/StartupCache.h"

using namespace mozilla::scache;

// XXX Need help with guard for ENABLE_TEST
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(StartupCacheWrapper,
                                         StartupCacheWrapper::GetSingleton)
NS_DEFINE_NAMED_CID(NS_STARTUPCACHE_CID);

static const mozilla::Module::CIDEntry kStartupCacheCIDs[] = {
    { &kNS_STARTUPCACHE_CID, false, NULL, StartupCacheWrapperConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kStartupCacheContracts[] = {
    { "@mozilla.org/startupcache/cache;1", &kNS_STARTUPCACHE_CID },
    { NULL }
};

static const mozilla::Module kStartupCacheModule = {
    mozilla::Module::kVersion,
    kStartupCacheCIDs,
    kStartupCacheContracts,
    NULL,
    NULL,
    NULL,
    NULL
};

NSMODULE_DEFN(StartupCacheModule) = &kStartupCacheModule;
