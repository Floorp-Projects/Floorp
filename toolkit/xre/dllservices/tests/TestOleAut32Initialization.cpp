/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsOleAut32Initialization.h"

#include <windows.h>
#include <oleauto.h>

#pragma comment(lib, "oleaut32")

#include <cstdio>

// This test fails by crashing the process with a heap corruption, which occurs
// if VariantClear has its default behavior but should not occur after a call
// to WindowsOleAut32Initialization().
void TestNoCrash() {
  // This string is intentionally very long to ensure that a double free of a
  // BSTR with this value will be detected by heap verification. This is
  // required because OLEAUT32 caches shorter string in the APP_DATA cache.
  //
  // For more details, see "OLEAUT32 memory allocator" in:
  //   http://www.phreedom.org/research/heap-feng-shui/heap-feng-shui.html
  static wchar_t sLongString[32768 / 2 + 1];
  static_assert(
      sizeof sLongString > 32768,
      "sLongString might not be long enough to bypass the APP_DATA cache");

  for (size_t i = 0; i < (sizeof sLongString / 2) - 1; ++i) {
    sLongString[i] = L'A';
  }
  sLongString[(sizeof sLongString / 2) - 1] = 0;

  VARIANTARG var;
  VariantInit(&var);
  var.vt = VT_BSTR;
  var.bstrVal = SysAllocString(sLongString);
  VariantClear(&var);
  var.vt = VT_BSTR;
  VariantClear(&var);

  wprintf(
      L"TEST-PASS | TestOleAut32Initialization | Did not crash after double "
      L"free pattern\n");
}

int wmain(int argc, wchar_t* argv[]) {
  if (!::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                            0)) {
    wprintf(
        L"TEST-FAILED | TestOleAut32Initialization | HeapSetInformation "
        L"failed to HeapEnableTerminationOnCorruption\n");
    return 1;
  }

  if (!mozilla::WindowsOleAut32Initialization()) {
    wprintf(
        L"TEST-FAILED | TestOleAut32Initialization | "
        L"WindowsOleAut32Initialization failed\n");
    return 1;
  }

  TestNoCrash();

  return 0;
}
