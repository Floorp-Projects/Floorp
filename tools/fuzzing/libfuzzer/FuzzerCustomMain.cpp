/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#include "FuzzerInterface.h"
#include "FuzzerInternal.h"
#include "harness/LibFuzzerRegistry.h"

/* This is a wrapper defined in browser/app/nsBrowserApp.cpp,
 * encapsulating the XRE_ equivalent defined in libxul */
extern void libFuzzerGetFuncs(const char*, LibFuzzerInitFunc*,
                                 LibFuzzerTestingFunc*);

int libfuzzer_main(int argc, char **argv) {
  LibFuzzerInitFunc initFunc = nullptr;
  LibFuzzerTestingFunc testingFunc = nullptr;

  libFuzzerGetFuncs(getenv("LIBFUZZER"), &initFunc, &testingFunc);

  if (initFunc) {
    int ret = initFunc(&argc, &argv);
    if (ret) {
      fprintf(stderr, "LibFuzzer: Error: Initialize callback failed\n");
      return ret;
    }
  }

  if (!testingFunc) {
      fprintf(stderr, "LibFuzzer: Error: No testing callback found\n");
      return 1;
  }

  return fuzzer::FuzzerDriver(&argc, &argv, testingFunc);
}
