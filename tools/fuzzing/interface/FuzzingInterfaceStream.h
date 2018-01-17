/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface definitions for the unified fuzzing interface with streaming support
 */

#ifndef FuzzingInterfaceStream_h__
#define FuzzingInterfaceStream_h__

#ifdef JS_STANDALONE
#error "FuzzingInterfaceStream.h cannot be used in JS standalone builds."
#endif

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

#include "FuzzingInterface.h"

namespace mozilla {

typedef int(*FuzzingTestFuncStream)(nsCOMPtr<nsIInputStream>);

#ifdef __AFL_COMPILER
void afl_interface_stream(const char* testFile, FuzzingTestFuncStream testFunc);

#define MOZ_AFL_INTERFACE_COMMON(initFunc)                                                    \
  if (initFunc) initFunc(NULL, NULL);                                                         \
  char* testFilePtr = getenv("MOZ_FUZZ_TESTFILE");                                            \
  if (!testFilePtr) {                                                                         \
    fprintf(stderr, "Must specify testfile in MOZ_FUZZ_TESTFILE environment variable.\n");    \
    return;                                                                                   \
  }                                                                                           \
  /* Make a copy of testFilePtr so the testing function can safely call getenv */             \
  std::string testFile(testFilePtr);

#define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName) \
  TEST(AFL, moduleName) {                                        \
    MOZ_AFL_INTERFACE_COMMON(initFunc);                          \
    ::mozilla::afl_interface_stream(testFile.c_str(), testFunc); \
  }
#else
#define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName) /* Nothing */
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
    ::mozilla::FuzzerRegistry::getInstance().registerModule(                \
      #moduleName, initFunc, LibFuzzerTest##moduleName                      \
    );                                                                      \
  }
#else
#define MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName) /* Nothing */
#endif

#define MOZ_FUZZING_INTERFACE_STREAM(initFunc, testFunc, moduleName) \
  MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName);    \
  MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName);

} // namespace mozilla

#endif  // FuzzingInterfaceStream_h__
