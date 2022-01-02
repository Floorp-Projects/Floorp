/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "nscore.h"

extern "C" {

NS_EXPORT_(BOOL)
DllMain(HINSTANCE DllInstance, DWORD Reason, LPVOID Reserved) {
  UNREFERENCED_PARAMETER(DllInstance);
  UNREFERENCED_PARAMETER(Reason);
  UNREFERENCED_PARAMETER(Reserved);

  return TRUE;
}

} /* extern "C" */
