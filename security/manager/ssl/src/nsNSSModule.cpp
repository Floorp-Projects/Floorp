/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsNSSComponent.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"
#include "nsKeygenHandler.h"

#include "nsCURILoader.h"

#include "nsSDR.h"

#include "nsPK11TokenDB.h"
#include "nsPKCS11Slot.h"
#include "nsNSSCertificate.h"
#include "nsCertOutliner.h"
#include "nsCrypto.h"
//For the NS_CRYPTO_CONTRACTID define
#include "nsDOMCID.h"

#include "nsCMSSecureMessage.h"
#include "nsCMS.h"
#include "nsCertPicker.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSComponent, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSSLSocketProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTLSSocketProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecretDecoderRing)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPK11TokenDB)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPKCS11ModuleDB)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(PSMContentListener, init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSCertificateDB)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCertOutliner)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCrypto)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPkcs11)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCMSSecureMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCMSDecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCMSEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCMSMessage)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHash)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCertPicker)

static nsModuleComponentInfo components[] =
{
  {
    PSM_COMPONENT_CLASSNAME,
    NS_NSSCOMPONENT_CID,
    PSM_COMPONENT_CONTRACTID,
    nsNSSComponentConstructor
  },
  
  {
    "NSS Content Handler - application/x-x509-ca-cert",
    NS_NSSCOMPONENT_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-x509-ca-cert",
    nsNSSComponentConstructor
  },
  
  {
    "NSS Content Handler - application/x-x509-server-cert",
    NS_NSSCOMPONENT_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-x509-server-cert",
    nsNSSComponentConstructor
  },
  
  {
    "NSS Content Handler - application/x-x509-user-cert",
    NS_NSSCOMPONENT_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-x509-user-cert",
    nsNSSComponentConstructor
  },
  
  {
    "NSS Content Handler - application/x-x509-email-cert",
    NS_NSSCOMPONENT_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-x509-email-cert",
    nsNSSComponentConstructor
  },
  
  {
    NS_SECURE_BROWSER_UI_CLASSNAME,
    NS_SECURE_BROWSER_UI_CID,
    NS_SECURE_BROWSER_UI_CONTRACTID,
    nsSecureBrowserUIImplConstructor
  },
  
  {
    NS_ISSLSOCKETPROVIDER_CLASSNAME,
    NS_SSLSOCKETPROVIDER_CID,
    NS_ISSLSOCKETPROVIDER_CONTRACTID,
    nsSSLSocketProviderConstructor
  },
  
  {
    NS_TLSSTEPUPSOCKETPROVIDER_CLASSNAME,
    NS_TLSSTEPUPSOCKETPROVIDER_CID,
    NS_TLSSTEPUPSOCKETPROVIDER_CONTRACTID,
    nsTLSSocketProviderConstructor
  },
  
  {
    NS_ISSLFHSOCKETPROVIDER_CLASSNAME,
    NS_SSLSOCKETPROVIDER_CID,
    NS_ISSLFHSOCKETPROVIDER_CONTRACTID,
    nsSSLSocketProviderConstructor
  },

  {
    NS_SDR_CLASSNAME,
    NS_SDR_CID,
    NS_SDR_CONTRACTID,
    nsSecretDecoderRingConstructor
  },

  {
    "Entropy Collector",
    NS_NSSCOMPONENT_CID,
    NS_ENTROPYCOLLECTOR_CONTRACTID,
    nsNSSComponentConstructor
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
    "Form Processor",
    NS_FORMPROCESSOR_CID,
    NS_FORMPROCESSOR_CONTRACTID,
    nsKeygenFormProcessor::Create
  },

  {
    "Certificate Outliner",
    NS_CERTOUTLINER_CID,
    NS_CERTOUTLINER_CONTRACTID,
    nsCertOutlinerConstructor
  },

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
    NS_HASH_CLASSNAME,
    NS_HASH_CID,
    NS_HASH_CONTRACTID,
    nsHashConstructor
  },

  {
    NS_CERT_PICKER_CLASSNAME,
    NS_CERT_PICKER_CID,
    NS_CERT_PICKER_CONTRACTID,
    nsCertPickerConstructor
  }
};

NS_IMPL_NSGETMODULE(NSS, components);
