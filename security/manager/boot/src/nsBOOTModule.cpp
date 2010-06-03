/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "nsEntropyCollector.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsSecurityWarningDialogs.h"
#include "nsStrictTransportSecurityService.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntropyCollector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSecurityWarningDialogs, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStrictTransportSecurityService, Init)

NS_DEFINE_NAMED_CID(NS_ENTROPYCOLLECTOR_CID);
NS_DEFINE_NAMED_CID(NS_SECURITYWARNINGDIALOGS_CID);
NS_DEFINE_NAMED_CID(NS_SECURE_BROWSER_UI_CID);
NS_DEFINE_NAMED_CID(NS_STRICT_TRANSPORT_SECURITY_CID);

static const mozilla::Module::CIDEntry kBOOTCIDs[] = {
  { &kNS_ENTROPYCOLLECTOR_CID, false, NULL, nsEntropyCollectorConstructor },
  { &kNS_SECURITYWARNINGDIALOGS_CID, false, NULL, nsSecurityWarningDialogsConstructor },
  { &kNS_SECURE_BROWSER_UI_CID, false, NULL, nsSecureBrowserUIImplConstructor },
  { &kNS_STRICT_TRANSPORT_SECURITY_CID, false, NULL, nsStrictTransportSecurityServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kBOOTContracts[] = {
  { NS_ENTROPYCOLLECTOR_CONTRACTID, &kNS_ENTROPYCOLLECTOR_CID },
  { NS_SECURITYWARNINGDIALOGS_CONTRACTID, &kNS_SECURITYWARNINGDIALOGS_CID },
  { NS_SECURE_BROWSER_UI_CONTRACTID, &kNS_SECURE_BROWSER_UI_CID },
  { NS_STSSERVICE_CONTRACTID, &kNS_STRICT_TRANSPORT_SECURITY_CID },
  { NULL }
};

static const mozilla::Module kBootModule = {
  mozilla::Module::kVersion,
  kBOOTCIDs,
  kBOOTContracts
};

NSMODULE_DEFN(BOOT) = &kBootModule;
