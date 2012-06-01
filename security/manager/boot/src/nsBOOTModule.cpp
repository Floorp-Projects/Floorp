/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
