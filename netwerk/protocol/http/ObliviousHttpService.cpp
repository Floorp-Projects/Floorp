/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObliviousHttpService.h"

#include "DNSUtils.h"
#include "ObliviousHttpChannel.h"
#include "mozilla/Base64.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsNetUtil.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(ObliviousHttpService, nsIObliviousHttpService, nsIObserver,
                  nsIStreamLoaderObserver)

ObliviousHttpService::ObliviousHttpService()
    : mTRRConfig(ObliviousHttpConfig(), "ObliviousHttpService::mTRRConfig") {
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    prefBranch->AddObserver("network.trr.ohttp", this, false);
  }
  MOZ_ASSERT(NS_IsMainThread());
  ReadPrefs("*"_ns);
}

void ObliviousHttpService::ReadPrefs(const nsACString& whichPref) {
  nsAutoCString relayURIPref("network.trr.ohttp.relay_uri");
  if (whichPref.Equals(relayURIPref) || whichPref.EqualsLiteral("*")) {
    nsAutoCString relayURIString;
    nsresult rv = Preferences::GetCString(relayURIPref.get(), relayURIString);
    if (NS_FAILED(rv)) {
      return;
    }
    nsCOMPtr<nsIURI> relayURI;
    rv = NS_NewURI(getter_AddRefs(relayURI), relayURIString);
    if (NS_FAILED(rv)) {
      return;
    }
    auto trrConfig = mTRRConfig.Lock();
    trrConfig->mRelayURI = relayURI;
  }

  nsAutoCString configURIPref("network.trr.ohttp.config_uri");
  if (whichPref.Equals(configURIPref) || whichPref.EqualsLiteral("*")) {
    nsAutoCString configURIString;
    nsresult rv = Preferences::GetCString(configURIPref.get(), configURIString);
    if (NS_FAILED(rv)) {
      return;
    }
    nsCOMPtr<nsIURI> configURI;
    rv = NS_NewURI(getter_AddRefs(configURI), configURIString);
    if (NS_FAILED(rv)) {
      return;
    }

    nsCOMPtr<nsIChannel> channel;
    rv = DNSUtils::CreateChannelHelper(configURI, getter_AddRefs(channel));
    if (NS_FAILED(rv)) {
      return;
    }
    rv = channel->SetLoadFlags(
        nsIRequest::LOAD_ANONYMOUS | nsIRequest::INHIBIT_CACHING |
        nsIRequest::LOAD_BYPASS_CACHE | nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);
    if (NS_FAILED(rv)) {
      return;
    }
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (!httpChannel) {
      return;
    }
    // This connection should not use TRR
    rv = httpChannel->SetTRRMode(nsIRequest::TRR_DISABLED_MODE);
    if (NS_FAILED(rv)) {
      return;
    }
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
    if (NS_FAILED(rv)) {
      return;
    }
    rv = httpChannel->AsyncOpen(loader);
    if (NS_FAILED(rv)) {
      return;
    }
    auto trrConfig = mTRRConfig.Lock();
    trrConfig->mEncodedConfig.Clear();
  }
}

// nsIObliviousHttpService

NS_IMETHODIMP
ObliviousHttpService::NewChannel(nsIURI* relayURI, nsIURI* targetURI,
                                 const nsTArray<uint8_t>& encodedConfig,
                                 nsIChannel** result) {
  nsCOMPtr<nsIChannel> innerChannel;
  nsresult rv =
      DNSUtils::CreateChannelHelper(relayURI, getter_AddRefs(innerChannel));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIHttpChannel> innerHttpChannel(do_QueryInterface(innerChannel));
  if (!innerHttpChannel) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIChannel> obliviousHttpChannel(
      new ObliviousHttpChannel(targetURI, encodedConfig, innerHttpChannel));
  obliviousHttpChannel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpService::GetTRRSettings(nsIURI** relayURI,
                                     nsTArray<uint8_t>& encodedConfig) {
  auto trrConfig = mTRRConfig.Lock();
  *relayURI = do_AddRef(trrConfig->mRelayURI).take();
  encodedConfig.Assign(trrConfig->mEncodedConfig.Clone());
  return NS_OK;
}

// nsIObserver

NS_IMETHODIMP
ObliviousHttpService::Observe(nsISupports* subject, const char* topic,
                              const char16_t* data) {
  if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(data));
  }
  return NS_OK;
}

// nsIStreamLoaderObserver

NS_IMETHODIMP
ObliviousHttpService::OnStreamComplete(nsIStreamLoader* aLoader,
                                       nsISupports* aContext, nsresult aStatus,
                                       uint32_t aLength,
                                       const uint8_t* aContent) {
  if (NS_SUCCEEDED(aStatus)) {
    auto trrConfig = mTRRConfig.Lock();
    trrConfig->mEncodedConfig.Clear();
    trrConfig->mEncodedConfig.AppendElements(aContent, aLength);
  }

  nsCOMPtr<nsIObserverService> observerService(
      mozilla::services::GetObserverService());
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = observerService->NotifyObservers(
      nullptr, "ohttp-service-config-loaded", nullptr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

}  // namespace mozilla::net
