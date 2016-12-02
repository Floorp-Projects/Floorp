/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LibFuzzerRegistry.h"

void XRE_LibFuzzerGetFuncs(const char* moduleName, LibFuzzerInitFunc* initFunc, LibFuzzerTestingFunc* testingFunc) {
    std::string moduleNameStr(moduleName);
    mozilla::LibFuzzerFunctions funcs = mozilla::LibFuzzerRegistry::getInstance().getModuleFunctions(moduleNameStr);
    *initFunc = funcs.first;
    *testingFunc = funcs.second;
}

namespace mozilla {

LibFuzzerRegistry& LibFuzzerRegistry::getInstance() {
    static LibFuzzerRegistry instance;
    return instance;
}

void LibFuzzerRegistry::registerModule(std::string moduleName, LibFuzzerInitFunc initFunc, LibFuzzerTestingFunc testingFunc) {
    moduleMap.insert(std::pair<std::string, LibFuzzerFunctions>(moduleName,LibFuzzerFunctions(initFunc, testingFunc)));
}

LibFuzzerFunctions LibFuzzerRegistry::getModuleFunctions(std::string& moduleName) {
    return moduleMap[moduleName];
}

} // namespace mozilla
