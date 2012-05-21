/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nspr.h"

extern "C" {
#include <proxy.h>
}

class nsUnixSystemProxySettings : public nsISystemProxySettings {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsUnixSystemProxySettings() { mProxyFactory = nsnull; }
  nsresult Init();

private:
  ~nsUnixSystemProxySettings() {
    if (mProxyFactory) 
      px_proxy_factory_free(mProxyFactory); 
  }

  pxProxyFactory *mProxyFactory;
};

NS_IMPL_ISUPPORTS1(nsUnixSystemProxySettings, nsISystemProxySettings)

nsresult
nsUnixSystemProxySettings::Init()
{
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::GetPACURI(nsACString& aResult)
{
  // Make sure we return an empty result.
  aResult.Truncate();
  return NS_OK;
}

nsresult
nsUnixSystemProxySettings::GetProxyForURI(nsIURI* aURI, nsACString& aResult)
{
  nsresult rv;

  if (!mProxyFactory) {
    mProxyFactory = px_proxy_factory_new();
  }
  NS_ENSURE_TRUE(mProxyFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  char **proxyArray = nsnull;
  proxyArray = px_proxy_factory_get_proxies(mProxyFactory, (char*)(spec.get()));
  NS_ENSURE_TRUE(proxyArray, NS_ERROR_NOT_AVAILABLE);

  // Translate libproxy's output to PAC string as expected
  // libproxy returns an array of proxies in the format:
  // <procotol>://[username:password@]proxy:port
  // or
  // direct://
  //
  // PAC format: "PROXY proxy1.foo.com:8080; PROXY proxy2.foo.com:8080; DIRECT"
  int c = 0;
  while (proxyArray[c] != NULL) {
    if (!aResult.IsEmpty()) {
      aResult.AppendLiteral("; ");
    }

    bool isScheme = false;
    nsXPIDLCString schemeString;
    nsXPIDLCString hostPortString;
    nsCOMPtr<nsIURI> proxyURI;

    rv = ios->NewURI(nsDependentCString(proxyArray[c]),
                                        nsnull,
                                        nsnull,
                                        getter_AddRefs(proxyURI));
    if (NS_FAILED(rv)) {
      c++;
      continue;
    }

    proxyURI->GetScheme(schemeString);
    if (NS_SUCCEEDED(proxyURI->SchemeIs("http", &isScheme)) && isScheme) {
      schemeString.AssignLiteral("proxy");
    }
    aResult.Append(schemeString);
    if (NS_SUCCEEDED(proxyURI->SchemeIs("direct", &isScheme)) && !isScheme) {
      // Add the proxy URI only if it's not DIRECT
      proxyURI->GetHostPort(hostPortString);
      aResult.AppendLiteral(" ");
      aResult.Append(hostPortString);
    }

    c++;
  }

#ifdef DEBUG
  printf("returned PAC proxy string: %s\n", PromiseFlatCString(aResult).get());
#endif

  PR_Free(proxyArray);
  return NS_OK;
}

#define NS_UNIXSYSTEMPROXYSERVICE_CID  /* 0fa3158c-d5a7-43de-9181-a285e74cf1d4 */\
     { 0x0fa3158c, 0xd5a7, 0x43de, \
       {0x91, 0x81, 0xa2, 0x85, 0xe7, 0x4c, 0xf1, 0xd4 } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUnixSystemProxySettings, Init)
NS_DEFINE_NAMED_CID(NS_UNIXSYSTEMPROXYSERVICE_CID);

static const mozilla::Module::CIDEntry kUnixProxyCIDs[] = {
  { &kNS_UNIXSYSTEMPROXYSERVICE_CID, false, NULL, nsUnixSystemProxySettingsConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kUnixProxyContracts[] = {
  { NS_SYSTEMPROXYSETTINGS_CONTRACTID, &kNS_UNIXSYSTEMPROXYSERVICE_CID },
  { NULL }
};

static const mozilla::Module kUnixProxyModule = {
  mozilla::Module::kVersion,
  kUnixProxyCIDs,
  kUnixProxyContracts
};
        
NSMODULE_DEFN(nsUnixProxyModule) = &kUnixProxyModule;

