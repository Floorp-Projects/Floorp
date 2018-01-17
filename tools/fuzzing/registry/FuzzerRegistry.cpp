/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzerRegistry.h"

namespace mozilla {

FuzzerRegistry& FuzzerRegistry::getInstance() {
    static FuzzerRegistry instance;
    return instance;
}

void FuzzerRegistry::registerModule(std::string moduleName, FuzzerInitFunc initFunc, FuzzerTestingFunc testingFunc) {
    moduleMap.insert(std::pair<std::string, FuzzerFunctions>(moduleName,FuzzerFunctions(initFunc, testingFunc)));
}

FuzzerFunctions FuzzerRegistry::getModuleFunctions(std::string& moduleName) {
    return moduleMap[moduleName];
}

} // namespace mozilla
