/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsService.h"
#include "nsToolkitCompsCID.h"
#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsICategoryManager.h"
#include "nsMemory.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAlertsService, Init)
NS_DEFINE_NAMED_CID(NS_ALERTSSERVICE_CID);

static const mozilla::Module::CIDEntry kAlertsCIDs[] = {
  { &kNS_ALERTSSERVICE_CID, false, NULL, nsAlertsServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kAlertsContracts[] = {
  { NS_ALERTSERVICE_CONTRACTID, &kNS_ALERTSSERVICE_CID },
  { NULL }
};

static const mozilla::Module kAlertsModule = {
  mozilla::Module::kVersion,
  kAlertsCIDs,
  kAlertsContracts
};

NSMODULE_DEFN(nsAlertsServiceModule) = &kAlertsModule;
