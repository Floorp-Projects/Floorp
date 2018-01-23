/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface definitions for the unified fuzzing interface
 */

#ifndef FuzzingInterface_h__
#define FuzzingInterface_h__

#include <fstream>

#include "FuzzerRegistry.h"
#include "mozilla/Assertions.h"

namespace mozilla {

typedef int(*FuzzingTestFuncRaw)(const uint8_t*, size_t);

#ifdef __AFL_COMPILER

static int afl_interface_raw(const char* testFile, FuzzingTestFuncRaw testFunc) {
    char* buf = NULL;

    while(__AFL_LOOP(1000)) {
      std::ifstream is;
      is.open (testFile, std::ios::binary);
      is.seekg (0, std::ios::end);
      int len = is.tellg();
      is.seekg (0, std::ios::beg);
      MOZ_RELEASE_ASSERT(len >= 0);
      if (!len) {
        is.close();
        continue;
      }
      buf = (char*)realloc(buf, len);
      MOZ_RELEASE_ASSERT(buf);
      is.read(buf,len);
      is.close();
      testFunc((uint8_t*)buf, (size_t)len);
    }

    free(buf);

    return 0;
}

#define MOZ_AFL_INTERFACE_COMMON()                                                            \
  char* testFilePtr = getenv("MOZ_FUZZ_TESTFILE");                                            \
  if (!testFilePtr) {                                                                         \
    fprintf(stderr, "Must specify testfile in MOZ_FUZZ_TESTFILE environment variable.\n");    \
    return 1;                                                                                 \
  }                                                                                           \
  /* Make a copy of testFilePtr so the testing function can safely call getenv */             \
  std::string testFile(testFilePtr);

#define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName)            \
  static int afl_fuzz_##moduleName(const uint8_t *data, size_t size) {   \
    MOZ_RELEASE_ASSERT(data == NULL && size == 0);                       \
    MOZ_AFL_INTERFACE_COMMON();                                          \
    return ::mozilla::afl_interface_raw(testFile.c_str(), testFunc);     \
  }                                                                      \
  static void __attribute__ ((constructor)) AFLRegister##moduleName() {  \
    ::mozilla::FuzzerRegistry::getInstance().registerModule(             \
      #moduleName, initFunc, afl_fuzz_##moduleName                       \
    );                                                                   \
  }
#else
#define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName)    /* Nothing */
#endif // __AFL_COMPILER

#ifdef LIBFUZZER
#define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName)            \
  static void __attribute__ ((constructor)) LibFuzzerRegister##moduleName() {  \
    ::mozilla::FuzzerRegistry::getInstance().registerModule(                   \
      #moduleName, initFunc, testFunc                                          \
    );                                                                         \
  }
#else
#define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName)    /* Nothing */
#endif

#define MOZ_FUZZING_INTERFACE_RAW(initFunc, testFunc, moduleName)    \
  MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName);       \
  MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName);

} // namespace mozilla

#endif  // FuzzingInterface_h__
