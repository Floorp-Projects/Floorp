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

#include "nsCURILoader.h"

#include "nsSDR.h"

#include "nsPK11TokenDB.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSComponent, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSSLSocketProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTLSSocketProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecretDecoderRing)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPK11TokenDB)

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
    NS_TLSSOCKETPROVIDER_CLASSNAME,
    NS_TLSSOCKETPROVIDER_CID,
    NS_TLSSOCKETPROVIDER_CONTRACTID,
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
};

NS_IMPL_NSGETMODULE("NSS", components);
