// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

namespace sandbox {

BOOL WINAPI TargetGdiDllInitialize(
    GdiDllInitializeFunction orig_gdi_dll_initialize,
    HANDLE dll,
    DWORD reason) {
  return TRUE;
}

HGDIOBJ WINAPI TargetGetStockObject(
    GetStockObjectFunction orig_get_stock_object,
    int object) {
  return reinterpret_cast<HGDIOBJ>(NULL);
}

ATOM WINAPI TargetRegisterClassW(
    RegisterClassWFunction orig_register_class_function,
    const WNDCLASS* wnd_class) {
  return TRUE;
}

}  // namespace sandbox

