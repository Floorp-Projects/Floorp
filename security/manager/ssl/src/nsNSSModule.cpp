/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/ModuleUtils.h"

#include "nsNSSComponent.h"
#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"
#include "nsKeygenHandler.h"

#include "nsSDR.h"

#include "nsPK11TokenDB.h"
#include "nsPKCS11Slot.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateFakeTransport.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSCertCache.h"
#include "nsCMS.h"
#ifdef MOZ_XUL
#include "nsCertTree.h"
#endif
#include "nsCrypto.h"
//For the NS_CRYPTO_CONTRACTID define
#include "nsDOMCID.h"

#include "nsCMSSecureMessage.h"
#include "nsCertPicker.h"
#include "nsCURILoader.h"
#include "nsICategoryManager.h"
#include "nsCRLManager.h"
#include "nsCipherInfo.h"
#include "nsNTLMAuthModule.h"
#include "nsStreamCipher.h"
#include "nsKeyModule.h"
#include "nsDataSignatureVerifier.h"
#include "nsCertOverrideService.h"
#include "nsRandomGenerator.h"
#include "nsRecentBadCerts.h"
#include "nsSSLStatus.h"
#include "nsNSSIOLayer.h"
#include "NSSErrorsService.h"

#ifdef MOZ_IPC
#include "nsXULAppAPI.h"
#define NS_IS_PROCESS_DEFAULT                                                 \
    (GeckoProcessType_Default == XRE_GetProcessType())
#else
#define NS_IS_PROCESS_DEFAULT                                                 \
    (true)
#endif

#define NS_NSS_INSTANTIATE(ensureOperator, _InstanceClass)                    \
    PR_BEGIN_MACRO                                                            \
        _InstanceClass * inst;                                                \
        inst = new _InstanceClass();                                          \
        if (NULL == inst) {                                                   \
            if (ensureOperator == nssLoadingComponent)                        \
                EnsureNSSInitialized(nssInitFailed);                          \
            rv = NS_ERROR_OUT_OF_MEMORY;                                      \
            return rv;                                                        \
        }                                                                     \
        NS_ADDREF(inst);                                                      \
        rv = inst->QueryInterface(aIID, aResult);                             \
        NS_RELEASE(inst);                                                     \
    PR_END_MACRO

#define NS_NSS_INSTANTIATE_INIT(ensureOperator, _InstanceClass, _InitMethod)  \
    PR_BEGIN_MACRO                                                            \
        _InstanceClass * inst;                                                \
        inst = new _InstanceClass();                                          \
        if (NULL == inst) {                                                   \
            if (ensureOperator == nssLoadingComponent)                        \
                EnsureNSSInitialized(nssInitFailed);                          \
            rv = NS_ERROR_OUT_OF_MEMORY;                                      \
            return rv;                                                        \
        }                                                                     \
        NS_ADDREF(inst);                                                      \
        rv = inst->_InitMethod();                                             \
        if(NS_SUCCEEDED(rv)) {                                                \
            rv = inst->QueryInterface(aIID, aResult);                         \
        }                                                                     \
        NS_RELEASE(inst);                                                     \
   PR_END_MACRO


#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(ensureOperator,                    \
                                           _InstanceClass)                    \
   NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_BYPROCESS(ensureOperator,               \
                                                _InstanceClass,               \
                                                _InstanceClass)

// These two macros are ripped off from nsIGenericFactory.h and slightly
// modified.
#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_BYPROCESS(ensureOperator,          \
                                                     _InstanceClassChrome,    \
                                                     _InstanceClassContent)   \
static nsresult                                                               \
_InstanceClassChrome##Constructor(nsISupports *aOuter, REFNSIID aIID,         \
                                  void **aResult)                             \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    if (!EnsureNSSInitialized(ensureOperator))                                \
        return NS_ERROR_FAILURE;                                              \
                                                                              \
    if (NS_IS_PROCESS_DEFAULT)                                                \
        NS_NSS_INSTANTIATE(ensureOperator, _InstanceClassChrome);             \
    else                                                                      \
        NS_NSS_INSTANTIATE(ensureOperator, _InstanceClassContent);            \
                                                                              \
    if (ensureOperator == nssLoadingComponent)                                \
    {                                                                         \
        if (NS_SUCCEEDED(rv))                                                 \
            EnsureNSSInitialized(nssInitSucceeded);                           \
        else                                                                  \
            EnsureNSSInitialized(nssInitFailed);                              \
    }                                                                         \
                                                                              \
    return rv;                                                                \
}

 
#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ensureOperator,               \
                                                _InstanceClass,               \
                                                _InitMethod)                  \
    NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT_BYPROCESS(ensureOperator,         \
                                                      _InstanceClass,         \
                                                      _InstanceClass,         \
                                                      _InitMethod)

#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT_BYPROCESS(ensureOperator,     \
                                                _InstanceClassChrome,         \
                                                _InstanceClassContent,        \
                                                _InitMethod)                  \
static nsresult                                                               \
_InstanceClassChrome##Constructor(nsISupports *aOuter, REFNSIID aIID,         \
                                  void **aResult)                             \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    if (!EnsureNSSInitialized(ensureOperator))                                \
        return NS_ERROR_FAILURE;                                              \
                                                                              \
    if (NS_IS_PROCESS_DEFAULT)                                                \
        NS_NSS_INSTANTIATE_INIT(ensureOperator,                               \
                                _InstanceClassChrome,                         \
                                _InitMethod);                                 \
    else                                                                      \
        NS_NSS_INSTANTIATE_INIT(ensureOperator,                               \
                                _InstanceClassContent,                        \
                                _InitMethod);                                 \
                                                                              \
    if (ensureOperator == nssLoadingComponent)                                \
    {                                                                         \
        if (NS_SUCCEEDED(rv))                                                 \
            EnsureNSSInitialized(nssInitSucceeded);                           \
        else                                                                  \
            EnsureNSSInitialized(nssInitFailed);                              \
    }                                                                         \
                                                                              \
    return rv;                                                                \
}

NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nssLoadingComponent, nsNSSComponent,
                                        Init)

// Use the special factory constructor for everything this module implements,
// because all code could potentially require the NSS library.
// Our factory constructor takes an additional boolean parameter.
// Only for the nsNSSComponent, set this to PR_TRUE.
// All other classes must have this set to PR_FALSE.

NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsSSLSocketProvider)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsTLSSocketProvider)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsSecretDecoderRing)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsPK11TokenDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsPKCS11ModuleDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nssEnsure, PSMContentListener, init)
#ifdef MOZ_IPC
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_BYPROCESS(nssEnsureOnChromeOnly,
                                             nsNSSCertificate,
                                             nsNSSCertificateFakeTransport)
#else
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsNSSCertificate)
#endif
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsNSSCertificateDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsNSSCertCache)
#ifdef MOZ_XUL
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCertTree)
#endif
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCrypto)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsPkcs11)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCMSSecureMessage)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCMSDecoder)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCMSEncoder)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCMSMessage)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCertPicker)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCRLManager)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCipherInfoService)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nssEnsure, nsNTLMAuthModule, InitTest)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCryptoHash)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsCryptoHMAC)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsStreamCipher)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsKeyObject)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsKeyObjectFactory)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsDataSignatureVerifier)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nssEnsure, nsCertOverrideService, Init)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsure, nsRandomGenerator)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nssEnsure, nsRecentBadCertsService, Init)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsureOnChromeOnly, nsSSLStatus)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(nssEnsureOnChromeOnly, nsNSSSocketInfo)

typedef mozilla::psm::NSSErrorsService NSSErrorsService;
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(NSSErrorsService, Init)

NS_DEFINE_NAMED_CID(NS_NSSCOMPONENT_CID);
NS_DEFINE_NAMED_CID(NS_SSLSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_STARTTLSSOCKETPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_SDR_CID);
NS_DEFINE_NAMED_CID(NS_PK11TOKENDB_CID);
NS_DEFINE_NAMED_CID(NS_PKCS11MODULEDB_CID);
NS_DEFINE_NAMED_CID(NS_PSMCONTENTLISTEN_CID);
NS_DEFINE_NAMED_CID(NS_X509CERT_CID);
NS_DEFINE_NAMED_CID(NS_X509CERTDB_CID);
NS_DEFINE_NAMED_CID(NS_NSSCERTCACHE_CID);
NS_DEFINE_NAMED_CID(NS_FORMPROCESSOR_CID);
#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_CERTTREE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_PKCS11_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_CID);
NS_DEFINE_NAMED_CID(NS_CMSSECUREMESSAGE_CID);
NS_DEFINE_NAMED_CID(NS_CMSDECODER_CID);
NS_DEFINE_NAMED_CID(NS_CMSENCODER_CID);
NS_DEFINE_NAMED_CID(NS_CMSMESSAGE_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_HASH_CID);
NS_DEFINE_NAMED_CID(NS_CRYPTO_HMAC_CID);
NS_DEFINE_NAMED_CID(NS_CERT_PICKER_CID);
NS_DEFINE_NAMED_CID(NS_CRLMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_CIPHERINFOSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_NTLMAUTHMODULE_CID);
NS_DEFINE_NAMED_CID(NS_STREAMCIPHER_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECT_CID);
NS_DEFINE_NAMED_CID(NS_KEYMODULEOBJECTFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_DATASIGNATUREVERIFIER_CID);
NS_DEFINE_NAMED_CID(NS_CERTOVERRIDE_CID);
NS_DEFINE_NAMED_CID(NS_RANDOMGENERATOR_CID);
NS_DEFINE_NAMED_CID(NS_RECENTBADCERTS_CID);
NS_DEFINE_NAMED_CID(NS_SSLSTATUS_CID);
NS_DEFINE_NAMED_CID(NS_NSSSOCKETINFO_CID);
NS_DEFINE_NAMED_CID(NS_NSSERRORSSERVICE_CID);


static const mozilla::Module::CIDEntry kNSSCIDs[] = {
  { &kNS_NSSCOMPONENT_CID, false, NULL, nsNSSComponentConstructor },
  { &kNS_SSLSOCKETPROVIDER_CID, false, NULL, nsSSLSocketProviderConstructor },
  { &kNS_STARTTLSSOCKETPROVIDER_CID, false, NULL, nsTLSSocketProviderConstructor },
  { &kNS_SDR_CID, false, NULL, nsSecretDecoderRingConstructor },
  { &kNS_PK11TOKENDB_CID, false, NULL, nsPK11TokenDBConstructor },
  { &kNS_PKCS11MODULEDB_CID, false, NULL, nsPKCS11ModuleDBConstructor },
  { &kNS_PSMCONTENTLISTEN_CID, false, NULL, PSMContentListenerConstructor },
  { &kNS_X509CERT_CID, false, NULL, nsNSSCertificateConstructor },
  { &kNS_X509CERTDB_CID, false, NULL, nsNSSCertificateDBConstructor },
  { &kNS_NSSCERTCACHE_CID, false, NULL, nsNSSCertCacheConstructor },
  { &kNS_FORMPROCESSOR_CID, false, NULL, nsKeygenFormProcessor::Create },
#ifdef MOZ_XUL
  { &kNS_CERTTREE_CID, false, NULL, nsCertTreeConstructor },
#endif
  { &kNS_PKCS11_CID, false, NULL, nsPkcs11Constructor },
  { &kNS_CRYPTO_CID, false, NULL, nsCryptoConstructor },
  { &kNS_CMSSECUREMESSAGE_CID, false, NULL, nsCMSSecureMessageConstructor },
  { &kNS_CMSDECODER_CID, false, NULL, nsCMSDecoderConstructor },
  { &kNS_CMSENCODER_CID, false, NULL, nsCMSEncoderConstructor },
  { &kNS_CMSMESSAGE_CID, false, NULL, nsCMSMessageConstructor },
  { &kNS_CRYPTO_HASH_CID, false, NULL, nsCryptoHashConstructor },
  { &kNS_CRYPTO_HMAC_CID, false, NULL, nsCryptoHMACConstructor },
  { &kNS_CERT_PICKER_CID, false, NULL, nsCertPickerConstructor },
  { &kNS_CRLMANAGER_CID, false, NULL, nsCRLManagerConstructor },
  { &kNS_CIPHERINFOSERVICE_CID, false, NULL, nsCipherInfoServiceConstructor },
  { &kNS_NTLMAUTHMODULE_CID, false, NULL, nsNTLMAuthModuleConstructor },
  { &kNS_STREAMCIPHER_CID, false, NULL, nsStreamCipherConstructor },
  { &kNS_KEYMODULEOBJECT_CID, false, NULL, nsKeyObjectConstructor },
  { &kNS_KEYMODULEOBJECTFACTORY_CID, false, NULL, nsKeyObjectFactoryConstructor },
  { &kNS_DATASIGNATUREVERIFIER_CID, false, NULL, nsDataSignatureVerifierConstructor },
  { &kNS_CERTOVERRIDE_CID, false, NULL, nsCertOverrideServiceConstructor },
  { &kNS_RANDOMGENERATOR_CID, false, NULL, nsRandomGeneratorConstructor },
  { &kNS_RECENTBADCERTS_CID, false, NULL, nsRecentBadCertsServiceConstructor },
  { &kNS_SSLSTATUS_CID, false, NULL, nsSSLStatusConstructor },
  { &kNS_NSSSOCKETINFO_CID, false, NULL, nsNSSSocketInfoConstructor },
  { &kNS_NSSERRORSSERVICE_CID, false, NULL, NSSErrorsServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kNSSContracts[] = {
  { PSM_COMPONENT_CONTRACTID, &kNS_NSSCOMPONENT_CID },
  { NS_NSS_ERRORS_SERVICE_CONTRACTID, &kNS_NSSERRORSSERVICE_CID },
  { NS_SSLSOCKETPROVIDER_CONTRACTID, &kNS_SSLSOCKETPROVIDER_CID },
  { NS_STARTTLSSOCKETPROVIDER_CONTRACTID, &kNS_STARTTLSSOCKETPROVIDER_CID },
  { NS_SDR_CONTRACTID, &kNS_SDR_CID },
  { NS_PK11TOKENDB_CONTRACTID, &kNS_PK11TOKENDB_CID },
  { NS_PKCS11MODULEDB_CONTRACTID, &kNS_PKCS11MODULEDB_CID },
  { NS_PSMCONTENTLISTEN_CONTRACTID, &kNS_PSMCONTENTLISTEN_CID },
  { NS_X509CERTDB_CONTRACTID, &kNS_X509CERTDB_CID },
  { NS_NSSCERTCACHE_CONTRACTID, &kNS_NSSCERTCACHE_CID },
  { NS_FORMPROCESSOR_CONTRACTID, &kNS_FORMPROCESSOR_CID },
#ifdef MOZ_XUL
  { NS_CERTTREE_CONTRACTID, &kNS_CERTTREE_CID },
#endif
  { NS_PKCS11_CONTRACTID, &kNS_PKCS11_CID },
  { NS_CRYPTO_CONTRACTID, &kNS_CRYPTO_CID },
  { NS_CMSSECUREMESSAGE_CONTRACTID, &kNS_CMSSECUREMESSAGE_CID },
  { NS_CMSDECODER_CONTRACTID, &kNS_CMSDECODER_CID },
  { NS_CMSENCODER_CONTRACTID, &kNS_CMSENCODER_CID },
  { NS_CMSMESSAGE_CONTRACTID, &kNS_CMSMESSAGE_CID },
  { NS_CRYPTO_HASH_CONTRACTID, &kNS_CRYPTO_HASH_CID },
  { NS_CRYPTO_HMAC_CONTRACTID, &kNS_CRYPTO_HMAC_CID },
  { NS_CERT_PICKER_CONTRACTID, &kNS_CERT_PICKER_CID },
  { "@mozilla.org/uriloader/psm-external-content-listener;1", &kNS_PSMCONTENTLISTEN_CID },
  { NS_CRLMANAGER_CONTRACTID, &kNS_CRLMANAGER_CID },
  { NS_CIPHERINFOSERVICE_CONTRACTID, &kNS_CIPHERINFOSERVICE_CID },
  { NS_CRYPTO_FIPSINFO_SERVICE_CONTRACTID, &kNS_PKCS11MODULEDB_CID },
  { NS_NTLMAUTHMODULE_CONTRACTID, &kNS_NTLMAUTHMODULE_CID },
  { NS_STREAMCIPHER_CONTRACTID, &kNS_STREAMCIPHER_CID },
  { NS_KEYMODULEOBJECT_CONTRACTID, &kNS_KEYMODULEOBJECT_CID },
  { NS_KEYMODULEOBJECTFACTORY_CONTRACTID, &kNS_KEYMODULEOBJECTFACTORY_CID },
  { NS_DATASIGNATUREVERIFIER_CONTRACTID, &kNS_DATASIGNATUREVERIFIER_CID },
  { NS_CERTOVERRIDE_CONTRACTID, &kNS_CERTOVERRIDE_CID },
  { NS_RANDOMGENERATOR_CONTRACTID, &kNS_RANDOMGENERATOR_CID },
  { NS_RECENTBADCERTS_CONTRACTID, &kNS_RECENTBADCERTS_CID },
  { NULL }
};

static const mozilla::Module::CategoryEntry kNSSCategories[] = {
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-ca-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-server-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-user-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-email-cert", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-pkcs7-crl", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/x-x509-crl", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY, "application/pkix-crl", "@mozilla.org/uriloader/psm-external-content-listener;1" },
  { NULL }
};

static const mozilla::Module kNSSModule = {
  mozilla::Module::kVersion,
  kNSSCIDs,
  kNSSContracts,
  kNSSCategories
};

NSMODULE_DEFN(NSS) = &kNSSModule;
