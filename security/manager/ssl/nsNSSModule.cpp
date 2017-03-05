/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertBlocklist.h"
#include "ContentSignatureVerifier.h"
#include "NSSErrorsService.h"
#include "PSMContentListener.h"
#include "SecretDecoderRing.h"
#include "TransportSecurityInfo.h"
#include "mozilla/ModuleUtils.h"
#include "nsCURILoader.h"
#include "nsCertOverrideService.h"
#include "nsCrypto.h"
#include "nsCryptoHash.h"
#include "nsDOMCID.h" // For the NS_CRYPTO_CONTRACTID define
#include "nsDataSignatureVerifier.h"
#include "nsICategoryManager.h"
#include "nsKeyModule.h"
#include "nsKeygenHandler.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSCertificateFakeTransport.h"
#include "nsNSSComponent.h"
#include "nsNSSU2FToken.h"
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

MOZ_ALWAYS_INLINE static bool IsProcessDefault()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

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

template<EnsureNSSOperator ensureOperator, class InstanceClass,
         nsresult (InstanceClass::*InitMethod)() = nullptr>
static nsresult
Constructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  *aResult = nullptr;
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  if (ensureOperator == nssEnsureChromeOrContent &&
      !IsProcessDefault()) {
    if (!EnsureNSSInitializedChromeOrContent()) {
      return NS_ERROR_FAILURE;
    }
  } else if (!EnsureNSSInitialized(ensureOperator)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Instantiate<InstanceClass, InitMethod>(aIID, aResult);

  if (ensureOperator == nssLoadingComponent) {
    if (NS_SUCCEEDED(rv)) {
      EnsureNSSInitialized(nssInitSucceeded);
    } else {
      EnsureNSSInitialized(nssInitFailed);
    }
  }

  return rv;
}

template<class InstanceClassChrome, class InstanceClassContent>
MOZ_ALWAYS_INLINE static nsresult
Constructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (IsProcessDefault()) {
    return Constructor<nssEnsureOnChromeOnly,
                       InstanceClassChrome>(aOuter, aIID, aResult);
  }

  return Constructor<nssEnsureOnChromeOnly,
                     InstanceClassContent>(aOuter, aIID, aResult);
}

template<class InstanceClass>
MOZ_ALWAYS_INLINE static nsresult
Constructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  return Constructor<nssEnsure, InstanceClass>(aOuter, aIID, aResult);
}

} } // namespace mozilla::psm

using namespace mozilla::psm;

namespace {

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PSMContentListener, init)

typedef mozilla::psm::NSSErrorsService NSSErrorsService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(NSSErrorsService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSVersion)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCertOverrideService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(CertBlocklist, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSiteSecurityService, Init)

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
NS_DEFINE_NAMED_CID(NS_PKCS11_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_HASH_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_HMAC_CID);
NS_DEFINE_NAMED_CID(NS_NTLMAUTHMODULE_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECTFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_DATASIGNATUREVERIFIER_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTSIGNATUREVERIFIER_CID);
NS_DEFINE_NAMED_CID(NS_CERTOVERRIDE_CID);
NS_DEFINE_NAMED_CID(NS_RANDOMGENERATOR_CID);
NS_DEFINE_NAMED_CID(NS_NSSU2FTOKEN_CID);
NS_DEFINE_NAMED_CID(NS_SSLSTATUS_CID);
NS_DEFINE_NAMED_CID(TRANSPORTSECURITYINFO_CID);
NS_DEFINE_NAMED_CID(NS_NSSERRORSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_NSSVERSION_CID);
NS_DEFINE_NAMED_CID(NS_SECURE_BROWSER_UI_CID);
NS_DEFINE_NAMED_CID(NS_SITE_SECURITY_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_CERT_BLOCKLIST_CID);

// Use the special factory constructor for everything this module implements,
// because all code could potentially require the NSS library.
// Our factory constructor takes an optional EnsureNSSOperator template
// parameter.
// Only for the nsNSSComponent, set this to nssLoadingComponent.
// For classes available from a content process, set this to
// nssEnsureOnChromeOnly.
// All other classes must have this set to nssEnsure (default).

static const mozilla::Module::CIDEntry kNSSCIDs[] = {
  { &kNS_NSSCOMPONENT_CID, false, nullptr,
    Constructor<nssLoadingComponent, nsNSSComponent, &nsNSSComponent::Init> },
  { &kNS_SSLSOCKETPROVIDER_CID, false, nullptr,
    Constructor<nsSSLSocketProvider> },
  { &kNS_STARTTLSSOCKETPROVIDER_CID, false, nullptr,
    Constructor<nsTLSSocketProvider> },
  { &kNS_SECRETDECODERRING_CID, false, nullptr,
    Constructor<SecretDecoderRing> },
  { &kNS_PK11TOKENDB_CID, false, nullptr, Constructor<nsPK11TokenDB> },
  { &kNS_PKCS11MODULEDB_CID, false, nullptr, Constructor<nsPKCS11ModuleDB> },
  { &kNS_PSMCONTENTLISTEN_CID, false, nullptr, PSMContentListenerConstructor },
  { &kNS_X509CERT_CID, false, nullptr,
    Constructor<nsNSSCertificate, nsNSSCertificateFakeTransport> },
  { &kNS_X509CERTDB_CID, false, nullptr, Constructor<nsNSSCertificateDB> },
  { &kNS_X509CERTLIST_CID, false, nullptr,
    Constructor<nsNSSCertList, nsNSSCertListFakeTransport> },
  { &kNS_FORMPROCESSOR_CID, false, nullptr, nsKeygenFormProcessor::Create },
#ifdef MOZ_XUL
  { &kNS_CERTTREE_CID, false, nullptr, Constructor<nsCertTree> },
#endif
  { &kNS_PKCS11_CID, false, nullptr, Constructor<nsPkcs11> },
  { &kNS_CRYPTO_HASH_CID, false, nullptr,
    Constructor<nssEnsureChromeOrContent, nsCryptoHash> },
  { &kNS_CRYPTO_HMAC_CID, false, nullptr,
    Constructor<nssEnsureChromeOrContent, nsCryptoHMAC> },
  { &kNS_NTLMAUTHMODULE_CID, false, nullptr,
    Constructor<nssEnsure, nsNTLMAuthModule, &nsNTLMAuthModule::InitTest> },
  { &kNS_KEYMODULEOBJECT_CID, false, nullptr,
    Constructor<nssEnsureChromeOrContent, nsKeyObject> },
  { &kNS_KEYMODULEOBJECTFACTORY_CID, false, nullptr,
    Constructor<nssEnsureChromeOrContent, nsKeyObjectFactory> },
  { &kNS_DATASIGNATUREVERIFIER_CID, false, nullptr,
    Constructor<nsDataSignatureVerifier> },
  { &kNS_CONTENTSIGNATUREVERIFIER_CID, false, nullptr,
    Constructor<ContentSignatureVerifier> },
  { &kNS_CERTOVERRIDE_CID, false, nullptr, nsCertOverrideServiceConstructor },
  { &kNS_RANDOMGENERATOR_CID, false, nullptr,
    Constructor<nssEnsureChromeOrContent, nsRandomGenerator> },
  { &kNS_NSSU2FTOKEN_CID, false, nullptr,
    Constructor<nssEnsure, nsNSSU2FToken, &nsNSSU2FToken::Init> },
  { &kNS_SSLSTATUS_CID, false, nullptr,
    Constructor<nssEnsureOnChromeOnly, nsSSLStatus> },
  { &kTRANSPORTSECURITYINFO_CID, false, nullptr,
    Constructor<nssEnsureOnChromeOnly, TransportSecurityInfo> },
  { &kNS_NSSERRORSSERVICE_CID, false, nullptr, NSSErrorsServiceConstructor },
  { &kNS_NSSVERSION_CID, false, nullptr, nsNSSVersionConstructor },
  { &kNS_SECURE_BROWSER_UI_CID, false, nullptr, nsSecureBrowserUIImplConstructor },
  { &kNS_SITE_SECURITY_SERVICE_CID, false, nullptr, nsSiteSecurityServiceConstructor },
  { &kNS_CERT_BLOCKLIST_CID, false, nullptr, CertBlocklistConstructor},
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
  { NS_PKCS11_CONTRACTID, &kNS_PKCS11_CID },
  { NS_CRYPTO_HASH_CONTRACTID, &kNS_CRYPTO_HASH_CID },
  { NS_CRYPTO_HMAC_CONTRACTID, &kNS_CRYPTO_HMAC_CID },
  { "@mozilla.org/uriloader/psm-external-content-listener;1", &kNS_PSMCONTENTLISTEN_CID },
  { NS_NTLMAUTHMODULE_CONTRACTID, &kNS_NTLMAUTHMODULE_CID },
  { NS_KEYMODULEOBJECT_CONTRACTID, &kNS_KEYMODULEOBJECT_CID },
  { NS_KEYMODULEOBJECTFACTORY_CONTRACTID, &kNS_KEYMODULEOBJECTFACTORY_CID },
  { NS_DATASIGNATUREVERIFIER_CONTRACTID, &kNS_DATASIGNATUREVERIFIER_CID },
  { NS_CONTENTSIGNATUREVERIFIER_CONTRACTID, &kNS_CONTENTSIGNATUREVERIFIER_CID },
  { NS_CERTOVERRIDE_CONTRACTID, &kNS_CERTOVERRIDE_CID },
  { NS_RANDOMGENERATOR_CONTRACTID, &kNS_RANDOMGENERATOR_CID },
  { NS_NSSU2FTOKEN_CONTRACTID, &kNS_NSSU2FTOKEN_CID },
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
