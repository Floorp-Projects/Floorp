// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wow64 implementation for native 64-bit Windows (in other words, never WOW).

#include "sandbox/win/src/wow64.h"

namespace sandbox {

Wow64::Wow64(TargetProcess* child, HMODULE ntdll)
    : child_(child), ntdll_(ntdll), dll_load_(NULL), continue_load_(NULL) {
}

Wow64::~Wow64() {
}

bool Wow64::WaitForNtdll() {
  return true;
}

}  // namespace sandbox
