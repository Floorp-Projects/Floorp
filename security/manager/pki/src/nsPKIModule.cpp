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

#include "mozilla/ModuleUtils.h"

#include "nsNSSDialogs.h"
#include "nsPKIParamBlock.h"
#include "nsASN1Tree.h"
#include "nsFormSigningDialog.h"
#include "nsISSLCertErrorDialog.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNSSDialogs, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPKIParamBlock, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNSSASN1Tree)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFormSigningDialog)

NS_DEFINE_NAMED_CID(NS_NSSDIALOGS_CID);
NS_DEFINE_NAMED_CID(NS_NSSASN1OUTINER_CID);
NS_DEFINE_NAMED_CID(NS_PKIPARAMBLOCK_CID);
NS_DEFINE_NAMED_CID(NS_FORMSIGNINGDIALOG_CID);


static const mozilla::Module::CIDEntry kPKICIDs[] = {
  { &kNS_NSSDIALOGS_CID, false, NULL, nsNSSDialogsConstructor },
  { &kNS_NSSASN1OUTINER_CID, false, NULL, nsNSSASN1TreeConstructor },
  { &kNS_PKIPARAMBLOCK_CID, false, NULL, nsPKIParamBlockConstructor },
  { &kNS_FORMSIGNINGDIALOG_CID, false, NULL, nsFormSigningDialogConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kPKIContracts[] = {
  { NS_SSLCERTERRORDIALOG_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_TOKENPASSWORDSDIALOG_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_CERTIFICATEDIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_CLIENTAUTHDIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_CERTPICKDIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_TOKENDIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_DOMCRYPTODIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID, &kNS_NSSDIALOGS_CID },
  { NS_ASN1TREE_CONTRACTID, &kNS_NSSASN1OUTINER_CID },
  { NS_PKIPARAMBLOCK_CONTRACTID, &kNS_PKIPARAMBLOCK_CID },
  { NS_FORMSIGNINGDIALOG_CONTRACTID, &kNS_FORMSIGNINGDIALOG_CID },
  { NULL }
};

static const mozilla::Module kPKIModule = {
  mozilla::Module::kVersion,
  kPKICIDs,
  kPKIContracts
};

NSMODULE_DEFN(PKI) = &kPKIModule;
