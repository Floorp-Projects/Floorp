/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"

#include "mozStorageService.h"
#include "mozStorageConnection.h"
#include "mozStorageStatementWrapper.h"
#include "VacuumManager.h"

#include "mozStorageCID.h"

namespace mozilla {
namespace storage {

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(Service,
                                         Service::getSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(StatementWrapper)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(VacuumManager,
                                         VacuumManager::getSingleton)

} // namespace storage
} // namespace mozilla

NS_DEFINE_NAMED_CID(MOZ_STORAGE_SERVICE_CID);
NS_DEFINE_NAMED_CID(MOZ_STORAGE_STATEMENT_WRAPPER_CID);
NS_DEFINE_NAMED_CID(VACUUMMANAGER_CID);

static const mozilla::Module::CIDEntry kStorageCIDs[] = {
    { &kMOZ_STORAGE_SERVICE_CID, false, NULL, mozilla::storage::ServiceConstructor },
    { &kMOZ_STORAGE_STATEMENT_WRAPPER_CID, false, NULL, mozilla::storage::StatementWrapperConstructor },
    { &kVACUUMMANAGER_CID, false, NULL, mozilla::storage::VacuumManagerConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kStorageContracts[] = {
    { MOZ_STORAGE_SERVICE_CONTRACTID, &kMOZ_STORAGE_SERVICE_CID },
    { MOZ_STORAGE_STATEMENT_WRAPPER_CONTRACTID, &kMOZ_STORAGE_STATEMENT_WRAPPER_CID },
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
