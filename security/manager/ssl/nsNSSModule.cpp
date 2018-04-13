/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertBlocklist.h"
#include "ContentSignatureVerifier.h"
#include "NSSErrorsService.h"
#include "PKCS11ModuleDB.h"
#include "PSMContentListener.h"
#include "SecretDecoderRing.h"
#include "TransportSecurityInfo.h"
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
#include "nsNTLMAuthModule.h"
#include "nsNetCID.h"
#include "nsPK11TokenDB.h"
#include "nsPKCS11Slot.h"
#include "nsRandomGenerator.h"
#include "nsSSLSocketProvider.h"
#include "nsSSLStatus.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsSiteSecurityService.h"
#include "nsTLSSocketProvider.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_XUL
#include "nsCertTree.h"
#endif

namespace mozilla { namespace psm {

// Many of the implementations in this module call NSS functions and as a result
// require that PSM has successfully initialized NSS before being used.
// Additionally, some of the implementations have various restrictions on which
// process and threads they can be used on (e.g. some can only be used in the
// parent process and some must be initialized only on the main thread).
// The following initialization framework allows these requirements to be
// succinctly expressed and implemented.

template<class InstanceClass, nsresult (InstanceClass::*InitMethod)()>
MOZ_ALWAYS_INLINE static nsresult
Instantiate(REFNSIID aIID, void** aResult)
{
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

template<class InstanceClass, nsresult (InstanceClass::*InitMethod)() = nullptr,
         ProcessRestriction processRestriction = ProcessRestriction::ParentProcessOnly,
         ThreadRestriction threadRestriction = ThreadRestriction::AnyThread>
static nsresult
Constructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
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

} } // namespace mozilla::psm

using namespace mozilla::psm;

namespace {

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PSMContentListener, init)

typedef mozilla::psm::NSSErrorsService NSSErrorsService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(NSSErrorsService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSVersion)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSComponent, Init)

NS_DEFINE_NAMED_CID(NS_NSSCOMPONENT_CID);
NS_DEFINE_NAMED_CID(NS_SSLSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_STARTTLSSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_SECRETDECODERRING_CID);
NS_DEFINE_NAMED_CID(NS_PK11TOKENDB_CID);
NS_DEFINE_NAMED_CID(NS_PKCS11MODULEDB_CID);
NS_DEFINE_NAMED_CID(NS_PSMCONTENTLISTEN_CID);
NS_DEFINE_NAMED_CID(NS_X509CERT_CID);
NS_DEFINE_NAMED_CID(NS_X509CERTDB_CID);
NS_DEFINE_NAMED_CID(NS_X509CERTLIST_CID);
NS_DEFINE_NAMED_CID(NS_FORMPROCESSOR_CID);
#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_CERTTREE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_CRYPTO_HASH_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_HMAC_CID);
NS_DEFINE_NAMED_CID(NS_NTLMAUTHMODULE_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECTFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTSIGNATUREVERIFIER_CID);
NS_DEFINE_NAMED_CID(NS_CERTOVERRIDE_CID);
NS_DEFINE_NAMED_CID(NS_RANDOMGENERATOR_CID);
NS_DEFINE_NAMED_CID(NS_SSLSTATUS_CID);
NS_DEFINE_NAMED_CID(TRANSPORTSECURITYINFO_CID);
NS_DEFINE_NAMED_CID(NS_NSSERRORSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_NSSVERSION_CID);
NS_DEFINE_NAMED_CID(NS_SECURE_BROWSER_UI_CID);
NS_DEFINE_NAMED_CID(NS_SITE_SECURITY_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CERT_BLOCKLIST_CID);

// Components that require main thread initialization could cause a deadlock
// in necko code (bug 1418752). To prevent it we initialize all such components
// on main thread in advance in net_EnsurePSMInit(). Update that function when
// new component with ThreadRestriction::MainThreadOnly is added.
static const mozilla::Module::CIDEntry kNSSCIDs[] = {
  { &kNS_NSSCOMPONENT_CID, false, nullptr, nsNSSComponentConstructor },
  { &kNS_SSLSOCKETPROVIDER_CID, false, nullptr,
    Constructor<nsSSLSocketProvider> },
  { &kNS_STARTTLSSOCKETPROVIDER_CID, false, nullptr,
    Constructor<nsTLSSocketProvider> },
  { &kNS_SECRETDECODERRING_CID, false, nullptr,
    Constructor<SecretDecoderRing> },
  { &kNS_PK11TOKENDB_CID, false, nullptr, Constructor<nsPK11TokenDB> },
  { &kNS_PKCS11MODULEDB_CID, false, nullptr, Constructor<PKCS11ModuleDB> },
  { &kNS_PSMCONTENTLISTEN_CID, false, nullptr, PSMContentListenerConstructor },
  { &kNS_X509CERT_CID, false, nullptr,
    Constructor<nsNSSCertificate, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_X509CERTDB_CID, false, nullptr, Constructor<nsNSSCertificateDB> },
  { &kNS_X509CERTLIST_CID, false, nullptr,
    Constructor<nsNSSCertList, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_FORMPROCESSOR_CID, false, nullptr, nsKeygenFormProcessor::Create },
#ifdef MOZ_XUL
  { &kNS_CERTTREE_CID, false, nullptr, Constructor<nsCertTree> },
#endif
  { &kNS_CRYPTO_HASH_CID, false, nullptr,
    Constructor<nsCryptoHash, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_CRYPTO_HMAC_CID, false, nullptr,
    Constructor<nsCryptoHMAC, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_NTLMAUTHMODULE_CID, false, nullptr,
    Constructor<nsNTLMAuthModule, &nsNTLMAuthModule::InitTest> },
  { &kNS_KEYMODULEOBJECT_CID, false, nullptr,
    Constructor<nsKeyObject, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_KEYMODULEOBJECTFACTORY_CID, false, nullptr,
    Constructor<nsKeyObjectFactory, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_CONTENTSIGNATUREVERIFIER_CID, false, nullptr,
    Constructor<ContentSignatureVerifier> },
  { &kNS_CERTOVERRIDE_CID, false, nullptr,
    Constructor<nsCertOverrideService, &nsCertOverrideService::Init,
                ProcessRestriction::ParentProcessOnly,
                ThreadRestriction::MainThreadOnly> },
  { &kNS_RANDOMGENERATOR_CID, false, nullptr,
    Constructor<nsRandomGenerator, nullptr, ProcessRestriction::AnyProcess> },
  { &kNS_SSLSTATUS_CID, false, nullptr,
    Constructor<nsSSLStatus, nullptr, ProcessRestriction::AnyProcess> },
  { &kTRANSPORTSECURITYINFO_CID, false, nullptr,
    Constructor<TransportSecurityInfo, nullptr,
                ProcessRestriction::AnyProcess> },
  { &kNS_NSSERRORSSERVICE_CID, false, nullptr, NSSErrorsServiceConstructor },
  { &kNS_NSSVERSION_CID, false, nullptr, nsNSSVersionConstructor },
  { &kNS_SECURE_BROWSER_UI_CID, false, nullptr, nsSecureBrowserUIImplConstructor },
  { &kNS_SITE_SECURITY_SERVICE_CID, false, nullptr,
    Constructor<nsSiteSecurityService, &nsSiteSecurityService::Init,
                ProcessRestriction::AnyProcess,
                ThreadRestriction::MainThreadOnly> },
  { &kNS_CERT_BLOCKLIST_CID, false, nullptr,
    Constructor<CertBlocklist, &CertBlocklist::Init,
                ProcessRestriction::ParentProcessOnly,
                ThreadRestriction::MainThreadOnly> },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kNSSContracts[] = {
  { PSM_COMPONENT_CONTRACTID, &kNS_NSSCOMPONENT_CID },
  { NS_NSS_ERRORS_SERVICE_CONTRACTID, &kNS_NSSERRORSSERVICE_CID },
  { NS_NSSVERSION_CONTRACTID, &kNS_NSSVERSION_CID },
  { NS_SSLSOCKETPROVIDER_CONTRACTID, &kNS_SSLSOCKETPROVIDER_CID },
  { NS_STARTTLSSOCKETPROVIDER_CONTRACTID, &kNS_STARTTLSSOCKETPROVIDER_CID },
  { NS_SECRETDECODERRING_CONTRACTID, &kNS_SECRETDECODERRING_CID },
  { NS_PK11TOKENDB_CONTRACTID, &kNS_PK11TOKENDB_CID },
  { NS_PKCS11MODULEDB_CONTRACTID, &kNS_PKCS11MODULEDB_CID },
  { NS_PSMCONTENTLISTEN_CONTRACTID, &kNS_PSMCONTENTLISTEN_CID },
  { NS_X509CERTDB_CONTRACTID, &kNS_X509CERTDB_CID },
  { NS_X509CERTLIST_CONTRACTID, &kNS_X509CERTLIST_CID },
  { NS_FORMPROCESSOR_CONTRACTID, &kNS_FORMPROCESSOR_CID },
#ifdef MOZ_XUL
  { NS_CERTTREE_CONTRACTID, &kNS_CERTTREE_CID },
#endif
  { NS_CRYPTO_HASH_CONTRACTID, &kNS_CRYPTO_HASH_CID },
  { NS_CRYPTO_HMAC_CONTRACTID, &kNS_CRYPTO_HMAC_CID },
  { "@mozilla.org/uriloader/psm-external-content-listener;1", &kNS_PSMCONTENTLISTEN_CID },
  { NS_NTLMAUTHMODULE_CONTRACTID, &kNS_NTLMAUTHMODULE_CID },
  { NS_KEYMODULEOBJECT_CONTRACTID, &kNS_KEYMODULEOBJECT_CID },
  { NS_KEYMODULEOBJECTFACTORY_CONTRACTID, &kNS_KEYMODULEOBJECTFACTORY_CID },
  { NS_CONTENTSIGNATUREVERIFIER_CONTRACTID, &kNS_CONTENTSIGNATUREVERIFIER_CID },
  { NS_CERTOVERRIDE_CONTRACTID, &kNS_CERTOVERRIDE_CID },
  { NS_RANDOMGENERATOR_CONTRACTID, &kNS_RANDOMGENERATOR_CID },
  { NS_SECURE_BROWSER_UI_CONTRACTID, &kNS_SECURE_BROWSER_UI_CID },
  { NS_SSSERVICE_CONTRACTID, &kNS_SITE_SECURITY_SERVICE_CID },
  { NS_CERTBLOCKLIST_CONTRACTID, &kNS_CERT_BLOCKLIST_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kNSSCategories[] = {
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-ca-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-server-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-user-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-email-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { nullptr }
};

static const mozilla::Module kNSSModule = {
  mozilla::Module::kVersion,
  kNSSCIDs,
  kNSSContracts,
  kNSSCategories
};

} // unnamed namespace

NSMODULE_DEFN(NSS) = &kNSSModule;
