/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nscore.h"
#include "nsIWindowMediator.h"

#include "nsIAppShellService.h"
#include "nsAppShellService.h"
#include "nsWindowMediator.h"
#include "nsChromeTreeOwner.h"
#include "nsAppShellCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShellService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowMediator, Init)

NS_DEFINE_NAMED_CID(NS_APPSHELLSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_WINDOWMEDIATOR_CID);

static const mozilla::Module::CIDEntry kAppShellCIDs[] = {
  { &kNS_APPSHELLSERVICE_CID, false, nullptr, nsAppShellServiceConstructor },
  { &kNS_WINDOWMEDIATOR_CID, false, nullptr, nsWindowMediatorConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kAppShellContracts[] = {
  { NS_APPSHELLSERVICE_CONTRACTID, &kNS_APPSHELLSERVICE_CID },
  { NS_WINDOWMEDIATOR_CONTRACTID, &kNS_WINDOWMEDIATOR_CID },
  { nullptr }
};

static nsresult
nsAppShellModuleConstructor()
{
  return nsChromeTreeOwner::InitGlobals();
}

static void
nsAppShellModuleDestructor()
{
  nsChromeTreeOwner::FreeGlobals();
}

static const mozilla::Module kAppShellModule = {
  mozilla::Module::kVersion,
  kAppShellCIDs,
  kAppShellContracts,
  nullptr,
  nullptr,
  nsAppShellModuleConstructor,
  nsAppShellModuleDestructor
};

NSMODULE_DEFN(appshell) = &kAppShellModule;
