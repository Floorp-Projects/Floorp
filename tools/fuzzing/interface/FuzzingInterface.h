/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface definitions for the unified fuzzing interface
 */

#ifndef FuzzingInterface_h__
#define FuzzingInterface_h__

#include "gtest/gtest.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include <fstream>

#ifdef LIBFUZZER
#include "LibFuzzerRegistry.h"
#endif

namespace mozilla {

typedef int(*FuzzingTestFuncRaw)(const uint8_t*, size_t);
typedef int(*FuzzingTestFuncStream)(nsCOMPtr<nsIInputStream>);

#ifdef __AFL_COMPILER
void afl_interface_stream(const char* testFile, FuzzingTestFuncStream testFunc);
void afl_interface_raw(const char* testFile, FuzzingTestFuncRaw testFunc);

#define MOZ_AFL_INTERFACE_COMMON(initFunc)                                                    \
  initFunc(NULL, NULL);                                                                       \
  char* testFilePtr = getenv("MOZ_FUZZ_TESTFILE");                                            \
  if (!testFilePtr) {                                                                         \
    EXPECT_TRUE(false) << "Must specify testfile in MOZ_FUZZ_TESTFILE environment variable."; \
    return;                                                                                   \
  }                                                                                           \
  /* Make a copy of testFilePtr so the testing function can safely call getenv */             \
  std::string testFile(testFilePtr);

#define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName) \
  TEST(AFL, moduleName) {                                        \
    MOZ_AFL_INTERFACE_COMMON(initFunc);                          \
    ::mozilla::afl_interface_stream(testFile.c_str(), testFunc); \
  }

#define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName)    \
  TEST(AFL, moduleName) {                                        \
    MOZ_AFL_INTERFACE_COMMON(initFunc);                          \
    ::mozilla::afl_interface_raw(testFile.c_str(), testFunc);    \
  }
#else
#define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName) /* Nothing */
#define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName)    /* Nothing */
#endif

#ifdef LIBFUZZER
#define MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName)      \
  static int LibFuzzerTest##moduleName (const uint8_t *data, size_t size) { \
    if (size > INT32_MAX)                                                   \
      return 0;                                                             \
    nsCOMPtr<nsIInputStream> stream;                                        \
    nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),             \
      (const char*)data, size, NS_ASSIGNMENT_DEPEND);                       \
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));                                   \
    testFunc(stream.forget());                                              \
    return 0;                                                               \
  }                                                                         \
  static void __attribute__ ((constructor)) LibFuzzerRegister() {           \
    ::mozilla::LibFuzzerRegistry::getInstance().registerModule(             \
      #moduleName, initFunc, LibFuzzerTest##moduleName                      \
    );                                                                      \
  }

#define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName)         \
  static void __attribute__ ((constructor)) LibFuzzerRegister() {           \
    ::mozilla::LibFuzzerRegistry::getInstance().registerModule(             \
      #moduleName, initFunc, testFunc                                       \
    );                                                                      \
  }
#else
#define MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName) /* Nothing */
#define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName)    /* Nothing */
#endif

#define MOZ_FUZZING_INTERFACE_STREAM(initFunc, testFunc, moduleName) \
  MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName);    \
  MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName);

#define MOZ_FUZZING_INTERFACE_RAW(initFunc, testFunc, moduleName)    \
  MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName);       \
  MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName);

} // namespace mozilla

#endif  // FuzzingInterface_h__
