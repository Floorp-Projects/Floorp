/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingInterface.h"
#include "RustFuzzingTargets.h"

int FuzzingInitDummy(int* argc, char*** argv) { return 0; }

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitDummy, fuzz_rkv, Rkv);
