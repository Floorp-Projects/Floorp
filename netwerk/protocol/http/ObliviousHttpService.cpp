/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObliviousHttpService.h"

#include "DNSUtils.h"
#include "ObliviousHttpChannel.h"
#include "mozilla/Base64.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(ObliviousHttpService, nsIObliviousHttpService, nsIObserver,
                  nsIStreamLoaderObserver)

ObliviousHttpService::ObliviousHttpService()
    : mTRRConfig(ObliviousHttpConfig(), "ObliviousHttpService::mTRRConfig") {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    prefBranch->AddObserver("network.trr.ohttp", this, false);
  }

  if (nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService()) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "network:captive-portal-connectivity-changed",
                     false);
    obs->AddObserver(this, "network:trr-confirmation", false);
  }

  ReadPrefs("*"_ns);
}

static constexpr nsLiteralCString kTRRohttpConfigURIPref =
    "network.trr.ohttp.config_uri"_ns;
static constexpr nsLiteralCString kTRRohttpRelayURIPref =
    "network.trr.ohttp.relay_uri"_ns;

void ObliviousHttpService::FetchConfig(bool aConfigURIChanged) {
  auto scopeExit = MakeScopeExit([&] {
    nsCOMPtr<nsIObserverService> obs(mozilla::services::GetObserverService());
    if (!obs) {
      return;
    }
    obs->NotifyObservers(nullptr, "ohttp-service-config-loaded", u"no-changes");
  });

  {
    auto trrConfig = mTRRConfig.Lock();
    if (aConfigURIChanged) {
      // If the config URI has changed, we need to clear the config.
      trrConfig->mEncodedConfig.Clear();
    } else if (trrConfig->mEncodedConfig.Length()) {
      // The URI hasn't changed and we already have a config. No need to reload
      return;
    }
  }

  nsAutoCString configURIString;
  nsresult rv =
      Preferences::GetCString(kTRRohttpConfigURIPref.get(), configURIString);
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

  if (NS_SUCCEEDED(rv)) {
    scopeExit.release();
    return;
  }

  nsPrintfCString msg(
      "ObliviousHttpService::FetchConfig AsyncOpen failed rv=%X",
      static_cast<uint32_t>(rv));
  NS_WARNING(msg.get());
}

void ObliviousHttpService::ReadPrefs(const nsACString& whichPref) {
  if (whichPref.Equals(kTRRohttpRelayURIPref) || whichPref.EqualsLiteral("*")) {
    nsAutoCString relayURIString;
    nsresult rv =
        Preferences::GetCString(kTRRohttpRelayURIPref.get(), relayURIString);
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

  if (whichPref.Equals(kTRRohttpConfigURIPref) ||
      whichPref.EqualsLiteral("*")) {
    FetchConfig(true);
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

NS_IMETHODIMP
ObliviousHttpService::ClearTRRConfig() {
  auto trrConfig = mTRRConfig.Lock();
  trrConfig->mEncodedConfig.Clear();
  return NS_OK;
}

// nsIObserver

NS_IMETHODIMP
ObliviousHttpService::Observe(nsISupports* subject, const char* topic,
                              const char16_t* data) {
  if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(data));
  } else if (!strcmp(topic, "xpcom-shutdown")) {
    if (nsCOMPtr<nsIPrefBranch> prefBranch =
            do_GetService(NS_PREFSERVICE_CONTRACTID)) {
      prefBranch->RemoveObserver("network.trr.ohttp", this);
    }

    if (nsCOMPtr<nsIObserverService> obs =
            mozilla::services::GetObserverService()) {
      obs->RemoveObserver(this, "xpcom-shutdown");
      obs->RemoveObserver(this, "network:captive-portal-connectivity-changed");
      obs->RemoveObserver(this, "network:trr-confirmation");
    }
  } else if (!strcmp(topic, "network:captive-portal-connectivity-changed") ||
             (!strcmp(topic, "network:trr-confirmation") && data &&
              u"CONFIRM_FAILED"_ns.Equals(data))) {
    FetchConfig(false);
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
      nullptr, "ohttp-service-config-loaded",
      NS_SUCCEEDED(aStatus) ? u"success" : u"failed");
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

}  // namespace mozilla::net
