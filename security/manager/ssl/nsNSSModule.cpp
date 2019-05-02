/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSModule.h"

#ifndef MOZ_NEW_CERT_STORAGE
#  include "CertBlocklist.h"
#endif
#include "ContentSignatureVerifier.h"
#include "NSSErrorsService.h"
#include "OSKeyStore.h"
#include "OSReauthenticator.h"
#include "PKCS11ModuleDB.h"
#include "PSMContentListener.h"
#include "SecretDecoderRing.h"
#include "TransportSecurityInfo.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/SyncRunnable.h"
#include "nsCURILoader.h"
#include "nsCertOverrideService.h"
#include "nsCryptoHash.h"
#include "nsICategoryManager.h"
#include "nsKeyModule.h"
#include "nsKeygenHandler.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSComponent.h"
#include "nsNSSVersion.h"
#include "nsNetCID.h"
#include "nsPK11TokenDB.h"
#include "nsPKCS11Slot.h"
#include "nsRandomGenerator.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsSiteSecurityService.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_XUL
#  include "nsCertTree.h"
#endif

namespace mozilla {
namespace psm {

// Many of the implementations in this module call NSS functions and as a result
// require that PSM has successfully initialized NSS before being used.
// Additionally, some of the implementations have various restrictions on which
// process and threads they can be used on (e.g. some can only be used in the
// parent process and some must be initialized only on the main thread).
// The following initialization framework allows these requirements to be
// succinctly expressed and implemented.

template <class InstanceClass, nsresult (InstanceClass::*InitMethod)()>
MOZ_ALWAYS_INLINE static nsresult Instantiate(REFNSIID aIID, void** aResult) {
  InstanceClass* inst = new InstanceClass();
  NS_ADDREF(inst);
  nsresult rv = InitMethod != nullptr ? (inst->*InitMethod)() : NS_OK;
  if (NS_SUCCEEDED(rv)) {
    rv = inst->QueryInterface(aIID, aResult);
  }
  NS_RELEASE(inst);
  return rv;
}

enum class ThreadRestriction {
  // must be initialized on the main thread (but can be used on any thread)
  MainThreadOnly,
  // can be initialized and used on any thread
  AnyThread,
};

enum class ProcessRestriction {
  ParentProcessOnly,
  AnyProcess,
};

template <class InstanceClass,
          nsresult (InstanceClass::*InitMethod)() = nullptr,
          ProcessRestriction processRestriction =
              ProcessRestriction::ParentProcessOnly,
          ThreadRestriction threadRestriction = ThreadRestriction::AnyThread>
static nsresult Constructor(nsISupports* aOuter, REFNSIID aIID,
                            void** aResult) {
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  if (processRestriction == ProcessRestriction::ParentProcessOnly &&
      !XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!EnsureNSSInitializedChromeOrContent()) {
    return NS_ERROR_FAILURE;
  }

  if (threadRestriction == ThreadRestriction::MainThreadOnly &&
      !NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Forward to the main thread synchronously.
    mozilla::SyncRunnable::DispatchToThread(
        mainThread,
        new SyncRunnable(NS_NewRunnableFunction("psm::Constructor", [&]() {
          rv = Instantiate<InstanceClass, InitMethod>(aIID, aResult);
        })));

    return rv;
  }

  return Instantiate<InstanceClass, InitMethod>(aIID, aResult);
}

#define IMPL(type, ...)                                                  \
  template <>                                                            \
  nsresult NSSConstructor<type>(nsISupports * aOuter, const nsIID& aIID, \
                                void** aResult) {                        \
    return Constructor<type, __VA_ARGS__>(aOuter, aIID, aResult);        \
  }

// Components that require main thread initialization could cause a deadlock
// in necko code (bug 1418752). To prevent it we initialize all such components
// on main thread in advance in net_EnsurePSMInit(). Update that function when
// new component with ThreadRestriction::MainThreadOnly is added.
IMPL(SecretDecoderRing, nullptr)
IMPL(nsPK11TokenDB, nullptr)
IMPL(PKCS11ModuleDB, nullptr)
IMPL(nsNSSCertificate, nullptr, ProcessRestriction::AnyProcess)
IMPL(nsNSSCertificateDB, nullptr)
IMPL(nsNSSCertList, nullptr, ProcessRestriction::AnyProcess)
#ifdef MOZ_XUL
IMPL(nsCertTree, nullptr)
#endif
IMPL(nsCryptoHash, nullptr, ProcessRestriction::AnyProcess)
IMPL(nsCryptoHMAC, nullptr, ProcessRestriction::AnyProcess)
IMPL(nsKeyObject, nullptr, ProcessRestriction::AnyProcess)
IMPL(nsKeyObjectFactory, nullptr, ProcessRestriction::AnyProcess)
IMPL(ContentSignatureVerifier, nullptr)
IMPL(nsCertOverrideService, &nsCertOverrideService::Init,
     ProcessRestriction::ParentProcessOnly, ThreadRestriction::MainThreadOnly)
IMPL(nsRandomGenerator, nullptr, ProcessRestriction::AnyProcess)
IMPL(TransportSecurityInfo, nullptr, ProcessRestriction::AnyProcess)
IMPL(nsSiteSecurityService, &nsSiteSecurityService::Init,
     ProcessRestriction::AnyProcess, ThreadRestriction::MainThreadOnly)
#ifndef MOZ_NEW_CERT_STORAGE
IMPL(CertBlocklist, &CertBlocklist::Init, ProcessRestriction::ParentProcessOnly,
     ThreadRestriction::MainThreadOnly)
#endif
IMPL(OSKeyStore, nullptr, ProcessRestriction::ParentProcessOnly,
     ThreadRestriction::MainThreadOnly)
IMPL(OSReauthenticator, nullptr, ProcessRestriction::ParentProcessOnly,
     ThreadRestriction::MainThreadOnly)
#undef IMPL

}  // namespace psm
}  // namespace mozilla
