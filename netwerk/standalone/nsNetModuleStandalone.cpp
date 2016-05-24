/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "necko-config.h"

#include "mozilla/ModuleUtils.h"
#include "mozilla/DebugOnly.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsPIDNSService.h"
#include "nsPISocketTransportService.h"
#include "nscore.h"

extern const mozilla::Module kNeckoStandaloneModule;

namespace mozilla {

nsresult
InitNetModuleStandalone()
{
  nsresult rv;

  nsCOMPtr<nsPIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    mozilla::DebugOnly<nsresult> rv = dns->Init();
    NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service init failed");
  } else {
    NS_WARNING("failed to get dns service");
  }

  nsCOMPtr<nsPISocketTransportService> sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    mozilla::DebugOnly<nsresult> rv = sts->Init();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Socket transport service init failed");
  } else {
    NS_WARNING("failed to get socket transport service");
  }

  return NS_OK;
}

nsresult
ShutdownNetModuleStandalone()
{
  nsresult rv;

  nsCOMPtr<nsPIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    mozilla::DebugOnly<nsresult> rv = dns->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "DNS service shutdown failed");
  } else {
    NS_WARNING("failed to get dns service");
  }

  nsCOMPtr<nsPISocketTransportService> sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    mozilla::DebugOnly<nsresult> rv = sts->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Socket transport service shutdown failed");
  } else {
    NS_WARNING("failed to get socket transport service");
  }

  return NS_OK;
}

} // namespace mozilla

#include "nsDNSService2.h"
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIDNSService,
  nsDNSService::GetXPCOMSingleton)

#include "nsSocketTransportService2.h"
typedef mozilla::net::nsSocketTransportService nsSocketTransportService;
#undef LOG
#undef LOG_ENABLED
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSocketTransportService, Init)

// Net module startup hook
static nsresult nsNetStartup()
{
    return NS_OK;
}

// Net module shutdown hook
static void nsNetShutdown()
{
}

NS_DEFINE_NAMED_CID(NS_SOCKETTRANSPORTSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_DNSSERVICE_CID);

static const mozilla::Module::CIDEntry kNeckoCIDs[] = {
    { &kNS_SOCKETTRANSPORTSERVICE_CID, false, nullptr, nsSocketTransportServiceConstructor },
    { &kNS_DNSSERVICE_CID, false, nullptr, nsIDNSServiceConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kNeckoContracts[] = {
    { NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &kNS_SOCKETTRANSPORTSERVICE_CID },
    { NS_DNSSERVICE_CONTRACTID, &kNS_DNSSERVICE_CID },
    { nullptr }
};

const mozilla::Module kNeckoStandaloneModule = {
    mozilla::Module::kVersion,
    kNeckoCIDs,
    kNeckoContracts,
    nullptr,
    nullptr,
    nsNetStartup,
    nsNetShutdown
};
