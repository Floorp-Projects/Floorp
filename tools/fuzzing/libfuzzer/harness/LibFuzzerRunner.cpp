/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int LibFuzzerRunner::Run() {
  ScopedXPCOM xpcom("LibFuzzer");
  return mFuzzerMain(mArgc, mArgv);
}

typedef int(*LibFuzzerMain)(int, char**);

void LibFuzzerRunner::setParams(int argc, char** argv, LibFuzzerMain main) {
  mArgc = argc;
  mArgv = argv;
  mFuzzerMain = main;
}

} // namespace mozilla
