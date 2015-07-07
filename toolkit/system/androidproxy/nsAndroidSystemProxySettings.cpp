/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURI.h"

#include "nsISystemProxySettings.h"
#include "nsIServiceManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"

#include "AndroidBridge.h"

class nsAndroidSystemProxySettings : public nsISystemProxySettings
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISYSTEMPROXYSETTINGS

    nsAndroidSystemProxySettings() {};
    nsresult Init();

private:
    virtual ~nsAndroidSystemProxySettings() {}
};

NS_IMPL_ISUPPORTS(nsAndroidSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsAndroidSystemProxySettings::GetMainThreadOnly(bool *aMainThreadOnly)
{
  *aMainThreadOnly = true;
  return NS_OK;
}


nsresult
nsAndroidSystemProxySettings::Init()
{
    return NS_OK;
}

nsresult
nsAndroidSystemProxySettings::GetPACURI(nsACString& aResult)
{
    return NS_OK;
}

nsresult
nsAndroidSystemProxySettings::GetProxyForURI(const nsACString & aSpec,
                                             const nsACString & aScheme,
                                             const nsACString & aHost,
                                             const int32_t      aPort,
                                             nsACString & aResult)
{
    return mozilla::AndroidBridge::Bridge()->GetProxyForURI(aSpec, aScheme, aHost, aPort, aResult);
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAndroidSystemProxySettings, Init)

#define NS_ANDROIDSYSTEMPROXYSERVICE_CID                    \
    {0xf01f0060, 0x3708, 0x478e,                            \
    {0xb9, 0x35, 0x3e, 0xce, 0x8b, 0xe2, 0x94, 0xb8}}

NS_DEFINE_NAMED_CID(NS_ANDROIDSYSTEMPROXYSERVICE_CID);

void test() {};

static const mozilla::Module::CIDEntry kSysProxyCIDs[] = {
    { &kNS_ANDROIDSYSTEMPROXYSERVICE_CID, false, nullptr, nsAndroidSystemProxySettingsConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kSysProxyContracts[] = {
    { NS_SYSTEMPROXYSETTINGS_CONTRACTID, &kNS_ANDROIDSYSTEMPROXYSERVICE_CID },
    { nullptr }
};

static const mozilla::Module kSysProxyModule = {
    mozilla::Module::kVersion,
    kSysProxyCIDs,
    kSysProxyContracts
};

NSMODULE_DEFN(nsAndroidProxyModule) = &kSysProxyModule;
