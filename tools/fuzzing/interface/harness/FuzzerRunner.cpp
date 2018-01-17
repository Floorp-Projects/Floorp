/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#include "FuzzerRunner.h"
#include "mozilla/Attributes.h"
#include "prenv.h"

#include "FuzzerTestHarness.h"

namespace mozilla {

// We use a static var 'fuzzerRunner' defined in nsAppRunner.cpp.
// fuzzerRunner is initialized to nullptr but if this file is linked in,
// then fuzzerRunner will be set here indicating that
// we want to call into either LibFuzzer's main or the AFL entrypoint.
class _InitFuzzer {
public:
  _InitFuzzer() {
    fuzzerRunner = new FuzzerRunner();
  }
} InitLibFuzzer;

int FuzzerRunner::Run(int* argc, char*** argv) {
  ScopedXPCOM xpcom("Fuzzer");
  const char* fuzzerEnv = getenv("FUZZER");

  if (!fuzzerEnv) {
    fuzzerEnv = getenv("LIBFUZZER");
    if (fuzzerEnv) {
      fprintf(stderr, "Fuzzer Interface: Warning: \
        Using deprecated LIBFUZZER variable, use FUZZER instead\n");
    } else {
      fprintf(stderr, "Must specify fuzzing target in FUZZER environment variable\n");
      return 1;
    }
  }

  std::string moduleNameStr(fuzzerEnv);
  FuzzerFunctions funcs = FuzzerRegistry::getInstance().getModuleFunctions(moduleNameStr);
  FuzzerInitFunc initFunc = funcs.first;
  FuzzerTestingFunc testingFunc = funcs.second;
  if (initFunc) {
    int ret = initFunc(argc, argv);
    if (ret) {
      fprintf(stderr, "Fuzzing Interface: Error: Initialize callback failed\n");
      return ret;
    }
  }

  if (!testingFunc) {
      fprintf(stderr, "Fuzzing Interface: Error: No testing callback found\n");
      return 1;
  }

#ifdef LIBFUZZER
  return mFuzzerDriver(argc, argv, testingFunc);
#else
  // For AFL, testingFunc points to the entry function we need.
  return testingFunc(NULL, 0);
#endif
}

#ifdef LIBFUZZER
void FuzzerRunner::setParams(LibFuzzerDriver aDriver) {
  mFuzzerDriver = aDriver;
}
#endif

} // namespace mozilla
