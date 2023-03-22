/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/TRRServiceParent.h"

#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/psm/PSMIPCTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsHttpConnectionInfo.h"
#include "nsICaptivePortalService.h"
#include "nsIParentalControlsService.h"
#include "nsINetworkLinkService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"
#include "nsNetCID.h"
#include "TRRService.h"

#include "DNSLogging.h"

namespace mozilla {
namespace net {

static Atomic<TRRServiceParent*> sTRRServiceParentPtr;

static const char* gTRRUriCallbackPrefs[] = {
    "network.trr.uri",  "network.trr.default_provider_uri",
    "network.trr.mode", kRolloutURIPref,
    kRolloutModePref,   nullptr,
};

NS_IMPL_ISUPPORTS_INHERITED(TRRServiceParent, TRRServiceBase, nsIObserver,
                            nsISupportsWeakReference)

TRRServiceParent::~TRRServiceParent() = default;

void TRRServiceParent::Init() {
  MOZ_ASSERT(gIOService);

  if (!gIOService->SocketProcessReady()) {
    RefPtr<TRRServiceParent> self = this;
    gIOService->CallOrWaitForSocketProcess([self]() { self->Init(); });
    return;
  }

  SocketProcessParent* socketParent = SocketProcessParent::GetSingleton();
  if (!socketParent) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs =
      static_cast<nsIObserverService*>(gIOService);
  TRRService::AddObserver(this, obs);

  bool captiveIsPassed = TRRService::CheckCaptivePortalIsPassed();
  bool parentalControlEnabled = TRRService::GetParentalControlEnabledInternal();

  nsCOMPtr<nsINetworkLinkService> nls =
      do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID);
  nsTArray<nsCString> suffixList;
  if (nls) {
    nls->GetDnsSuffixList(suffixList);
  }

  Preferences::RegisterPrefixCallbacks(TRRServiceParent::PrefsChanged,
                                       gTRRUriCallbackPrefs, this);
  prefsChanged(nullptr);

  if (socketParent->SendPTRRServiceConstructor(
          this, captiveIsPassed, parentalControlEnabled, suffixList)) {
    sTRRServiceParentPtr = this;
  }
}

NS_IMETHODIMP
TRRServiceParent::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  if (!strcmp(aTopic, NS_DNS_SUFFIX_LIST_UPDATED_TOPIC) ||
      !strcmp(aTopic, NS_NETWORK_LINK_TOPIC)) {
    nsCOMPtr<nsINetworkLinkService> link = do_QueryInterface(aSubject);
    // The network link service notification normally passes itself as the
    // subject, but some unit tests will sometimes pass a null subject.
    if (link) {
      nsTArray<nsCString> suffixList;
      link->GetDnsSuffixList(suffixList);
      Unused << SendUpdatePlatformDNSInformation(suffixList);
    }

    if (!strcmp(aTopic, NS_NETWORK_LINK_TOPIC) && mURISetByDetection) {
      CheckURIPrefs();
    }
  }

  return NS_OK;
}

mozilla::ipc::IPCResult
TRRServiceParent::RecvNotifyNetworkConnectivityServiceObservers(
    const nsCString& aTopic) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return IPC_OK();
}

bool TRRServiceParent::MaybeSetPrivateURI(const nsACString& aURI) {
  nsAutoCString newURI(aURI);
  ProcessURITemplate(newURI);

  if (mPrivateURI.Equals(newURI)) {
    return false;
  }

  mPrivateURI = newURI;
  AsyncCreateTRRConnectionInfo(mPrivateURI);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, NS_NETWORK_TRR_URI_CHANGED_TOPIC, nullptr);
  }
  return true;
}

void TRRServiceParent::SetDetectedTrrURI(const nsACString& aURI) {
  if (!mURIPref.IsEmpty()) {
    return;
  }

  mURISetByDetection = MaybeSetPrivateURI(aURI);
  gIOService->CallOrWaitForSocketProcess(
      [self = RefPtr{this}, uri = nsAutoCString(aURI)]() {
        Unused << self->SendSetDetectedTrrURI(uri);
      });
}

void TRRServiceParent::GetURI(nsACString& aURI) {
  // We don't need a lock here, since mPrivateURI is only touched on main
  // thread.
  MOZ_ASSERT(NS_IsMainThread());
  aURI = mPrivateURI;
}

void TRRServiceParent::UpdateParentalControlEnabled() {
  bool enabled = TRRService::GetParentalControlEnabledInternal();
  RefPtr<TRRServiceParent> self = this;
  gIOService->CallOrWaitForSocketProcess([self, enabled]() {
    Unused << self->SendUpdateParentalControlEnabled(enabled);
  });
}

// static
void TRRServiceParent::PrefsChanged(const char* aName, void* aSelf) {
  static_cast<TRRServiceParent*>(aSelf)->prefsChanged(aName);
}

void TRRServiceParent::prefsChanged(const char* aName) {
  if (!aName || !strcmp(aName, "network.trr.uri") ||
      !strcmp(aName, "network.trr.default_provider_uri") ||
      !strcmp(aName, kRolloutURIPref) ||
      !strcmp(aName, "network.trr.ohttp.uri")) {
    OnTRRURIChange();
  }

  if (!aName || !strcmp(aName, "network.trr.mode") ||
      !strcmp(aName, kRolloutModePref)) {
    OnTRRModeChange();
  }
}

void TRRServiceParent::ActorDestroy(ActorDestroyReason why) {
  sTRRServiceParentPtr = nullptr;
  Preferences::UnregisterPrefixCallbacks(TRRServiceParent::PrefsChanged,
                                         gTRRUriCallbackPrefs, this);
}

NS_IMETHODIMP TRRServiceParent::OnProxyConfigChanged() {
  LOG(("TRRServiceParent::OnProxyConfigChanged"));

  AsyncCreateTRRConnectionInfo(mPrivateURI);
  return NS_OK;
}

void TRRServiceParent::SetDefaultTRRConnectionInfo(
    nsHttpConnectionInfo* aConnInfo) {
  TRRServiceBase::SetDefaultTRRConnectionInfo(aConnInfo);

  if (!CanSend()) {
    return;
  }

  if (!aConnInfo) {
    Unused << SendSetDefaultTRRConnectionInfo(Nothing());
    return;
  }

  HttpConnectionInfoCloneArgs infoArgs;
  nsHttpConnectionInfo::SerializeHttpConnectionInfo(aConnInfo, infoArgs);
  Unused << SendSetDefaultTRRConnectionInfo(Some(infoArgs));
}

mozilla::ipc::IPCResult TRRServiceParent::RecvInitTRRConnectionInfo() {
  InitTRRConnectionInfo();
  return IPC_OK();
}

mozilla::ipc::IPCResult TRRServiceParent::RecvSetConfirmationState(
    uint32_t aNewState) {
  mConfirmationState = aNewState;
  return IPC_OK();
}

void TRRServiceParent::ReadEtcHostsFile() {
  if (!sTRRServiceParentPtr) {
    return;
  }

  DoReadEtcHostsFile([](const nsTArray<nsCString>* aArray) -> bool {
    RefPtr<TRRServiceParent> service(sTRRServiceParentPtr);
    if (service && aArray) {
      nsTArray<nsCString> hosts(aArray->Clone());
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "TRRServiceParent::ReadEtcHostsFile",
          [service, hosts = std::move(hosts)]() mutable {
            if (service->CanSend()) {
              Unused << service->SendUpdateEtcHosts(hosts);
            }
          }));
    }
    return !!service;
  });
}

}  // namespace net
}  // namespace mozilla
