/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interface definitions for the unified fuzzing interface
 */

#ifndef FuzzingInterface_h__
#define FuzzingInterface_h__

#include <fstream>

#ifdef LIBFUZZER
#  include "FuzzerExtFunctions.h"
#endif

#include "FuzzerRegistry.h"
#include "mozilla/Assertions.h"

#ifndef JS_STANDALONE
#  include "mozilla/Logging.h"
#endif

namespace mozilla {

#ifdef JS_STANDALONE
void fuzzing_log(const char* aFmt, ...);
#  define MOZ_LOG_EXPAND_ARGS(...) __VA_ARGS__

#  define FUZZING_LOG(args) fuzzing_log(MOZ_LOG_EXPAND_ARGS args);
#else
extern LazyLogModule gFuzzingLog;

#  define FUZZING_LOG(args) \
    MOZ_LOG(mozilla::gFuzzingLog, mozilla::LogLevel::Verbose, args)
#endif  // JS_STANDALONE

}  // namespace mozilla

typedef int (*FuzzingTestFuncRaw)(const uint8_t*, size_t);

#ifdef AFLFUZZ

int afl_interface_raw(FuzzingTestFuncRaw testFunc);

#  define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName)          \
    static int afl_fuzz_##moduleName(const uint8_t* data, size_t size) { \
      return afl_interface_raw(testFunc);                                \
    }                                                                    \
    static void __attribute__((constructor)) AFLRegister##moduleName() { \
      ::mozilla::FuzzerRegistry::getInstance().registerModule(           \
          #moduleName, initFunc, afl_fuzz_##moduleName);                 \
    }
#else
#  define MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName) /* Nothing */
#endif                                                          // AFLFUZZ

#ifdef LIBFUZZER
#  define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName)          \
    static void __attribute__((constructor)) LibFuzzerRegister##moduleName() { \
      ::mozilla::FuzzerRegistry::getInstance().registerModule(                 \
          #moduleName, initFunc, testFunc);                                    \
    }
#else
#  define MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, \
                                      moduleName) /* Nothing */
#endif

#define MOZ_FUZZING_INTERFACE_RAW(initFunc, testFunc, moduleName) \
  MOZ_LIBFUZZER_INTERFACE_RAW(initFunc, testFunc, moduleName);    \
  MOZ_AFL_INTERFACE_RAW(initFunc, testFunc, moduleName);

#endif  // FuzzingInterface_h__
