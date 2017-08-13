/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsShellService.h"

#ifdef MOZ_ANDROID_HISTORY
#include "nsDocShellCID.h"
#include "nsAndroidHistory.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsShellService)
NS_DEFINE_NAMED_CID(nsShellService_CID);

#ifdef MOZ_ANDROID_HISTORY
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsAndroidHistory, nsAndroidHistory::GetSingleton)
NS_DEFINE_NAMED_CID(NS_ANDROIDHISTORY_CID);
#endif

static const mozilla::Module::CIDEntry kBrowserCIDs[] = {
  { &knsShellService_CID, false, nullptr, nsShellServiceConstructor },
#ifdef MOZ_ANDROID_HISTORY
  { &kNS_ANDROIDHISTORY_CID, false, nullptr, nsAndroidHistoryConstructor },
#endif
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kBrowserContracts[] = {
  { nsShellService_ContractID, &knsShellService_CID },
#ifdef MOZ_ANDROID_HISTORY
  { NS_IHISTORY_CONTRACTID, &kNS_ANDROIDHISTORY_CID },
#endif
  { nullptr }
};

static const mozilla::Module kBrowserModule = {
  mozilla::Module::kVersion,
  kBrowserCIDs,
  kBrowserContracts
};

NSMODULE_DEFN(nsBrowserCompsModule) = &kBrowserModule;
