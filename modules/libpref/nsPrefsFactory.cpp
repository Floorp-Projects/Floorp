/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/Preferences.h"
#include "nsPrefBranch.h"
#include "prefapi.h"

using namespace mozilla;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(Preferences,
                                         Preferences::GetInstanceForService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefLocalizedString, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRelativeFilePref)

static NS_DEFINE_CID(kPrefServiceCID, NS_PREFSERVICE_CID);
static NS_DEFINE_CID(kPrefLocalizedStringCID, NS_PREFLOCALIZEDSTRING_CID);
static NS_DEFINE_CID(kRelativeFilePrefCID, NS_RELATIVEFILEPREF_CID);

static mozilla::Module::CIDEntry kPrefCIDs[] = {
  { &kPrefServiceCID, true, nullptr, PreferencesConstructor },
  { &kPrefLocalizedStringCID, false, nullptr, nsPrefLocalizedStringConstructor },
  { &kRelativeFilePrefCID, false, nullptr, nsRelativeFilePrefConstructor },
  { nullptr }
};

static mozilla::Module::ContractIDEntry kPrefContracts[] = {
  { NS_PREFSERVICE_CONTRACTID, &kPrefServiceCID },
  { NS_PREFLOCALIZEDSTRING_CONTRACTID, &kPrefLocalizedStringCID },
  { NS_RELATIVEFILEPREF_CONTRACTID, &kRelativeFilePrefCID },
  // compatibility for extension that uses old service
  { "@mozilla.org/preferences;1", &kPrefServiceCID },
  { nullptr }
};

static void
UnloadPrefsModule()
{
  Preferences::Shutdown();
}

static const mozilla::Module kPrefModule = {
  mozilla::Module::kVersion,
  kPrefCIDs,
  kPrefContracts,
  nullptr,
  nullptr,
  nullptr,
  UnloadPrefsModule
};

NSMODULE_DEFN(nsPrefModule) = &kPrefModule;
