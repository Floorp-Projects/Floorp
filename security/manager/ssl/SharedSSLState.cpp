/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSSLState.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsICertOverrideService.h"
#include "mozilla/OriginAttributes.h"
#include "nsNSSComponent.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"
#include "PSMRunnable.h"
#include "PublicSSL.h"
#include "ssl.h"
#include "nsNetCID.h"
#include "mozilla/Atomics.h"
#include "mozilla/Unused.h"

using mozilla::Atomic;
using mozilla::Unused;
using mozilla::psm::SyncRunnableBase;

namespace {

static Atomic<bool> sCertOverrideSvcExists(false);

class MainThreadClearer : public SyncRunnableBase {
 public:
  MainThreadClearer() : mShouldClearSessionCache(false) {}

  void RunOnTargetThread() override {
    // In some cases it's possible to cause PSM/NSS to initialize while XPCOM
    // shutdown is in progress. We want to avoid this, since they do not handle
    // the situation well, hence the flags to avoid instantiating the services
    // if they don't already exist.

    bool certOverrideSvcExists = sCertOverrideSvcExists.exchange(false);
    if (certOverrideSvcExists) {
      sCertOverrideSvcExists = true;
      nsCOMPtr<nsICertOverrideService> icos =
          do_GetService(NS_CERTOVERRIDE_CONTRACTID);
      if (icos) {
        icos->ClearValidityOverride("all:temporary-certificates"_ns, 0,
                                    OriginAttributes());
      }
    }

    // This needs to be checked on the main thread to avoid racing with NSS
    // initialization.
    mShouldClearSessionCache = mozilla::psm::PrivateSSLState() &&
                               mozilla::psm::PrivateSSLState()->SocketCreated();
  }
  bool mShouldClearSessionCache;
};

}  // namespace

namespace mozilla {

void ClearPrivateSSLState() {
  // This only works if it is called on the socket transport
  // service thread immediately after closing all private SSL
  // connections.
#ifdef DEBUG
  nsresult rv;
  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  bool onSTSThread;
  rv = sts->IsOnCurrentThread(&onSTSThread);
  MOZ_ASSERT(NS_SUCCEEDED(rv) && onSTSThread);
#endif

  RefPtr<MainThreadClearer> runnable = new MainThreadClearer;
  runnable->DispatchToMainThreadAndWait();

  // If NSS isn't initialized, this throws an assertion. We guard it by checking
  // if the session cache might even have anything worth clearing.
  if (runnable->mShouldClearSessionCache) {
    nsNSSComponent::DoClearSSLExternalAndInternalSessionCache();
  }
}

namespace psm {

namespace {
class PrivateBrowsingObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  explicit PrivateBrowsingObserver(SharedSSLState* aOwner) : mOwner(aOwner) {}

 protected:
  virtual ~PrivateBrowsingObserver() = default;

 private:
  SharedSSLState* mOwner;
};

SharedSSLState* gPublicState;
SharedSSLState* gPrivateState;
}  // namespace

NS_IMPL_ISUPPORTS(PrivateBrowsingObserver, nsIObserver)

NS_IMETHODIMP
PrivateBrowsingObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, "last-pb-context-exited")) {
    mOwner->ResetStoredData();
  }
  return NS_OK;
}

SharedSSLState::SharedSSLState(uint32_t aTlsFlags)
    : mIOLayerHelpers(aTlsFlags),
      mMutex("SharedSSLState::mMutex"),
      mSocketCreated(false),
      mOCSPStaplingEnabled(false),
      mOCSPMustStapleEnabled(false),
      mSignedCertTimestampsEnabled(false) {
  mIOLayerHelpers.Init();
}

SharedSSLState::~SharedSSLState() = default;

void SharedSSLState::NotePrivateBrowsingStatus() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  mObserver = new PrivateBrowsingObserver(this);
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  obsSvc->AddObserver(mObserver, "last-pb-context-exited", false);
}

void SharedSSLState::ResetStoredData() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  mIOLayerHelpers.clearStoredData();
}

void SharedSSLState::NoteSocketCreated() {
  MutexAutoLock lock(mMutex);
  mSocketCreated = true;
}

bool SharedSSLState::SocketCreated() {
  MutexAutoLock lock(mMutex);
  return mSocketCreated;
}

/*static*/
void SharedSSLState::GlobalInit() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  gPublicState = new SharedSSLState();
  gPrivateState = new SharedSSLState();
  gPrivateState->NotePrivateBrowsingStatus();
}

/*static*/
void SharedSSLState::GlobalCleanup() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");

  if (gPrivateState) {
    gPrivateState->Cleanup();
    delete gPrivateState;
    gPrivateState = nullptr;
  }

  if (gPublicState) {
    gPublicState->Cleanup();
    delete gPublicState;
    gPublicState = nullptr;
  }
}

/*static*/
void SharedSSLState::NoteCertOverrideServiceInstantiated() {
  sCertOverrideSvcExists = true;
}

void SharedSSLState::Cleanup() { mIOLayerHelpers.Cleanup(); }

SharedSSLState* PublicSSLState() { return gPublicState; }

SharedSSLState* PrivateSSLState() { return gPrivateState; }

}  // namespace psm
}  // namespace mozilla
