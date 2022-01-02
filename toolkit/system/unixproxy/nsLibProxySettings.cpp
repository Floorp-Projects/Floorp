/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nspr.h"

extern "C" {
#include <proxy.h>
}

class nsUnixSystemProxySettings : public nsISystemProxySettings {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsUnixSystemProxySettings() { mProxyFactory = nullptr; }

 private:
  ~nsUnixSystemProxySettings() {
    if (mProxyFactory) px_proxy_factory_free(mProxyFactory);
  }

  pxProxyFactory* mProxyFactory;
};

NS_IMPL_ISUPPORTS(nsUnixSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsUnixSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  *aMainThreadOnly = false;
  return NS_OK;
}

nsresult nsUnixSystemProxySettings::GetPACURI(nsACString& aResult) {
  // Make sure we return an empty result.
  aResult.Truncate();
  return NS_OK;
}

nsresult nsUnixSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                   const nsACString& aScheme,
                                                   const nsACString& aHost,
                                                   const int32_t aPort,
                                                   nsACString& aResult) {
  nsresult rv;

  if (!mProxyFactory) {
    mProxyFactory = px_proxy_factory_new();
  }
  NS_ENSURE_TRUE(mProxyFactory, NS_ERROR_NOT_AVAILABLE);

  char** proxyArray = nullptr;
  proxyArray = px_proxy_factory_get_proxies(mProxyFactory,
                                            PromiseFlatCString(aSpec).get());
  NS_ENSURE_TRUE(proxyArray, NS_ERROR_NOT_AVAILABLE);

  // Translate libproxy's output to PAC string as expected
  // libproxy returns an array of proxies in the format:
  // <procotol>://[username:password@]proxy:port
  // or
  // direct://
  //
  // PAC format: "PROXY proxy1.foo.com:8080; PROXY proxy2.foo.com:8080; DIRECT"
  // but nsISystemProxySettings allows "PROXY http://proxy.foo.com:8080" as
  // well.

  int c = 0;
  while (proxyArray[c] != nullptr) {
    if (!aResult.IsEmpty()) {
      aResult.AppendLiteral("; ");
    }

    // figure out the scheme, and we can't use nsIIOService::NewURI because
    // this is not the main thread.
    char* colon = strchr(proxyArray[c], ':');
    uint32_t schemelen = colon ? colon - proxyArray[c] : 0;
    if (schemelen < 1) {
      c++;
      continue;
    }

    if (schemelen == 6 && !strncasecmp(proxyArray[c], "direct", 6)) {
      aResult.AppendLiteral("DIRECT");
    } else {
      aResult.AppendLiteral("PROXY ");
      aResult.Append(proxyArray[c]);
    }

    c++;
  }

  free(proxyArray);
  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsUnixSystemProxySettings) {
  return do_AddRef(new nsUnixSystemProxySettings()).downcast<nsISupports>();
}
