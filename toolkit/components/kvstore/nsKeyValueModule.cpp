/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIKeyValue.h"
#include "nsToolkitCompsCID.h"

extern "C" {
// Implemented in Rust.
nsresult nsKeyValueServiceConstructor(nsISupports* aOuter, REFNSIID aIID,
                                      void** aResult);
}  // extern "C"

NS_DEFINE_NAMED_CID(NS_KEY_VALUE_SERVICE_CID);

const mozilla::Module::CIDEntry kKeyValueCIDs[] = {
    {&kNS_KEY_VALUE_SERVICE_CID, false, nullptr, nsKeyValueServiceConstructor},
    {nullptr}};

const mozilla::Module::ContractIDEntry kKeyValueContracts[] = {
    {NS_KEY_VALUE_SERVICE_CONTRACTID, &kNS_KEY_VALUE_SERVICE_CID}, {nullptr}};

extern const mozilla::Module kKeyValueModule = {
    mozilla::Module::kVersion, kKeyValueCIDs, kKeyValueContracts, nullptr};
