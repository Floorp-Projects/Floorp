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

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsNSSComponent.h"
#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"
#include "nsKeygenHandler.h"

#include "nsSDR.h"

#include "nsPK11TokenDB.h"
#include "nsPKCS11Slot.h"
#include "nsNSSCertificate.h"
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

// We must ensure that the nsNSSComponent has been loaded before
// creating any other components.
static void EnsureNSSInitialized(PRBool triggeredByNSSComponent)
{
  static PRBool haveLoaded = PR_FALSE;
  if (haveLoaded)
    return;

  haveLoaded = PR_TRUE;
  
  if (triggeredByNSSComponent) {
    // We must prevent a recursion, as nsNSSComponent creates
    // additional instances
    return;
  }
  
  nsCOMPtr<nsISupports> nssComponent 
    = do_GetService(PSM_COMPONENT_CONTRACTID);
}

// These two macros are ripped off from nsIGenericFactory.h and slightly
// modified.
#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(triggeredByNSSComponent,           \
                                                      _InstanceClass)         \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    EnsureNSSInitialized(triggeredByNSSComponent);                            \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_NEWXPCOM(inst, _InstanceClass);                                        \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
    rv = inst->QueryInterface(aIID, aResult);                                 \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}                                                                             \

 
#define NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(triggeredByNSSComponent,      \
                                                _InstanceClass, _InitMethod)  \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    EnsureNSSInitialized(triggeredByNSSComponent);                            \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_NEWXPCOM(inst, _InstanceClass);                                        \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
    NS_ADDREF(inst);                                                          \
    rv = inst->_InitMethod();                                                 \
    if(NS_SUCCEEDED(rv)) {                                                    \
        rv = inst->QueryInterface(aIID, aResult);                             \
    }                                                                         \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}                                                                             \

NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PR_TRUE, nsNSSComponent, Init)

// Use the special factory constructor for everything this module implements,
// because all code could potentially require the NSS library.
// Our factory constructor takes an additional boolean parameter.
// Only for the nsNSSComponent, set this to PR_TRUE.
// All other classes must have this set to PR_FALSE.

NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsSSLSocketProvider)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsTLSSocketProvider)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsSecretDecoderRing)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsPK11TokenDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsPKCS11ModuleDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PR_FALSE, PSMContentListener, init)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsNSSCertificateDB)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsNSSCertCache)
#ifdef MOZ_XUL
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCertTree)
#endif
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCrypto)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsPkcs11)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCMSSecureMessage)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCMSDecoder)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCMSEncoder)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCMSMessage)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCertPicker)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCRLManager)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCipherInfoService)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PR_FALSE, nsNTLMAuthModule, InitTest)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsCryptoHash)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsStreamCipher)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsKeyObject)
NS_NSS_GENERIC_FACTORY_CONSTRUCTOR(PR_FALSE, nsKeyObjectFactory)

static NS_METHOD RegisterPSMContentListeners(
                      nsIComponentManager *aCompMgr,
                      nsIFile *aPath, const char *registryLocation, 
                      const char *componentType, const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = 
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString previous;

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-x509-ca-cert",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-x509-server-cert",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-x509-user-cert",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-x509-email-cert",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-pkcs7-crl",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/x-x509-crl",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  catman->AddCategoryEntry(
    NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
    "application/pkix-crl",
    info->mContractID, PR_TRUE, PR_TRUE, getter_Copies(previous));

  return NS_OK;
}

static const nsModuleComponentInfo components[] =
{
  {
    PSM_COMPONENT_CLASSNAME,
    NS_NSSCOMPONENT_CID,
    PSM_COMPONENT_CONTRACTID,
    nsNSSComponentConstructor
  },
  
  {
    PSM_COMPONENT_CLASSNAME,
    NS_NSSCOMPONENT_CID,
    NS_NSS_ERRORS_SERVICE_CONTRACTID,
    nsNSSComponentConstructor
  },
  
  {
    NS_SSLSOCKETPROVIDER_CLASSNAME,
    NS_SSLSOCKETPROVIDER_CID,
    NS_SSLSOCKETPROVIDER_CONTRACTID,
    nsSSLSocketProviderConstructor
  },
  
  {
    NS_STARTTLSSOCKETPROVIDER_CLASSNAME,
    NS_STARTTLSSOCKETPROVIDER_CID,
    NS_STARTTLSSOCKETPROVIDER_CONTRACTID,
    nsTLSSocketProviderConstructor
  },
  
  {
    NS_SDR_CLASSNAME,
    NS_SDR_CID,
    NS_SDR_CONTRACTID,
    nsSecretDecoderRingConstructor
  },

  {
    "PK11 Token Database",
    NS_PK11TOKENDB_CID,
    NS_PK11TOKENDB_CONTRACTID,
    nsPK11TokenDBConstructor
  },

  {
    "PKCS11 Module Database",
    NS_PKCS11MODULEDB_CID,
    NS_PKCS11MODULEDB_CONTRACTID,
    nsPKCS11ModuleDBConstructor
  },

  {
    "Generic Certificate Content Handler",
    NS_PSMCONTENTLISTEN_CID,
    NS_PSMCONTENTLISTEN_CONTRACTID,
    PSMContentListenerConstructor
  },

  {
    "X509 Certificate Database",
    NS_X509CERTDB_CID,
    NS_X509CERTDB_CONTRACTID,
    nsNSSCertificateDBConstructor
  },

  {
    "NSS Certificate Cache",
    NS_NSSCERTCACHE_CID,
    NS_NSSCERTCACHE_CONTRACTID,
    nsNSSCertCacheConstructor
  },

  {
    "Form Processor",
    NS_FORMPROCESSOR_CID,
    NS_FORMPROCESSOR_CONTRACTID,
    nsKeygenFormProcessor::Create
  },
#ifdef MOZ_XUL
  {
    "Certificate Tree",
    NS_CERTTREE_CID,
    NS_CERTTREE_CONTRACTID,
    nsCertTreeConstructor
  },
#endif
  {
    NS_PKCS11_CLASSNAME,
    NS_PKCS11_CID,
    NS_PKCS11_CONTRACTID,
    nsPkcs11Constructor
  },

  {
    NS_CRYPTO_CLASSNAME,
    NS_CRYPTO_CID,
    NS_CRYPTO_CONTRACTID,
    nsCryptoConstructor
  },

  {
    NS_CMSSECUREMESSAGE_CLASSNAME,
    NS_CMSSECUREMESSAGE_CID,
    NS_CMSSECUREMESSAGE_CONTRACTID,
    nsCMSSecureMessageConstructor
  },

  {
    NS_CMSDECODER_CLASSNAME,
    NS_CMSDECODER_CID,
    NS_CMSDECODER_CONTRACTID,
    nsCMSDecoderConstructor
  },

  {
    NS_CMSENCODER_CLASSNAME,
    NS_CMSENCODER_CID,
    NS_CMSENCODER_CONTRACTID,
    nsCMSEncoderConstructor
  },

  {
    NS_CMSMESSAGE_CLASSNAME,
    NS_CMSMESSAGE_CID,
    NS_CMSMESSAGE_CONTRACTID,
    nsCMSMessageConstructor
  },

  {
    NS_CRYPTO_HASH_CLASSNAME,
    NS_CRYPTO_HASH_CID,
    NS_CRYPTO_HASH_CONTRACTID,
    nsCryptoHashConstructor
  },

  {
    NS_CERT_PICKER_CLASSNAME,
    NS_CERT_PICKER_CID,
    NS_CERT_PICKER_CONTRACTID,
    nsCertPickerConstructor
  },

  {
    "PSM Content Listeners",
    NS_PSMCONTENTLISTEN_CID,
    "@mozilla.org/uriloader/psm-external-content-listener;1",
    PSMContentListenerConstructor,
    RegisterPSMContentListeners
  },

  {
    "PSM CRL Manager",
    NS_CRLMANAGER_CID,
    NS_CRLMANAGER_CONTRACTID,
    nsCRLManagerConstructor
  },
  
  {
    "PSM Cipher Info",
    NS_CIPHERINFOSERVICE_CID,
    NS_CIPHERINFOSERVICE_CONTRACTID,
    nsCipherInfoServiceConstructor
  },
  
  {
    NS_CRYPTO_FIPSINFO_SERVICE_CLASSNAME,
    NS_PKCS11MODULEDB_CID,
    NS_CRYPTO_FIPSINFO_SERVICE_CONTRACTID,
    nsPKCS11ModuleDBConstructor
  },

  {
    NS_NTLMAUTHMODULE_CLASSNAME,
    NS_NTLMAUTHMODULE_CID,
    NS_NTLMAUTHMODULE_CONTRACTID,
    nsNTLMAuthModuleConstructor
  },

  {
    NS_STREAMCIPHER_CLASSNAME,
    NS_STREAMCIPHER_CID,
    NS_STREAMCIPHER_CONTRACTID,
    nsStreamCipherConstructor
  },

  {
    NS_KEYMODULEOBJECT_CLASSNAME,
    NS_KEYMODULEOBJECT_CID,
    NS_KEYMODULEOBJECT_CONTRACTID,
    nsKeyObjectConstructor
  },

  {
    NS_KEYMODULEOBJECTFACTORY_CLASSNAME,
    NS_KEYMODULEOBJECTFACTORY_CID,
    NS_KEYMODULEOBJECTFACTORY_CONTRACTID,
    nsKeyObjectFactoryConstructor
  }
};

NS_IMPL_NSGETMODULE(NSS, components)
