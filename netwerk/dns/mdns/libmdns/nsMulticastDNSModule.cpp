/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_COCOA) || (defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 16)
#define ENABLE_DNS_SERVICE_DISCOVERY
#endif

#include "mozilla/ModuleUtils.h"

#ifdef ENABLE_DNS_SERVICE_DISCOVERY
#include "nsDNSServiceDiscovery.h"
#endif

#include "nsDNSServiceInfo.h"

#ifdef ENABLE_DNS_SERVICE_DISCOVERY
using mozilla::net::nsDNSServiceDiscovery;
#define DNSSERVICEDISCOVERY_CID \
  {0x8df43d23, 0xd3f9, 0x4dd5, \
    { 0xb9, 0x65, 0xde, 0x2c, 0xa3, 0xf6, 0xa4, 0x2c }}
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDNSServiceDiscovery, Init)
NS_DEFINE_NAMED_CID(DNSSERVICEDISCOVERY_CID);
#endif // ENABLE_DNS_SERVICE_DISCOVERY

using mozilla::net::nsDNSServiceInfo;
#define DNSSERVICEINFO_CID \
  {0x14a50f2b, 0x7ff6, 0x48a5, \
    { 0x88, 0xe3, 0x61, 0x5f, 0xd1, 0x11, 0xf5, 0xd3 }}
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDNSServiceInfo)
NS_DEFINE_NAMED_CID(DNSSERVICEINFO_CID);

static const mozilla::Module::CIDEntry knsDNSServiceDiscoveryCIDs[] = {
#ifdef ENABLE_DNS_SERVICE_DISCOVERY
  { &kDNSSERVICEDISCOVERY_CID, false, nullptr, nsDNSServiceDiscoveryConstructor },
#endif
  { &kDNSSERVICEINFO_CID, false, nullptr, nsDNSServiceInfoConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry knsDNSServiceDiscoveryContracts[] = {
#ifdef ENABLE_DNS_SERVICE_DISCOVERY
  { DNSSERVICEDISCOVERY_CONTRACT_ID, &kDNSSERVICEDISCOVERY_CID },
#endif
  { DNSSERVICEINFO_CONTRACT_ID, &kDNSSERVICEINFO_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry knsDNSServiceDiscoveryCategories[] = {
  { nullptr }
};

static const mozilla::Module knsDNSServiceDiscoveryModule = {
  mozilla::Module::kVersion,
  knsDNSServiceDiscoveryCIDs,
  knsDNSServiceDiscoveryContracts,
  knsDNSServiceDiscoveryCategories
};

NSMODULE_DEFN(nsDNSServiceDiscoveryModule) = &knsDNSServiceDiscoveryModule;
