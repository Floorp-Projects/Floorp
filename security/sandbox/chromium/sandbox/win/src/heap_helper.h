// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/win/windows_version.h"

namespace sandbox {
// These helper functions are not expected to be used generally, but are exposed
// only to allow direct testing of this functionality.

// Return the flags for this heap handle. Limited verification of the handle is
// performed. No verification of the flags is performed.
bool HeapFlags(HANDLE handle, DWORD* flags);

// Return the handle to the CSR Port Heap, return nullptr if none or more than
// one candidate was found.
HANDLE FindCsrPortHeap();

}  // namespace sandbox
