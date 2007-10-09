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
 *   Terry Hayes <thayes@netscape.com>
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

#include "nsNSSDialogs.h"
#include "nsPKIParamBlock.h"
#include "nsASN1Tree.h"
#include "nsFormSigningDialog.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSDialogs, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPKIParamBlock, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSASN1Tree)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormSigningDialog)

#define NSS_DIALOGS_DESCRIPTION "PSM Dialog Impl"

static const nsModuleComponentInfo components[] =
{
  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_TOKENPASSWORDSDIALOG_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_CERTIFICATEDIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_CLIENTAUTHDIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_CERTPICKDIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_TOKENDIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_DOMCRYPTODIALOGS_CONTRACTID,
    nsNSSDialogsConstructor
  },

  {
    NSS_DIALOGS_DESCRIPTION,
    NS_NSSDIALOGS_CID,
    NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID,
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
  },

  { "Form Signing Dialog", 
    NS_FORMSIGNINGDIALOG_CID,
    NS_FORMSIGNINGDIALOG_CONTRACTID,
    nsFormSigningDialogConstructor
  }
};

NS_IMPL_NSGETMODULE(PKI, components)
