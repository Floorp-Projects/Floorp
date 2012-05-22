/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

// A dummy vccorlib.dll for shunting winrt initialization calls when we are not
// using the winrt library. For a longer explanantion see nsDllMain.cpp.

extern "C" {
__declspec(dllexport) long __stdcall __InitializeWinRTRuntime(unsigned long data) { return S_OK; }
}

namespace Platform {
namespace Details {
__declspec(dllexport) HRESULT InitializeData(int __threading_model) { return S_OK; }
__declspec(dllexport) void UninitializeData(int __threading_model) { }
}
}
