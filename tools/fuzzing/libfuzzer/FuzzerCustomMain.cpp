/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#include "FuzzerInterface.h"
#include "FuzzerInternal.h"
#include "harness/LibFuzzerRegistry.h"

int libfuzzer_main(int argc, char **argv, LibFuzzerInitFunc initFunc,
                   LibFuzzerTestingFunc testingFunc) {
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
