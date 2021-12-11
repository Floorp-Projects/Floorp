/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

BOOL WINAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpvReserved) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    ::DisableThreadLibraryCalls((HMODULE)hModule);
  }
  return TRUE;
}
