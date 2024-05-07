/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Common code for the unified fuzzing interface
 */

#include <stdarg.h>
#include <stdlib.h>
#include "FuzzingInterface.h"

namespace mozilla {

#ifdef JS_STANDALONE
static bool fuzzing_verbose = !!getenv("MOZ_FUZZ_LOG");
void fuzzing_log(const char* aFmt, ...) {
  if (fuzzing_verbose) {
    va_list ap;
    va_start(ap, aFmt);
    vfprintf(stderr, aFmt, ap);
    va_end(ap);
  }
}
#else
LazyLogModule gFuzzingLog("nsFuzzing");
#endif

}  // namespace mozilla

#ifdef AFLFUZZ
__AFL_FUZZ_INIT();

int afl_interface_raw(FuzzingTestFuncRaw testFunc) {
  __AFL_INIT();
  char* testFilePtr = getenv("MOZ_FUZZ_TESTFILE");
  uint8_t* buf = NULL;

  if (testFilePtr) {
    std::string testFile(testFilePtr);
    while (__AFL_LOOP(1000)) {
      std::ifstream is;
      is.open(testFile, std::ios::binary);
      is.seekg(0, std::ios::end);
      size_t len = is.tellg();
      is.seekg(0, std::ios::beg);
      MOZ_RELEASE_ASSERT(len >= 0);
      if (!len) {
        is.close();
        continue;
      }
      buf = reinterpret_cast<uint8_t*>(realloc(buf, len));
      MOZ_RELEASE_ASSERT(buf);
      is.read(reinterpret_cast<char*>(buf), len);
      is.close();
      testFunc(buf, len);
    }
  } else {
    buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(1000)) {
      size_t len = __AFL_FUZZ_TESTCASE_LEN;
      testFunc(buf, len);
    }
  }

  return 0;
}
#endif  // AFLFUZZ
