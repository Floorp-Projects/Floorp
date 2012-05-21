/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"

#include "mozStorageService.h"
#include "mozStorageConnection.h"
#include "VacuumManager.h"

#include "mozStorageCID.h"

namespace mozilla {
namespace storage {

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(Service,
                                         Service::getSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(VacuumManager,
                                         VacuumManager::getSingleton)

} // namespace storage
} // namespace mozilla

NS_DEFINE_NAMED_CID(MOZ_STORAGE_SERVICE_CID);
NS_DEFINE_NAMED_CID(VACUUMMANAGER_CID);

static const mozilla::Module::CIDEntry kStorageCIDs[] = {
    { &kMOZ_STORAGE_SERVICE_CID, false, NULL, mozilla::storage::ServiceConstructor },
    { &kVACUUMMANAGER_CID, false, NULL, mozilla::storage::VacuumManagerConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kStorageContracts[] = {
    { MOZ_STORAGE_SERVICE_CONTRACTID, &kMOZ_STORAGE_SERVICE_CID },
    { VACUUMMANAGER_CONTRACTID, &kVACUUMMANAGER_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kStorageCategories[] = {
    { "idle-daily", "MozStorage Vacuum Manager", VACUUMMANAGER_CONTRACTID },
    { NULL }
};

static const mozilla::Module kStorageModule = {
    mozilla::Module::kVersion,
    kStorageCIDs,
    kStorageContracts,
    kStorageCategories
};

NSMODULE_DEFN(mozStorageModule) = &kStorageModule;
