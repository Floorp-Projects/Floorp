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
 *   Terry Hayes <thayes@netscape.com>
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsNSSDialogs.h"
#include "nsPKIParamBlock.h"
#include "nsASN1Tree.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSDialogs, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPKIParamBlock, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSASN1Tree)

static const nsModuleComponentInfo components[] =
{
  {
    "NSS Dialogs",
    NS_NSSDIALOGS_CID,
    NS_NSSDIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    "ASN1 Tree",
    NS_NSSASN1OUTINER_CID,
    NS_ASN1TREE_CONTRACTID,
    nsNSSASN1TreeConstructor
  },

  { "PKI Parm Block", 
    NS_PKIPARAMBLOCK_CID,
    NS_PKIPARAMBLOCK_CONTRACTID,
    nsPKIParamBlockConstructor
  }
};

NS_IMPL_NSGETMODULE(PKI, components)
