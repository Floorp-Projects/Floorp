/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface definitions for the unified fuzzing interface with streaming
 * support
 */

#ifndef FuzzingInterfaceStream_h__
#define FuzzingInterfaceStream_h__

#ifdef JS_STANDALONE
#  error "FuzzingInterfaceStream.h cannot be used in JS standalone builds."
#endif

#include "gtest/gtest.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

#include "nsDirectoryServiceDefs.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include <fstream>

#include "FuzzingInterface.h"

typedef int (*FuzzingTestFuncStream)(nsCOMPtr<nsIInputStream>);

#ifdef AFLFUZZ
#  define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName)             \
    static int afl_fuzz_inner_##moduleName(const uint8_t* data, size_t size) { \
      if (size > INT32_MAX) return 0;                                          \
      nsCOMPtr<nsIInputStream> stream;                                         \
      nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),              \
                                          Span((const char*)data, size),       \
                                          NS_ASSIGNMENT_DEPEND);               \
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));                                    \
      return testFunc(stream.forget());                                        \
    }                                                                          \
    static int afl_fuzz_##moduleName(const uint8_t* data, size_t size) {       \
      return afl_interface_raw(afl_fuzz_inner_##moduleName);                   \
    }                                                                          \
    static void __attribute__((constructor)) AFLRegister##moduleName() {       \
      ::mozilla::FuzzerRegistry::getInstance().registerModule(                 \
          #moduleName, initFunc, afl_fuzz_##moduleName);                       \
    }
#else
#  define MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName) /* Nothing \
                                                                    */
#endif

#ifdef LIBFUZZER
#  define MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName)       \
    static int LibFuzzerTest##moduleName(const uint8_t* data, size_t size) {   \
      if (size > INT32_MAX) return 0;                                          \
      nsCOMPtr<nsIInputStream> stream;                                         \
      nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),              \
                                          Span((const char*)data, size),       \
                                          NS_ASSIGNMENT_DEPEND);               \
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));                                    \
      testFunc(stream.forget());                                               \
      return 0;                                                                \
    }                                                                          \
    static void __attribute__((constructor)) LibFuzzerRegister##moduleName() { \
      ::mozilla::FuzzerRegistry::getInstance().registerModule(                 \
          #moduleName, initFunc, LibFuzzerTest##moduleName);                   \
    }
#else
#  define MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, \
                                         moduleName) /* Nothing */
#endif

#define MOZ_FUZZING_INTERFACE_STREAM(initFunc, testFunc, moduleName) \
  MOZ_LIBFUZZER_INTERFACE_STREAM(initFunc, testFunc, moduleName);    \
  MOZ_AFL_INTERFACE_STREAM(initFunc, testFunc, moduleName);

#endif  // FuzzingInterfaceStream_h__
