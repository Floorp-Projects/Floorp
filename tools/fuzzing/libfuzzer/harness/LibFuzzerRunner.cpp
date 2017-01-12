/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#include "LibFuzzerRunner.h"
#include "mozilla/Attributes.h"
#include "prenv.h"

#include "LibFuzzerTestHarness.h"

namespace mozilla {

// We use a static var 'libFuzzerRunner' defined in nsAppRunner.cpp.
// libFuzzerRunner is initialized to nullptr but if LibFuzzer (this file)
// is linked in then libFuzzerRunner will be set here indicating that
// we want to call into LibFuzzer's main.
class _InitLibFuzzer {
public:
  _InitLibFuzzer() {
    libFuzzerRunner = new LibFuzzerRunner();
  }
} InitLibFuzzer;

int LibFuzzerRunner::Run(int* argc, char*** argv) {
  ScopedXPCOM xpcom("LibFuzzer");
  std::string moduleNameStr(getenv("LIBFUZZER"));
  LibFuzzerFunctions funcs = LibFuzzerRegistry::getInstance().getModuleFunctions(moduleNameStr);
  LibFuzzerInitFunc initFunc = funcs.first;
  LibFuzzerTestingFunc testingFunc = funcs.second;
  if (initFunc) {
    int ret = initFunc(argc, argv);
    if (ret) {
      fprintf(stderr, "LibFuzzer: Error: Initialize callback failed\n");
      return ret;
    }
  }

  if (!testingFunc) {
      fprintf(stderr, "LibFuzzer: Error: No testing callback found\n");
      return 1;
  }

  return mFuzzerDriver(argc, argv, testingFunc);
}

void LibFuzzerRunner::setParams(LibFuzzerDriver aDriver) {
  mFuzzerDriver = aDriver;
}

} // namespace mozilla
