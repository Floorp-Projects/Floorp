/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsISupports.h"

#include "AndroidBridge.h"

class nsAndroidSystemProxySettings : public nsISystemProxySettings {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsAndroidSystemProxySettings(){};

 private:
  virtual ~nsAndroidSystemProxySettings() {}
};

NS_IMPL_ISUPPORTS(nsAndroidSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsAndroidSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  *aMainThreadOnly = true;
  return NS_OK;
}

nsresult nsAndroidSystemProxySettings::GetPACURI(nsACString& aResult) {
  return NS_OK;
}

nsresult nsAndroidSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                      const nsACString& aScheme,
                                                      const nsACString& aHost,
                                                      const int32_t aPort,
                                                      nsACString& aResult) {
  return mozilla::AndroidBridge::Bridge()->GetProxyForURI(aSpec, aScheme, aHost,
                                                          aPort, aResult);
}

void test(){};

NS_IMPL_COMPONENT_FACTORY(nsAndroidSystemProxySettings) {
  return mozilla::MakeAndAddRef<nsAndroidSystemProxySettings>()
      .downcast<nsISupports>();
}
