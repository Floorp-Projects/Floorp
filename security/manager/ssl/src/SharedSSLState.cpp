/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSSLState.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsICertOverrideService.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"
#include "nsRecentBadCerts.h"
#include "PSMRunnable.h"
#include "PublicSSL.h"
#include "ssl.h"
#include "nsNetCID.h"
#include "mozilla/unused.h"

using mozilla::psm::SyncRunnableBase;
using mozilla::unused;

namespace {

static PRInt32 sCertOverrideSvcExists = 0;
static PRInt32 sCertDBExists = 0;

class MainThreadClearer : public SyncRunnableBase
{
public:
  MainThreadClearer() : mShouldClearSessionCache(false) {}

  void RunOnTargetThread() {
    // In some cases it's possible to cause PSM/NSS to initialize while XPCOM shutdown
    // is in progress. We want to avoid this, since they do not handle the situation well,
    // hence the flags to avoid instantiating the services if they don't already exist.

    bool certOverrideSvcExists = (bool)PR_ATOMIC_SET(&sCertOverrideSvcExists, 0);
    if (certOverrideSvcExists) {
      unused << PR_ATOMIC_SET(&sCertOverrideSvcExists, 1);
      nsCOMPtr<nsICertOverrideService> icos = do_GetService(NS_CERTOVERRIDE_CONTRACTID);
      if (icos) {
        icos->ClearValidityOverride(
          NS_LITERAL_CSTRING("all:temporary-certificates"),
          0);
      }
    }

    bool certDBExists = (bool)PR_ATOMIC_SET(&sCertDBExists, 0);
    if (certDBExists) {
      unused << PR_ATOMIC_SET(&sCertDBExists, 1);
      nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
      if (certdb) {
        nsCOMPtr<nsIRecentBadCerts> badCerts;
        certdb->GetRecentBadCerts(true, getter_AddRefs(badCerts));
        if (badCerts) {
          badCerts->ResetStoredCerts();
        }
      }
    }

    // This needs to be checked on the main thread to avoid racing with NSS
    // initialization.
    mShouldClearSessionCache = mozilla::psm::PrivateSSLState() &&
                               mozilla::psm::PrivateSSLState()->SocketCreated();
  }
  bool mShouldClearSessionCache;
};

} // anonymous namespace

namespace mozilla {

void ClearPrivateSSLState()
{
  // This only works if it is called on the socket transport
  // service thread immediately after closing all private SSL
  // connections.
#ifdef DEBUG
  nsresult rv;
  nsCOMPtr<nsIEventTarget> sts
    = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  bool onSTSThread;
  rv = sts->IsOnCurrentThread(&onSTSThread);
  MOZ_ASSERT(NS_SUCCEEDED(rv) && onSTSThread);
#endif

  RefPtr<MainThreadClearer> runnable = new MainThreadClearer;
  runnable->DispatchToMainThreadAndWait();

  // If NSS isn't initialized, this throws an assertion. We guard it by checking if
  // the session cache might even have anything worth clearing.
  if (runnable->mShouldClearSessionCache) {
    SSL_ClearSessionCache();
  }
}

namespace psm {

namespace {
class PrivateBrowsingObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  PrivateBrowsingObserver(SharedSSLState* aOwner) : mOwner(aOwner) {}
  virtual ~PrivateBrowsingObserver() {}
private:
  SharedSSLState* mOwner;
};

SharedSSLState* gPublicState;
SharedSSLState* gPrivateState;
} // anonymous namespace

NS_IMPL_ISUPPORTS1(PrivateBrowsingObserver, nsIObserver)

NS_IMETHODIMP
PrivateBrowsingObserver::Observe(nsISupports     *aSubject,
                                 const char      *aTopic,
                                 const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "last-pb-context-exited")) {
    mOwner->ResetStoredData();
  }
  return NS_OK;
}

SharedSSLState::SharedSSLState()
: mClientAuthRemember(new nsClientAuthRememberService)
, mMutex("SharedSSLState::mMutex")
, mSocketCreated(false)
{
  mIOLayerHelpers.Init();
  mClientAuthRemember->Init();
}

SharedSSLState::~SharedSSLState()
{
}

void
SharedSSLState::NotePrivateBrowsingStatus()
{
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  mObserver = new PrivateBrowsingObserver(this);
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  obsSvc->AddObserver(mObserver, "last-pb-context-exited", false);
}

void
SharedSSLState::ResetStoredData()
{
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  mClientAuthRemember->ClearRememberedDecisions();
  mIOLayerHelpers.clearStoredData();
}

void
SharedSSLState::NoteSocketCreated()
{
  MutexAutoLock lock(mMutex);
  mSocketCreated = true;
}

bool
SharedSSLState::SocketCreated()
{
  MutexAutoLock lock(mMutex);
  return mSocketCreated;
}

/*static*/ void
SharedSSLState::GlobalInit()
{
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  gPublicState = new SharedSSLState();
  gPrivateState = new SharedSSLState();
  gPrivateState->NotePrivateBrowsingStatus();
}

/*static*/ void
SharedSSLState::GlobalCleanup()
{
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");

  gPrivateState->Cleanup();
  delete gPrivateState;
  gPrivateState = nullptr;

  gPublicState->Cleanup();
  delete gPublicState;
  gPublicState = nullptr;
}

/*static*/ void
SharedSSLState::NoteCertOverrideServiceInstantiated()
{
  unused << PR_ATOMIC_SET(&sCertOverrideSvcExists, 1);
}

/*static*/ void
SharedSSLState::NoteCertDBServiceInstantiated()
{
  unused << PR_ATOMIC_SET(&sCertDBExists, 1);
}

void
SharedSSLState::Cleanup()
{
  mIOLayerHelpers.Cleanup();
}

SharedSSLState*
PublicSSLState()
{
  return gPublicState;
}

SharedSSLState*
PrivateSSLState()
{
  return gPrivateState;
}

} // namespace psm
} // namespace mozilla
