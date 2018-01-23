/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LibFuzzerRegistry_h__
#define _LibFuzzerRegistry_h__

#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

typedef int(*LibFuzzerInitFunc)(int*, char***);
typedef int(*LibFuzzerTestingFunc)(const uint8_t*, size_t);
typedef int(*LibFuzzerDriver)(int*, char***, LibFuzzerTestingFunc);

namespace mozilla {

typedef std::pair<LibFuzzerInitFunc, LibFuzzerTestingFunc> LibFuzzerFunctions;

class LibFuzzerRegistry {
    public:
        MOZ_EXPORT static LibFuzzerRegistry& getInstance();
        MOZ_EXPORT void registerModule(std::string moduleName, LibFuzzerInitFunc initFunc, LibFuzzerTestingFunc testingFunc);
        MOZ_EXPORT LibFuzzerFunctions getModuleFunctions(std::string& moduleName);

        LibFuzzerRegistry(LibFuzzerRegistry const&) = delete;
        void operator=(LibFuzzerRegistry const&) = delete;

    private:
        LibFuzzerRegistry() {};
        std::map<std::string, LibFuzzerFunctions> moduleMap;
};

} // namespace mozilla


#endif // _LibFuzzerRegistry_h__
